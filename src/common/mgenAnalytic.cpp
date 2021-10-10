
#include "mgenAnalytic.h"
#include "mgen.h"  // for logging
#include <string.h>  // for memcpy()

const double MgenAnalytic::DEFAULT_WINDOW = 1.0;

MgenAnalytic::MgenAnalytic()
 : flow_key(NULL), flow_keysize(0), window_size(DEFAULT_WINDOW), window_valid(false), 
   msg_count(0), byte_count(0), dup_msg_count(0), latency_sum(0.0), 
   report_valid(false), report_msg_count(0)
{ 
    // Adjust set window_size to quantized version
    UINT8 q = Report::QuantizeTimeValue(window_size);
    window_size = Report::UnquantizeTimeValue(q);
}

MgenAnalytic::~MgenAnalytic()
{
    if (NULL != flow_key)
    {
        delete[] flow_key;
        flow_key = NULL;
    }
    dup_mask.Destroy();
}

bool MgenAnalytic::Init(Protocol               protocol,
                        const ProtoAddress&    srcAddr,
                        const ProtoAddress&    dstAddr,
                        UINT32                 flowId, 
                        double                 windowSize,
                        UINT32                 historyDepth)
{
    if (!dup_mask.Init(historyDepth, 0xffffffff))
    {
        PLOG(PL_ERROR, "MgenAnalytic::Init() dup_mask.Init() error: %s\n", GetErrorString());
        return false;
    }
    if (NULL != flow_key) delete[] flow_key;
    unsigned int dstLen = dstAddr.GetLength();
    unsigned int srcLen = srcAddr.GetLength();
    if (0 == (flow_key = new char[dstLen + 2 + srcLen + 2 + 4]))
    {
        PLOG(PL_ERROR, "MgenAnalytic::Init() new flow_key[] error: %s\n", GetErrorString());
        dup_mask.Destroy();
        return false;
    }
    UINT8 q = Report::QuantizeTimeValue(windowSize);
    window_size = MgenAnalytic::Report::UnquantizeTimeValue(q);
    memcpy(flow_key, dstAddr.GetRawHostAddress(), dstLen);
    unsigned int keyLen = dstLen;
    UINT16 port = dstAddr.GetPort();
    memcpy(flow_key+keyLen, &port, 2);
    keyLen += 2;
    memcpy(flow_key+keyLen, srcAddr.GetRawHostAddress(), srcLen);
    keyLen += srcLen;
    port = srcAddr.GetPort();
    memcpy(flow_key+keyLen, &port, 2);
    keyLen += 2;
    memcpy(flow_key+keyLen, &flowId, 4);
    keyLen += 4;
    flow_keysize = keyLen << 3;
    report_msg.InitIntoBuffer(Report::REPORT_FLOW_IPv4, report_buffer, Report::MAX_LENGTH);
    report_msg.SetProtocol(protocol);
    report_msg.SetDstAddr(dstAddr);
    report_msg.SetSrcAddr(srcAddr);
    if (1 != flowId) report_msg.SetFlowId(flowId);
    report_msg.SetWindowSize(window_size);  // this must come after
    return true;
}  // end MgenAnalytic::Init()


bool MgenAnalytic::Update(const ProtoTime& rxTime,
                          unsigned int     msgSize,
                          const ProtoTime& txTime,
                          UINT32           seqNum)
{
   if (!window_valid)
   {
        // First event for this flow, so initialize the analytic
        window_valid = true;
        window_start = rxTime;
        window_end = window_start;
        window_end += window_size;
        if (0 != msgSize)
        {
            dup_mask.Set(seqNum);
            seq_start = seqNum;
            msg_count = 1;
            byte_count = msgSize;
            latency_sum = latency_min = latency_max = ProtoTime::Delta(rxTime, txTime);
        }
        else
        {
            msg_count = byte_count = 0;
            latency_sum = latency_min = latency_max = 0.0;
        }
        window_valid = true;
        return false;  
    }
    double latency = 0.0;
    if (0 != msgSize)
    {
        if (dup_mask.IsSet())
        {
        
            // IMPORTANT TBD: manage the duplicate detector more intelligently (or limit its range, perhaps temporally)
            if (dup_mask.Test(seqNum))
            {
                // It's a duplicate 
                dup_msg_count++;
                // TBD - keep track of dup_byte_count to report overall receive rate?
            }
            else if (window_valid && (dup_mask.Difference(seqNum, seq_start) < 0))
            {
                // It precedes our window, but mark dup_mask
                // for duplicate message count purposes
                dup_mask.Set(seqNum);  // doesn't matter if it succeeds
            }
            else
            {
                // It's a valid, new message, so count it
                if (!dup_mask.Set(seqNum))
                {
                    // Need to force our dup_mask forward to 
                    // (TBD - add a param to ProtoSlidingMask::Set() to do this automatically)
                    UINT32 firstSet;
                    dup_mask.GetFirstSet(firstSet);
                    UINT32 numBits = dup_mask.Difference(seqNum, firstSet);
                    dup_mask.UnsetBits(firstSet, numBits);
                    dup_mask.Set(seqNum);
                }
                // Ignore size of first message (serves as time reference only) 
                // when more than one message in window
                if (1 == msg_count)
                    byte_count = msgSize;  
                else
                    byte_count += msgSize;
                latency = ProtoTime::Delta(rxTime, txTime);
                if (0 == msg_count)
                {
                    latency_sum = latency_min = latency_max = latency;
                }
                else
                {
                    latency_sum += latency;
                    if (latency < latency_min)
                        latency_min = latency;
                    else if (latency > latency_max)
                        latency_max = latency;
                }
                msg_count++;
            }
        }
        else
        {
            // First actual message received for this analytic
            if (dup_mask.IsSet()) dup_mask.Clear(); // TBD - Is this right thing to do???
            dup_mask.Set(seqNum);
            seq_start = seqNum;
            byte_count = msgSize;
            latency_sum = latency_min = latency_max = ProtoTime::Delta(rxTime, txTime);
            msg_count = 1;
        }
    }
    
    if (rxTime >= window_end)
    {
        // Update report (also build REPORT msg into buffer)
        report_valid = true;
        report_start = window_start;
        report_duration = ProtoTime::Delta(rxTime, window_start);
        UINT32 seqMax;
        if (!dup_mask.GetLastSet(seqMax))  // gets highest sequence number observed
            seqMax = seq_start;
        
        switch (msg_count)
        {
            // Note case 0 and case 1 here will only come into play when window timeouts are implemented
            // At the moment, the MgenAnalytic is completely event-driven by received messages for the flow
            case 0:
            {
                report_msg_count = 0;
                report_rate_ave = 0.0;
                report_loss_ave = 1.0;  // assume 100% loss
                report_latency_ave = report_latency_min = report_latency_max = -1.0;
                break;
            }
            case 1:
            {
                // one-shot report (only a single message for window)
                report_msg_count = 1;
                report_rate_ave = (double)byte_count / report_duration;
                report_loss_ave = 0.0;
                report_latency_ave = latency_sum;
                report_latency_min = latency_min;
                report_latency_max = latency_max;
                break;
            }
            default:
            {
                report_msg_count = msg_count - 1;  // don't include the first packet in the count as it was included in the previous window
                report_rate_ave = (double)byte_count / report_duration;
                UINT32 seqDelta = seqMax - seq_start;
                // If "seqDelta" is zero, this means _only_ a single message
                // (or duplicate messages) has been received, so
                // report "one-shot" goodput, latency, and zero loss
                // TBD - provide option for reporting duplicate rx rate
                switch (seqDelta)
                {
                    case 0:
                    case 1:
                        report_loss_ave = 0.0;
                        break;
                    default:
                        report_loss_ave = 1.0 - (double)msg_count / (double)(seqDelta + 1);
                        break;   
                }
                report_latency_ave = latency_sum / (double)msg_count;
                report_latency_min = latency_min;
                report_latency_max = latency_max;
                break;
            }
        }
        
        // Reset window indices and counts
        window_start = rxTime;
        window_end = rxTime;
        window_end += window_size;
        seq_start = seqMax;
        if (0 != msgSize)
        {
            byte_count = 0;  // the bytes for the message already accounted (ignored anyway)
            msg_count = 1;
            latency_sum = latency_min = latency_max = latency;
        }
        else
        {
            byte_count = msg_count = 0;
            latency_sum = latency_min = latency_max = 0.0;
        }
        
        // Update our "report_buffer" metrics
        // TBD - make this conditional upon an initialization variable?
        // windowOffset not set until report is sent
        report_msg.SetWindowSize(report_duration);
        report_msg.SetLatencyAve(report_latency_ave);
        // Min/Max latencies reported as relative offsets from average latency
        report_msg.SetLatencyDeltaMin(report_latency_ave - report_latency_min);
        report_msg.SetLatencyDeltaMax(report_latency_max - report_latency_ave);
        report_msg.SetRateAve(report_rate_ave);
        report_msg.SetLossFraction(report_loss_ave);
        return true;  // report updated
    }
    
    return false;  // report not updated
}  // end MgenAnalytic::Update()

void MgenAnalytic::Log(FILE*            filePtr, 
                       const ProtoTime& sentTime, 
                       const ProtoTime& theTime, 
                       bool             localTime) const
{
    if (NULL == filePtr) return;  // not logging
    // MGEN logging timestamp format (TBD - create an Mgen::LogTimeStamp() method
    Mgen::LogTimestamp(filePtr, theTime.GetTimeVal(), localTime);
    UINT32 flowId = report_msg.GetFlowId();
    if (0 == flowId) flowId = 1;  // null flowId means flow 1 by default
    ProtoAddress srcAddr;
    report_msg.GetSrcAddr(srcAddr);
    const char* protocol = "???";
    switch(report_msg.GetProtocol())
    {
        case UDP:
            protocol = "UDP";
            break;
        case TCP:
            protocol = "TCP";
            break;
        case SINK:
            protocol = "SINK";
            break;
        default:
            protocol = "???";
            break;
    }
    Mgen::Log(filePtr, "REPORT proto>%s flow>%lu src>%s/%hu ", protocol, flowId, srcAddr.GetHostString(), srcAddr.GetPort());
    ProtoAddress dstAddr;
    report_msg.GetDstAddr(dstAddr);
    Mgen::Log(filePtr, "dst>%s/%hu ", dstAddr.GetHostString(), dstAddr.GetPort());
    Mgen::Log(filePtr,"window>%lf rate>%lf kbps loss>%lf latency ave>%lf min>%lf max>%lf, count>%u\n",
                report_duration, report_rate_ave*8.0e-03, report_loss_ave, 
                report_latency_ave, report_latency_min, report_latency_max, report_msg_count);
}  // end void MgenAnalytic:::Log()

const MgenAnalytic::Report& MgenAnalytic::GetReport(const ProtoTime& theTime)
{
    // Update report message window start offset given "theTime"
    // We use window_end since it will yield a smaller offset value (more accurate)
    double windowOffset = (theTime - report_start) - report_duration;
    if (windowOffset < 0.0)
    {
        PLOG(PL_WARN, "MgenAnalytic::GetReport() warning: negative window offset: %lf?!\n", windowOffset);
        windowOffset = 0.0;
    }
    report_msg.SetWindowOffset(windowOffset);
    report_time = theTime;
    return report_msg;
}  // end MgenAnalytic::GetReport()

MgenAnalytic* MgenAnalyticTable::FindFlow(const ProtoAddress& srcAddr, const ProtoAddress& dstAddr, UINT32 flowId)
{
    char flowKey[MgenAnalytic::KEY_MAX];
    memcpy(flowKey, dstAddr.GetRawHostAddress(), dstAddr.GetLength());
    unsigned int keyLen = dstAddr.GetLength();
    UINT16 port = dstAddr.GetPort();
    memcpy(flowKey+keyLen, &port, 2);
    keyLen += 2;
    memcpy(flowKey+keyLen, srcAddr.GetRawHostAddress(), srcAddr.GetLength());
    keyLen += srcAddr.GetLength();
    port = srcAddr.GetPort();
    memcpy(flowKey+keyLen, &port, 2);
    keyLen += 2;
    memcpy(flowKey+keyLen, &flowId, 4);
    keyLen += 4;
    return Find(flowKey, keyLen << 3);
}  // end MgenAnalyticTable::FindFlow();
        

MgenAnalytic::Report::Report(UINT32*        bufferPtr, 
                             unsigned int   bufferBytes, 
                             bool           freeOnDestruct)
 : MgenDataItem(bufferPtr, bufferBytes, freeOnDestruct)
{
    InitFromBuffer();
}

MgenAnalytic::Report::~Report()
{
}

bool MgenAnalytic::Report::InitFromBuffer(UINT32*         bufferPtr, 
                                          unsigned int    numBytes, 
                                          bool            freeOnDestruct)
{
    if (MgenDataItem::InitFromBuffer(bufferPtr, numBytes, freeOnDestruct) &&
        (GetBufferLength() >= OFFSET_FLAGS))
    {
        switch (GetReportType())
        {
            case REPORT_FLOW_IPv6:
            case REPORT_FLOW_IPv4:
            {
                unsigned int reportLength = OffsetLossFraction()+ 2;
                if (GetReportLength() == reportLength) 
                {
                    return true;
                }
                else
                {
                    PLOG(PL_ERROR, "MgenAnalytic::Report::InitFromBuffer() error: invalid report length?!\n");
                    SetType(DATA_ITEM_INVALID);
                }
                break;
            }
            default:
                PLOG(PL_ERROR, "MgenAnalytic::Report::InitFromBuffer() error: invalid report type\n");
                break;
        }
    }
    if (NULL != bufferPtr) DetachBuffer();
    return false;
}  // end MgenAnalytic::Report::InitFromBuffer()

unsigned int MgenAnalytic::Report::GetAddrLen() const
{
    switch (GetReportType())
    {
        case REPORT_FLOW_IPv4:
            return 4;
        case REPORT_FLOW_IPv6:
            return 16;
        default:
            PLOG(PL_ERROR, " MgenAnalytic::Report::GetAddrLen() error: invalid report type!\n");
            break;
    }
    return 0;
}  // end  MgenAnalytic::Report::GetAddrlen()

bool MgenAnalytic::Report::GetDstAddr(ProtoAddress& addr) const
{
    ProtoAddress::Type addrType;
    unsigned int addrLen;
    switch (GetAddrLen())
    {
        case 4:
            addrType = ProtoAddress::IPv4;
            addrLen = 4;
            break;
        case 16:
            addrType = ProtoAddress::IPv6;
            addrLen = 16;
            break;
        case 6:
            addrType = ProtoAddress::ETH;
            addrLen = 6;
            break;
        default:
            PLOG(PL_ERROR, "ElasticAck::GetDstAddr() error: invalid address type!\n");
            return false;
    }
    addr.SetRawHostAddress(addrType, (char*)GetBuffer32(OFFSET_DST), addrLen);
    addr.SetPort(GetUINT16(OffsetDstPort()));
    return true;
}  // end MgenAnalytic::Report::GetDstAddr()

bool MgenAnalytic::Report::GetSrcAddr(ProtoAddress& addr) const
{
    ProtoAddress::Type addrType;
    unsigned int addrLen;
    switch (GetAddrLen())
    {
        case 4:
            addrType = ProtoAddress::IPv4;
            addrLen = 4;
            break;
        case 16:
            addrType = ProtoAddress::IPv6;
            addrLen = 16;
            break;
        case 6:
            addrType = ProtoAddress::ETH;
            addrLen = 6;
            break;
        default:
            PLOG(PL_ERROR, "MgenAnalytic::Report::GetSrcAddr() error: invalid address type!\n");
            return false;
    }
    addr.SetRawHostAddress(addrType, (char*)GetBuffer32(OffsetSrc()), addrLen);
    addr.SetPort(GetUINT16(OffsetSrcPort()));
    return true;
}  // end MgenAnalytic::Report::GetSrcAddr()


bool MgenAnalytic::Report::InitIntoBuffer(ReportType    reportType,
                                          UINT32*       bufferPtr, 
                                          unsigned int  bufferBytes, 
                                          bool          freeOnDestruct)
{
    unsigned int minLength;
    switch (reportType)
    {
        case REPORT_FLOW_IPv4:
        case REPORT_FLOW_IPv6:
            minLength = 12 + OFFSET_DST*4;  // convert UINT32 offset to bytes   
            break;
        default:
            PLOG(PL_ERROR, "MgenAnalytic::Report::InitIntoBuffer() error: invalid report type\n");
            return false;
    }
    if (NULL != bufferPtr)
    {
        if (bufferBytes < minLength)
            return false;
        else
            AttachBuffer(bufferPtr, bufferBytes, freeOnDestruct);
    }
    else if (GetBufferLength() < minLength) 
    {
        return false;
    }
    memset(AccessBuffer(), 0, minLength);
    SetReportType(reportType);
    SetReportLength(minLength);  // will be updated as other fields are set
    SetLength(minLength);
    return true;
}  // end MgenAnalytic::Report::InitIntoBuffer()

bool MgenAnalytic::Report::SetDstAddr(const ProtoAddress& addr)
{
    // TBD - do some consistency checks in case source addr was set first?
    ReportType reportType;
    unsigned int addrLen;
    switch (addr.GetType())
    {
        case ProtoAddress::IPv4:
            reportType = REPORT_FLOW_IPv4;
            addrLen = 4;
            break;
        case ProtoAddress::IPv6:
            reportType = REPORT_FLOW_IPv6;
            addrLen = 16;
            break;
        default:
            PLOG(PL_ERROR, "MgenAnalytic::Report::SetDstAddr() error: invalid address type!\n");
            return false;
    }
    unsigned int reportLength = 12 + OFFSET_DST*4 + 2*addrLen;
    if (reportLength > GetBufferLength())
    {
        PLOG(PL_ERROR, "MgenAnalytic::Report::SetDstAddr() error: insufficient buffer length\n");
        return false;
    }
    SetReportType(reportType);
    // Update report length field
    SetReportLength(reportLength);
    // set dstAddr field
    memcpy((char*)AccessBuffer32(OFFSET_DST), addr.GetRawHostAddress(), addrLen);
    // Set dstPort field
    SetUINT16(OffsetDstPort(), addr.GetPort());
    SetLength(reportLength);  // update ProtoPkt length
    return true;
}  // end MgenAnalytic::Report::SetDstAddr()

bool MgenAnalytic::Report::SetSrcAddr(const ProtoAddress& addr)
{
    // TBD - do some consistency checks in case source addr was set first?
    ReportType reportType;
    unsigned int addrLen;
    switch (addr.GetType())
    {
        case ProtoAddress::IPv4:
            reportType = REPORT_FLOW_IPv4;
            addrLen = 4;
            break;
        case ProtoAddress::IPv6:
            reportType = REPORT_FLOW_IPv6;
            addrLen = 16;
            break;
        default:
            PLOG(PL_ERROR, "MgenAnalytic::Report::SetSrcAddr() error: invalid address type!\n");
            return false;
    }
    unsigned int reportLength = 12 + OFFSET_DST*4 + 2*addrLen;
    if (reportLength > GetBufferLength())
    {
        PLOG(PL_ERROR, "MgenAnalytic::Report::SetSrcAddr() error: insufficient buffer length\n");
        return false;
    }
    // TBD - Validate that src/dst addr type match??
    SetReportType(reportType);
    // Update report length field
    SetReportLength(reportLength);
    // set srcAddr field
    memcpy((char*)AccessBuffer32(OffsetSrc()), addr.GetRawHostAddress(), addrLen);
    // Set srcPort field
    SetUINT16(OffsetSrcPort(), addr.GetPort());
    SetLength(reportLength);  // update ProtoPkt length
    return true;
}  // end MgenAnalytic::Report::SetSrcAddr()

bool MgenAnalytic::Report::SetFlowId(UINT32 flowId)
{
    unsigned int reportLength = OffsetSrc()*4 + GetAddrLen() + 12 + 4;
    if (reportLength > GetBufferLength())
    {
        PLOG(PL_ERROR, "MgenAnalytic::Report::SetFlowId() error: insufficient buffer length\n");
        return false;
    }
    SetFlag(FLAG_FLOW_ID);
    SetUINT32(OffsetFlowId(), flowId);
    SetReportLength(reportLength); // update report length field
    SetLength(reportLength);       // update ProtoPkt length
    return true;
}  // end MgenAnalytic::Report::SetFlowId()

double MgenAnalytic::Report::UnquantizeOffset(UINT16 q)
{
    double mantissa = ((double)(q >> 3)) * (10.0/1024.0);
    double exponent = (double)(q & 0x0007);
    return mantissa * pow(10.0, exponent);   
}  // end MgenAnalytic::Report::UnquantizeOffset()

UINT16 MgenAnalytic::Report::QuantizeOffset(double offset)
{
    // encodes to 10 bits mantissa plus 3 bits of exponent
    // exponent in range of 1.e-03 to 1.0e+04
    if (offset < (1.0e-03))
        return 0x01;  // rate = 0.0
    else if (offset >= 10.0e+04)
        return 0x1fff;  // max value
    int exponent = (int)log10(offset);
    UINT16 mantissa = (UINT16)((1024.0/10.0) * (offset / pow(10.0, (double)exponent)) + 0.5);
    return (mantissa << 3) | ((UINT16)exponent + 3);
}  // end MgenAnalytic::Report::QuantizeOffset()

UINT16 MgenAnalytic::Report::QuantizeRate(double rate)
{
    if (rate <= 0.0) return 0x01;  // rate = 0.0
    UINT16 exponent = (UINT16)log10(rate);
    UINT16 mantissa = (UINT16)((4096.0/10.0) * (rate / pow(10.0, (double)exponent)) + 0.5);
    return ((mantissa << 4) | exponent);
}  // end MgenAnalytic::Report::QuantizeRate()

double MgenAnalytic::Report::UnquantizeRate(UINT16 rate)
{
    double mantissa = ((double)(rate >> 4)) * (10.0/4096.0);
    double exponent = (double)(rate & 0x000f);
    return mantissa * pow(10.0, exponent);   
}  // MgenAnalytic::Report::UnquantizeRate()

UINT16 MgenAnalytic::Report::QuantizeLoss(double lossFraction)
{
    if (0.0 == lossFraction) return 0;
    lossFraction = lossFraction*65535.0 + 0.5;
    if (lossFraction < 1.0)
        return 1;
    else if (lossFraction > 65535.0)
        return 65535;
    else
        return (UINT16)lossFraction;
}  // MgenAnalytic::Report::QuantizeLoss()

double MgenAnalytic::Report::UnquantizeLoss(UINT16 lossQuantized)
{
    return (((double)lossQuantized) / 65535.0);
}  // MgenAnalytic::Report::UnquantizeLoss()


const double MgenAnalytic::Report::TIME_STRETCH = 1.1;  // (1.0 + desired percent change per quantize step)
const double MgenAnalytic::Report::TIME_SCALE = 1.0 / (pow(TIME_STRETCH, 254) - TIME_STRETCH);
const double MgenAnalytic::Report::TIME_MIN = 1.0e-06;
const double MgenAnalytic::Report::TIME_MAX = 600.0;

UINT8 MgenAnalytic::Report::QuantizeTimeValue(double value)
{
    if (value > TIME_STRETCH*TIME_MAX)
        return 0xff;
    else if (value < TIME_MIN/2.0) 
        return 0;
    else if (value < TIME_MIN)
        return 1;
    else
        return (UINT8)((log(TIME_STRETCH + (value - TIME_MIN)/(TIME_SCALE*(TIME_MAX - TIME_MIN)))/ log(TIME_STRETCH)) + 0.5);
}  // end MgenAnalytic::Report::QuantizeTimeValue()
      
double MgenAnalytic::Report::UnquantizeTimeValue(UINT8 q)
{
    if (0 == q) return 0.0;
    return (TIME_MAX - TIME_MIN)*(pow(TIME_STRETCH, q) - TIME_STRETCH)*TIME_SCALE + TIME_MIN;
}  // end MgenAnalytic::Report::UnquantizeTimeValue()


MgenAnalyticReporter::MgenAnalyticReporter()
 : report_iterator(report_queue), last_unreported_item(NULL),
   iteration_start(NULL)
{
}

MgenAnalyticReporter::~MgenAnalyticReporter()
{
}


bool MgenAnalyticReporter::Add(MgenAnalytic& item)
{
    if (NULL == last_unreported_item)
    {
        MgenAnalytic* nextItem = report_iterator.PeekNextItem();
        if (NULL != nextItem)
        {
            // Insert new, unreported item before next item to be iterated
            if (!report_queue.Insert(item, *nextItem))
            {
                PLOG(PL_ERROR, "MgenAnalyticReporter::Add() error: %s\n", GetErrorString());
                return false;
            }
        }
        else
        {
            ASSERT(report_queue.IsEmpty());
            if (!report_queue.Prepend(item))
            {
                PLOG(PL_ERROR, "MgenAnalyticReporter::Add() error: %s\n", GetErrorString());
                return false;
            }
        }
    }
    else if (!report_queue.InsertAfter(item, *last_unreported_item))
    {
        PLOG(PL_ERROR, "MgenAnalyticReporter::Add() error: %s\n", GetErrorString());
        return false;
    }
    last_unreported_item = &item;
    return true;
}  // end  MgenAnalyticReporter::Add()

void MgenAnalyticReporter::Remove(MgenAnalytic& item)
{
    if (&item == last_unreported_item)
    {
        if (&item == report_iterator.PeekNextItem())
        {
            last_unreported_item = NULL;
        }
        else 
        {
            MgenAnalytic* prevItem = report_queue.GetPrev(item);
            if (NULL != prevItem)
                last_unreported_item = prevItem;
            else
                last_unreported_item = report_queue.GetTail();
        }
    }
    report_queue.Remove(item);
    if (NULL == report_iterator.PeekNextItem())
        report_iterator.Reset();
}  // end MgenAnalyticReporter::Remove(MgenAnalytic& item)

MgenAnalytic* MgenAnalyticReporter::PeekNextItem()
{
    MgenAnalytic* nextItem = report_iterator.PeekNextItem();
    if (NULL == nextItem)
    {
        // restart round-robin
        report_iterator.Reset();
        nextItem = report_iterator.PeekNextItem();
    }
    if (NULL == nextItem) return NULL; // empty list
    if (NULL == iteration_start)
        iteration_start = nextItem;
    else if (nextItem == iteration_start)
        return NULL;  // loop detected
    return nextItem;
}  // end MgenAnalyticReporter::PeekNextItem()

const MgenAnalytic::Report* MgenAnalyticReporter::GetNextReport(const ProtoTime& theTime)
{
    MgenAnalytic* nextItem = PeekNextItem();
    if (NULL == nextItem) return NULL;
    report_iterator.GetNextItem();  // advance iterator
    if (nextItem == last_unreported_item)
        last_unreported_item = NULL;
    const MgenAnalytic::Report& nextReport = nextItem->GetReport(theTime);
    return &nextReport;
}  // end MgenAnalyticReporter::GetNextReport()

const MgenAnalytic::Report* MgenAnalyticReporter::PeekNextReport(const ProtoTime& theTime)
{
    MgenAnalytic* nextItem = PeekNextItem();
    if (NULL == nextItem) return NULL;
    const MgenAnalytic::Report& nextReport = nextItem->GetReport(theTime);
    return &nextReport;
}  // end MgenAnalyticReporter::PeekNextReport()

void MgenAnalytic::Report::Log(FILE*                filePtr, 
                               const ProtoTime&     sentTime, 
                               const ProtoTime&     theTime, 
                               bool                 localTime,
                               const ProtoAddress&  reporterAddr) const
{
    if (NULL == filePtr) return;  // not logging
    // MGEN logging timestamp format (TBD - create an Mgen::LogTimeStamp() method
    Mgen::LogTimestamp(filePtr, theTime.GetTimeVal(), localTime);
    UINT32 flowId = GetFlowId();
    if (0 == flowId) flowId = 1;  // null flowId means flow 1 by default
    ProtoAddress srcAddr;
    GetSrcAddr(srcAddr);
    const char* protocol = "???";
    switch(GetProtocol())
    {
        case UDP:
            protocol = "UDP";
            break;
        case TCP:
            protocol = "TCP";
            break;
        case SINK:
            protocol = "SINK";
            break;
        default:
            protocol = "???";
            break;
    }
    Mgen::Log(filePtr, "REPORT proto>%s flow>%lu src>%s/%hu ", protocol, flowId, srcAddr.GetHostString(), srcAddr.GetPort());
    ProtoAddress dstAddr;
    GetDstAddr(dstAddr);
    Mgen::Log(filePtr, "dst>%s/%hu ", dstAddr.GetHostString(), dstAddr.GetPort());
    Mgen::Log(filePtr, "reporter>%s/%hu sent>", reporterAddr.GetHostString(), reporterAddr.GetPort());
    Mgen::LogTimestamp(filePtr, theTime.GetTimeVal(), localTime);
    Mgen::Log(filePtr, "offset>%lf ", GetWindowOffset());
    Mgen::Log(filePtr,"window>%lf rate>%lf kbps loss>%lf latency ave>%lf min>%lf max>%lf\n",
                GetWindowSize(), GetRateAve()*8.0e-03, GetLossFraction(), 
                GetLatencyAve(), GetLatencyMin(), GetLatencyMax());
}  // end MgenAnalytic::Report::Log()




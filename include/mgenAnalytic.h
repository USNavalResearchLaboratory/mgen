#ifndef _MGEN_ANALYTIC
#define _MGEN_ANALYTIC

#include "protoQueue.h"
#include "protoTime.h"
#include "protoAddress.h"
#include "protoBitmask.h"
#include "protoPkt.h"
#include "mgenGlobals.h"  // for Protocol types
#include "mgenPayload.h"

#include <math.h>  // for fabs()

// MGEN_DATA analytic report format
// 
//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      | type  | proto |      len      |flags|      windowOffset       |  
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                                                               |
//      +                          dstAddr ...                          +
//      |                                                               |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                                                               |
//      +                          srcAddr ...                          +
//      |                                                               |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |             dstPort           |            srcPort            |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
//      |                       flowID  (optional)                      |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |   windowSize   |   latencyAve |  latencyMin   |  latencyMax   |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
//      |            rateAve            |          lossFraction         |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//
//  type        - report message type 
//  proto       - transport protocol type
//  len         - message length in bytes
//  flags:
//      FLAG_FLOW_ID indicates report contains optional flowID field
//      FLAG_LATENCY_SIGN indicates "latencyAve" is a negative value
//  alen - length of address fields (in bytes) minus one (i.e. range of 1 - 16 bytes)
//  windowId     - window offset relative to MGEN txTime timestamp (offset = windowOffset*windowSize)
//  dstAddr      - flow dest addr.
//  srcAddr      - flow src addr
//  dstPort      - flow dst port
//  srcPort      - flow src port
//  flowId       - 32-bit MGEN flow id (optional field)  
//  windowSize   - duration of measurement time window for this report*
//  latencyAve   - measured average message latency for given window
//  latencyMin   - minimum observed message latency for given window (negative offset from average)
//  latencyMax   - maximum observed message latency for given window (positive offset from average)
//  rateAve      - measured flow rx rate for given measurement window
//  lossFraction - measured message loss rate during measurement window


// Per-flow statistics tracking for rate, loss, and latency
// where a flow is identified by src addr/port : dst addr/port 
// and MGEN flowId

class MgenAnalytic : public ProtoQueue::Item
{
    public:
        static const double DEFAULT_WINDOW;      
        enum {DEFAULT_HISTORY = 1024};  // default duplicate message detection history depth
            
        MgenAnalytic();
        ~MgenAnalytic();
        
        class Report;  // forward declaration
       
        
        // Init() MUST be called before analytic can be updated
        bool Init(Protocol               protocol,
                  const ProtoAddress&    srcAddr, 
                  const ProtoAddress&    dstAddr, 
                  UINT32                 flowId, 
                  double                 windowSize = MgenAnalytic::DEFAULT_WINDOW,
                  UINT32                 historyDepth = MgenAnalytic::DEFAULT_HISTORY);
        
        void SetWindowSize(double windowSize)
        {
            UINT8 q = Report::QuantizeTimeValue(windowSize);
            window_size = MgenAnalytic::Report::UnquantizeTimeValue(q);
        }
        
        // returns "true" when report values have been updated
        bool Update(const ProtoTime& rxTime,
                    unsigned int     msgSize = 0,
                    const ProtoTime& txTime = ProtoTime(0.0),
                    UINT32           seqNum = 0);
        
        void Log(FILE*            filePtr, 
                 const ProtoTime& sentTime, 
                 const ProtoTime& theTime, 
                 bool             localTime) const;
        
        const Report& GetReport(const ProtoTime& theTime);
        const ProtoTime& GetReportTime() const
            {return report_time;}
        
        const ProtoTime& GetWindowEnd() const
            {return window_end;}
        
        const ProtoTime& GetReportStartTime() const
            {return report_start;}
        double GetReportDuration() const
            {return report_duration;}
        unsigned long GetReportMessageCount() const
            {return report_msg_count;}
        double GetReportRateAverage() const     // bytes/sec
            {return report_rate_ave;}
        double GetReportLossFraction() const 
            {return report_loss_ave;}
        double GetReportLatencyAverage() const  // in seconds
            {return report_latency_ave;}
        double GetReportLatencyMin() const      // in seconds
            {return report_latency_min;}
        double GetReportLatencyMax() const      // in seconds
            {return report_latency_max;}
        
        // Worst-case flow key length (in bytes):
        // IPv6 dstAddr + dstPort + IPv6 srcAddr + srcPort + flowId
        //      16      +    2    +      16      +    2    +    4
        enum {KEY_MAX = (16 + 2 + 16 + 2 + 4)};
        
        
        class Report : public MgenDataItem
        {
            public:
                Report(UINT32*        bufferPtr = NULL, 
                       unsigned int   bufferBytes = 0, 
                       bool           freeOnDestruct = false);
                ~Report();
                
                enum ReportType
                {
                    REPORT_INVALID  = 0,
                    REPORT_FLOW_IPv4,
                    REPORT_FLOW_IPv6
                };
                
                enum Flag
                {
                    FLAG_FLOW_ID        = 0x01, // set when Flow ID field is present
                    FLAG_LATENCY_SIGN   = 0x02  // set when latencyAve is negative value
                };
                    
                // Useful for report buffer array sizing 
                // IPv6 with FlowId report is worst case. 
                enum {MAX_LENGTH = (4 + 2*16 + 4*4)};
              
                // Use these to parse    
                bool InitFromBuffer(UINT32*         bufferPtr = NULL, 
                                    unsigned int    numBytes = 0, 
                                    bool            freeOnDestruct = false);
                
                void Log(FILE*               filePtr, 
                         const ProtoTime&    sentTime, 
                         const ProtoTime&    theTime, 
                         bool                localTime,
                         const ProtoAddress& reporterAddr) const;
                
                ReportType GetReportType() const
                {
                    UINT8 field = GetUINT8(OFFSET_TYPE);
                    return (ReportType)((field >> 4) & 0x0f);
                }   
                Protocol GetProtocol() const
                {
                    UINT8 field = GetUINT8(OFFSET_PROTOCOL);
                    return (Protocol)(field & 0x0f);
                }   
                UINT8 GetReportLength() const
                    {return GetUINT8(OFFSET_LEN);}
                bool FlagIsSet(Flag flag) const
                    {return (0 != (flag & (GetUINT8(OFFSET_FLAGS) >> 5)));}     
                bool GetDstAddr(ProtoAddress& dstAddr) const;  // gets addr and port
                bool GetSrcAddr(ProtoAddress& srcAddr) const;  // gets addr and port
                UINT32 GetFlowId() const
                    {return (FlagIsSet(FLAG_FLOW_ID) ? GetUINT32(OffsetFlowId()) : 0);}
                double GetWindowOffset() const
                {
                    UINT16 q = GetUINT16(OFFSET_WINDOW) & 0x1fff;
                    return UnquantizeTimeValue(q);
                }
                double GetWindowSize() const
                {
                    UINT8 q = GetUINT8(OffsetWindowSize());
                    return UnquantizeTimeValue(q);
                }
                double GetLatencyAve() const
                {
                    UINT8 q = GetUINT8(OffsetLatencyAve());
                    double ave = UnquantizeTimeValue(q);
                    return (FlagIsSet(FLAG_LATENCY_SIGN) ? -ave : ave);
                }
                double GetLatencyMin() const
                {
                    UINT8 q = GetUINT8(OffsetLatencyMin());
                    return (GetLatencyAve() - UnquantizeTimeValue(q));
                }
                double GetLatencyMax() const
                {
                    UINT8 q = GetUINT8(OffsetLatencyMax());
                    return (GetLatencyAve() + UnquantizeTimeValue(q));
                }
                double GetRateAve() const
                {
                    UINT16 q = GetUINT16(OffsetRateAve());
                    return UnquantizeRate(q);
                }
                double GetLossFraction() const
                {
                    UINT16 q = GetUINT16(OffsetLossFraction());
                    return UnquantizeLoss(q);
                }
                
                // Use these to build a report (MUST call set dst/src/flowId in  
                // order first. All other fields may be set any time afterward)
                bool InitIntoBuffer(ReportType      reportType,
                                    UINT32*         bufferPtr = NULL, 
                                    unsigned int    bufferBytes = 0, 
                                    bool            freeOnDestruct = false);
             
                
                void SetProtocol(Protocol protocol)
                {
                    UINT8 field = GetUINT8(OFFSET_PROTOCOL);
                    field = (0xf0 & field) | (UINT8)protocol;
                    SetUINT8(OFFSET_PROTOCOL, field);
                }
                bool SetDstAddr(const ProtoAddress& dstAddr);
                bool SetSrcAddr(const ProtoAddress& srcAddr);
                bool SetFlowId(UINT32 flowId);
                void SetWindowOffset(double seconds)
                {
                    UINT16 q = QuantizeTimeValue(seconds);
                    UINT16 field = GetUINT16(OFFSET_WINDOW);
                    field = (field & 0xe000) | q;
                    SetUINT16(OFFSET_WINDOW, field);
                }
                void SetWindowSize(double seconds)
                {
                    UINT8 q = QuantizeTimeValue(seconds);
                    SetUINT8(OffsetWindowSize(), q);
                }
                void SetLatencyAve(double seconds)
                {
                    if (seconds < 0.0) SetFlag(FLAG_LATENCY_SIGN);
                    UINT8 q = QuantizeTimeValue(fabs(seconds));
                    SetUINT8(OffsetLatencyAve(), q);
                }
                void SetLatencyDeltaMin(double deltaSeconds)
                {
                    UINT8 q = QuantizeTimeValue(fabs(deltaSeconds));
                    SetUINT8(OffsetLatencyMin(), q);
                }
                void SetLatencyDeltaMax(double deltaSeconds)
                {
                    UINT8 q = QuantizeTimeValue(fabs(deltaSeconds));
                    SetUINT8(OffsetLatencyMax(), q);
                }
                void SetRateAve(double rate)
                {
                    UINT16 q = QuantizeRate(rate);
                    SetUINT16(OffsetRateAve(), q);
                }
                void SetLossFraction(double loss)
                {
                    UINT16 q = QuantizeLoss(loss);
                    SetUINT16(OffsetLossFraction(), q);
                }
                
                static UINT8 QuantizeTimeValue(double value);
                static double UnquantizeTimeValue(UINT8 timeQuantized);
                
                    
            private:
                // Routines for our compressed rate, loss, and time value fields
                static UINT16 QuantizeOffset(double offset);
                static double UnquantizeOffset(UINT16 q);
                static UINT16 QuantizeRate(double rate);
                static double UnquantizeRate(UINT16 rate);
                static UINT16 QuantizeLoss(double lossFraction);
                static double UnquantizeLoss(UINT16 lossQuantized);
                static const double TIME_STRETCH;
                static const double TIME_SCALE;
                static const double TIME_MIN;
                static const double TIME_MAX;
                    
                // ProtoPkt offsets for set/get are byte offsets!
                enum
                {
                    OFFSET_PROTOCOL = OFFSET_TYPE,     // lower 4 bits            
                    OFFSET_FLAGS = OFFSET_LEN + 1,    
                    OFFSET_WINDOW = OFFSET_LEN + 1, 
                    OFFSET_DST = (OFFSET_WINDOW + 2)/4 // UINT32 offset 
                };
                    
                unsigned int GetAddrLen() const;  // based on report type
                    
                unsigned int OffsetSrc() const         // UINT32 offset
                    {return (OFFSET_DST + GetAddrLen()/4);} 
                unsigned int OffsetDstPort() const     // byte offset
                    {return (4*OffsetSrc() + GetAddrLen());}
                unsigned int OffsetSrcPort() const     // byte offset
                    {return (OffsetDstPort() + 2);}
                unsigned int OffsetFlowId() const      // byte offset
                    {return (OffsetSrcPort() + 2);}
                unsigned int OffsetWindowSize() const  // byte offset
                    {return (OffsetFlowId() + (FlagIsSet(FLAG_FLOW_ID) ? 4 : 0));}
                unsigned int OffsetLatencyAve() const  // byte offset
                    {return (OffsetWindowSize() + 1);}
                unsigned int OffsetLatencyMin() const  // byte offset
                    {return (OffsetLatencyAve() + 1);}
                unsigned int OffsetLatencyMax() const  // byte offset
                    {return (OffsetLatencyMin() + 1);}
                unsigned int OffsetRateAve() const     // byte offset (2 bytes) 
                    {return (OffsetLatencyMax() + 1);}
                unsigned int OffsetLossFraction() const// byte offset (2 bytes) 
                    {return (OffsetRateAve() + 2);}
                    
                void SetReportType(ReportType type)
                {
                    UINT8 field = GetUINT8(OFFSET_TYPE);
                    field = (0x0f & field) | ((UINT8)type << 4);
                    SetUINT8(OFFSET_TYPE, field);
                }
                void SetReportLength(UINT8 numBytes)
                    {SetUINT8(OFFSET_LEN, numBytes);}
                void SetFlag(Flag flag)
                {
                    UINT8 field = GetUINT8(OFFSET_FLAGS);
                    field |= (flag << 5);
                    SetUINT8(OFFSET_FLAGS, field);
                }
                
        };  // end class MgenAnalytic::Report
        
        // Used for ProtoIndexedQueue::Item lookup
        const char* GetKey() const
            {return flow_key;} 
        unsigned int GetKeysize() const
            {return flow_keysize;}  
        
    private:
        char*               flow_key;
        unsigned int        flow_keysize;  // in bits
        
        // State variables for current (in progress) analytics
        double              window_size;  
        bool                window_valid;
        ProtoTime           window_start;
        ProtoTime           window_end;
        unsigned long       msg_count;
        unsigned long       byte_count;
        unsigned long       dup_msg_count;
        UINT32              seq_start;
        ProtoSlidingMask    dup_mask;
        double              latency_sum;
        double              latency_min;
        double              latency_max;
        
        // Results of previous analytic
        bool                report_valid;
        ProtoTime           report_start;
        double              report_duration;
        unsigned long       report_msg_count;
        double              report_rate_ave;  // bytes/sec
        double              report_loss_ave;
        double              report_latency_ave;
        double              report_latency_min;
        double              report_latency_max;
        ProtoTime           report_time;
        UINT32              report_buffer[Report::MAX_LENGTH/sizeof(UINT32)];
        Report              report_msg;
        
};  // end class MgenAnalytic

class MgenAnalyticTable : public ProtoIndexedQueueTemplate<MgenAnalytic>
{
    public:
        MgenAnalytic* FindFlow(const ProtoAddress& srcAddr, const ProtoAddress& dstAddr,  UINT32 flowId);
    
    private:
        // Required overrides for ProtoIndexedQueue subclasses
        // (Override these to determine how items are sorted)
        virtual const char* GetKey(const Item& item) const
            {return (static_cast<const MgenAnalytic&>(item).GetKey());}
        virtual unsigned int GetKeysize(const Item& item) const
            {return (static_cast<const MgenAnalytic&>(item).GetKeysize());}
};  // end class MgenAnalyticTable

// This has a linked list and a built-in iterator to prioritize reporting of flows
// (unreported updates have precedence over reports already transmitted.  Otherwise,
// round-robin reporting is implemented.
class MgenAnalyticReporter
{
    public:
        MgenAnalyticReporter();
        ~MgenAnalyticReporter();
        
        bool Add(MgenAnalytic& item);
        void Remove(MgenAnalytic& item);
        
        // The Reset() call here resets loop detection
        // I.e., this reset does _not_ reset the underlying
        // report_iterator so the reports with the most
        // precedence are still returned first after a reset.
        void Reset()
            {iteration_start = NULL;}
        
        const MgenAnalytic::Report* PeekNextReport(const ProtoTime& theTime);
        void Advance()
            {report_iterator.GetNextItem();}
        
        const MgenAnalytic::Report* GetNextReport(const ProtoTime& theTime);
        
        
    private:
        class Queue : public ProtoSimpleQueueTemplate<MgenAnalytic> {};
        MgenAnalytic* PeekNextItem();
    
        Queue            report_queue;
        Queue::Iterator  report_iterator;
        MgenAnalytic*    last_unreported_item;
        MgenAnalytic*    iteration_start;
        
};  // end class MgenReportQueue()

#endif // _MGEN_ANALYTIC

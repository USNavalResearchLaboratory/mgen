

// Makes an MGEN text log file from a pcap trace file
// (the pcap log time is used as MGEN "recv" time) 
// Assumes UDP packets in tcpdump trace file (pcap file) are
// MGEN packets and parses to build an MGEN log file

// Notes on options:
//
// 1) The "trace" option prepends MGEN log lines with epoch time and MAC src/addr information


#include <string.h>
#include <stdio.h>
#include <iostream>
#include <pcap.h>
#include "protoPktETH.h" // for Ethernet frame parsing
#include "protoPktIP.h"  // for IP packet parsing
#include "protoPktARP.h"
#include "mgenMsg.h"
#include "protoTime.h"
#include "mgen.h"

// enum Command
// {
//       INVALID_COMMAND,
//       // EVENT,
//       // START,     // specify absolute start time
//       INPUT,     // input and parse an MGEN script
//       OUTPUT,    // open output (log) file 
//       // LOG,       // open log file for appending
//       // NOLOG,     // no output
//       // TXLOG,     // turn transmit logging on
//       RXLOG,     // turn recv event loggging on/off (on by default)
//       // LOCALTIME, // print log messages in localtime rather than gmtime (the default)
//       // DLOG,      // set debug log file
//       // SAVE,      // save pending flow state/offset info on exit.
//       // DEBUG_LEVEL,     // specify debug level
//       // OFFSET,    // time offset into script
//       // TXBUFFER,  // Tx socket buffer size
//       // RXBUFFER,  // Rx socket buffer size
//       // LABEL,     // IPv6 flow label
//       // BROADCAST, // send/receive broadcasts from socket
//       // TOS,       // IPV4 Type-Of-Service 
//       // TTL,       // Multicast Time-To-Live
//       // UNICAST_TTL, // Unicast Time-To-Live
//       // DF,        // DF/Fragmentation status
//       // INTERFACE, //Multicast Interface
//       // BINARY,    // turn binary logfile mode on
//       // FLUSH,     // flush log after _each_ event
//       // CHECKSUM,  // turn on _both_ tx and rx checksum options
//       // TXCHECKSUM,// include checksums in transmitted MGEN messages
//       // RXCHECKSUM,// force checksum validation at receiver _always_
//       // QUEUE,     // Turn off tx_timer when pending queue exceeds this limit
//       // REUSE,     // Toggle socket reuse on and off
//       ANALYTICS, // Turns on compute of analytics (and logging if enabled)
//       REPORT,    // Include analytic reports in message payloads for all flows
//       WINDOW,    // specifies analytic averaging window size (seconds)
//       TRACE
//       // SUSPEND,
//       // RESUME,
//       // RETRY,     // Enables TCP retry connection attempts
//       // PAUSE,     // Pauses flow while tcp attempts to reconnect
//       // RECONNECT, // Enables TCP reconnect
//       // RESET
// };

enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
// default arg values. Global for now--TODO; make this more C++ code (i.e., pcap2mgen class) and less C code
bool compute_analytics = false;
double analytic_window = MgenAnalytic::DEFAULT_WINDOW;
MgenAnalyticTable analytic_table;
// MgenFlowList flow_list;
// unsigned int default_flow_label = 0;
// int default_queue_limit = 0;
FILE* infile = stdin;
FILE* outfile = stdout;
bool trace = false;
bool log_rx = true;


const char* const CMD_LIST[] = 
{
    "-report",    // Grab MGEN REPORT messages from pcap file
    "+infile",    // Name of pcap input file
    "+outfile",   // Name of output file
    "-trace",     // Prepends MGEN log lines with epoch time and MAC src/addr info
    "+rxlog",     // Turns on/off recv log info. For report messages only
    "+window"     // Sets analytic window
};


void Usage()
{
    fprintf(stderr, "pcap2mgen [trace][epoch][pcapInputFile [mgenOutputFile]]\n");
}

void SetAnalyticWindow(double windowSize)
{
    if (windowSize <= 0.0) return;
    unsigned short int q = MgenAnalytic::Report::QuantizeTimeValue(windowSize);
    analytic_window = MgenAnalytic::Report::UnquantizeTimeValue(q);
    MgenAnalyticTable::Iterator iterator(analytic_table);
    MgenAnalytic* next;
    while (NULL != (next = iterator.GetNextItem()))
        next->SetWindowSize(windowSize);
}


CmdType GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);

    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        char lowerCmd[32];  // all commands < 32 characters
        len = len < 31 ? len : 31;
        unsigned int i;

        for (i = 0; i < (len + 1); i++)
        {
            lowerCmd[i] = tolower(cmd[i]);
        }

        if (!strncmp(lowerCmd, *nextCmd + 1, len))
        {
            if (matched)
            {
                // ambiguous command, should only match oncee
                return CMD_INVALID;
            }
            else
            {
                matched = true;
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type;
}   // end GetCmdType


bool OnCommand(const char* cmd, const char* val)
{
    CmdType type = GetCmdType(cmd);
    unsigned int len = strlen(cmd);
    char lowerCmd[32];  // all commands < 32 characters    
    len = len < 31 ? len : 31;
    unsigned int i;
    for (i = 0; i < (len + 1); i++)
        lowerCmd[i] = tolower(cmd[i]);

    if (CMD_INVALID == type)
    {
        fprintf(stderr, "pcap2mgen ProcessCommands(%s) error: invalid command\n", cmd);
        return false;
    }
    else if ((CMD_ARG == type) && (NULL == val))
    {
        fprintf(stderr, "pcap2mgen ProcessCommands(%s) missing argument\n", cmd);
        return false;
    }
    else if (!strncmp("report", lowerCmd, len))
    {
        compute_analytics = true;
    }
    else if (!strncmp("infile", lowerCmd, len))
    {
        if(NULL == (infile = fopen(val, "r")))
        {
            fprintf(stderr, "pcap2mgen: error opening input file: %s", val);
            return false;
        }
    }
    else if (!strncmp("outfile", lowerCmd, len))
    {
        if(NULL == (outfile = fopen(val, "w+")))
        {
            fprintf(stderr, "pcap2mgen: error opening output file: %s", val);
            return false;
        }
    }
    else if (!strncmp("trace", lowerCmd, len))
    {
        trace = true;
    }
    else if (!strncmp("rxlog", lowerCmd, len))
    {
        // std::cout << "lowerCmd = " << lowerCmd << std::endl;
        if(!val)
        {
            fprintf(stderr, "pcap2mgen OnCommand() Error: missing argument to rxlog\n");
            return false;
        }
        bool rxLogTmp;
        // convert to lower case for case-insensitivity
        char temp[4];
        unsigned int len = strlen(val);
        len = len < 4 ? len : 4;
        unsigned int j;
        for(j = 0; j < len; j++)
            temp[j] = tolower(val[j]);
        temp[j] = '\0';
        if(!strncmp("on", temp, len))
            rxLogTmp = true;
        else if(!strncmp("off", temp, len))
            rxLogTmp = false;
        else
        {
            fprintf(stderr, "pcap2mgen OnCommand Error: wrong argument to rxlog: %s\n", val);
            return false;
        }
        log_rx = rxLogTmp;
    }
    else if (!strncmp("window", lowerCmd, len))
    {
        if (NULL != val)
        {
            double windowSize;
            if((1 != sscanf(val, "%lf", &windowSize)) || (windowSize <= 0.0))
            {
                fprintf(stderr, "Mgen::OnCommand() Error: invalid WINDOW interval\n");
            }
            SetAnalyticWindow(windowSize);
        }
        else
        {
            fprintf(stderr, "Mgen::OnCommand() Error: missing argument to WINDOW\n");
            return false;
        }
    }

    return true;
}


bool ProcessCommands(int argc, const char*const* argv)
{
    int i = 1;
    // bool convert = false;

    while (i < argc)
    {
        CmdType cmdType = GetCmdType(argv[i]);

        switch (cmdType)
        {
            case CMD_INVALID:
                fprintf(stderr, "pcap2mgen error: invalid command: %s\n", argv[i]);
                return false;
            case CMD_NOARG:
                if(!OnCommand(argv[i], NULL))
                {
                    fprintf(stderr, "pcap2mgen OnCommand(%s) error\n", argv[i]);
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if(!OnCommand(argv[i], argv[i+1]))
                {
                    fprintf(stderr, "pcap2mgen OnCommand(%s, %s) error\n", argv[i], argv[i+1]);
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;
}  // end ProcessCommands()



int main(int argc, char* argv[])
{
    // bool debug = true;
    if(!ProcessCommands(argc, argv))
    {
        fprintf(stderr, "pcap2mgen: error while processing startup commands\n");
    }
    // bool trace = false;
    // // First pull out any "options" from command line
    // int argOffset = 0;
    // for (int i = 1; i < argc; i++)
    // {
    //     if (0 == strcmp("trace", argv[i]))
    //     {
    //         trace = true;  // enables prepending of MAC src/dst addr info
    //     }
    //     else
    //     {
    //         // It's not an option so assume it's a file name
    //         argOffset = i - 1;
    //         break;
    //     }
    // }
    
    fprintf(stderr,"Pcap Version: %s\n",pcap_lib_version());

    // if(debug)
    // {
    //     std::cout << "log_rx = " << log_rx << std::endl;
    //     std::cout << "compute_analytics = " << compute_analytics << std::endl;
    // }

    
    // Use stdin/stdout by default
    
    // switch(argc - argOffset)
    // {
    //     case 1:
    //         // using default stdin/stdout
    //         break;
    //     case 2:
    //         // using named input pcap file and stdout
    //         if (NULL == (infile = fopen(argv[1 + argOffset], "r")))
    //         {
    //             perror("pcap2mgen: error opening input file");
    //             return -1;
    //         }
    //         break;
    //     case 3:
    //         // use name input and output files
    //         if (NULL == (infile = fopen(argv[1 + argOffset], "r")))
    //         {
    //             perror("pcap2mgen: error opening input file");
    //             return -1;
    //         }
    //         if (NULL == (outfile = fopen(argv[2 + argOffset], "w+")))
    //         {
    //             perror("pcap2mgen: error opening output file");
    //             return -1;
    //         }
    //         break;
    //     default:
    //         fprintf(stderr, "pcap2mgen: error: too many arguments!\n");
    //         Usage();
    //         return -1;
    // }  // end switch(argc)

    // flow_list.SetDefaultLabel(default_flow_label);
    
    char pcapErrBuf[PCAP_ERRBUF_SIZE+1];
    pcapErrBuf[PCAP_ERRBUF_SIZE] = '\0';
    pcap_t* pcapDevice = pcap_fopen_offline(infile, pcapErrBuf);
    if (NULL == pcapDevice)
    {
        fprintf(stderr, "pcap2mgen: pcap_fopen_offline() error: %s\n", pcapErrBuf);
        if (stdin != infile) fclose(infile);
        if (stdout != outfile) fclose(outfile);
        return -1;
    }
    
    
    UINT32 alignedBuffer[4096/4];   // 128 buffer for packet parsing
    UINT16* ethBuffer = ((UINT16*)alignedBuffer) + 1; 
    unsigned int maxBytes = 4096 - 2;  // due to offset, can only use 4094 bytes of buffer
    
    pcap_pkthdr hdr;
    const u_char* pktData;
    while(NULL != (pktData = pcap_next(pcapDevice, &hdr)))
    {
        unsigned int numBytes = maxBytes;
        if (hdr.caplen < numBytes) numBytes = hdr.caplen;
        memcpy(ethBuffer, pktData, numBytes);
        ProtoPktETH ethPkt((UINT32*)ethBuffer, maxBytes);
        if (!ethPkt.InitFromBuffer(hdr.len))
        {
            fprintf(stderr, "pcap2mgen error: invalid Ether frame in pcap file\n");
            continue;
        }    
        ProtoPktIP ipPkt;
        ProtoAddress srcAddr, dstAddr;
        ProtoPktETH::Type ethType = ethPkt.GetType();
        if ((ProtoPktETH::IP == ethType) ||
            (ProtoPktETH::IPv6 == ethType))
        {
            unsigned int payloadLength = ethPkt.GetPayloadLength();
            if (!ipPkt.InitFromBuffer(payloadLength, (UINT32*)ethPkt.AccessPayload(), payloadLength))
            {
                fprintf(stderr, "pcap2mgen error: bad IP packet\n");
                continue;
            }
            switch (ipPkt.GetVersion())
            {
                case 4:
                {
                    ProtoPktIPv4 ip4Pkt(ipPkt);
                    ip4Pkt.GetDstAddr(dstAddr);
                    ip4Pkt.GetSrcAddr(srcAddr);
                    break;
                } 
                case 6:
                {
                    ProtoPktIPv6 ip6Pkt(ipPkt);
                    ip6Pkt.GetDstAddr(dstAddr);
                    ip6Pkt.GetSrcAddr(srcAddr);
                    break;
                }
                default:
                {
                    PLOG(PL_ERROR,"pcap2mgen Error: Invalid IP pkt version.\n");
                    break;
                }
            }
            //PLOG(PL_ALWAYS, "pcap2mgen IP packet dst>%s ", dstAddr.GetHostString());
            //PLOG(PL_ALWAYS," src>%s length>%d\n", srcAddr.GetHostString(), ipPkt.GetLength());
        }
        if (!srcAddr.IsValid()) continue;  // wasn't an IP packet
        
        ProtoPktUDP udpPkt;
        if (!udpPkt.InitFromPacket(ipPkt)) continue;  // not a UDP packet
        
        MgenMsg msg;
        if (!msg.Unpack(udpPkt.AccessPayload(), udpPkt.GetPayloadLength(), false, false))
        {
            fprintf(stderr, "pcap2mgen warning: UDP packet not an MGEN packet?\n");
            continue;
        }
        msg.SetProtocol(UDP);
        srcAddr.SetPort(udpPkt.GetSrcPort());
        msg.SetSrcAddr(srcAddr);
        
        if (trace)
        {
            fprintf(outfile, "%lu.%lu ", (unsigned long)hdr.ts.tv_sec, (unsigned long)hdr.ts.tv_usec);
            ProtoAddress ethAddr;
            ethPkt.GetSrcAddr(ethAddr);
            fprintf(outfile, "esrc>%s ", ethAddr.GetHostString());
            ethPkt.GetDstAddr(ethAddr);
            fprintf(outfile, "edst>%s ", ethAddr.GetHostString());
        }
        // TBD - Add option to log REPORT events only?  Embed MGEN analytic, too?
        if(compute_analytics)
        {
            MgenAnalytic* analytic = analytic_table.FindFlow(msg.GetSrcAddr(), msg.GetDstAddr(), msg.GetFlowId());
            if (NULL == analytic)
            {
                if (NULL == (analytic = new MgenAnalytic()))
                {
                    fprintf(stderr, "Mgen::UpdateRecvAnalytics() new MgenAnalytic() error: %s\n", GetErrorString());
                    return -1;
                }
                if (!analytic->Init(msg.GetProtocol(), msg.GetSrcAddr(), msg.GetDstAddr(), msg.GetFlowId(), analytic_window))
                {
                    fprintf(stderr, "Mgen::UpdateRecvAnalytics() MgenAnalytic() initialization error: %s\n", GetErrorString());
                    return -1;
                }
                if (!analytic_table.Insert(*analytic))
                {
                    fprintf(stderr, "Mgen::UpdateRecvAnalytics() unable to add new flow analytic: %s\n", GetErrorString());
                    delete analytic;
                    return -1;
                }
            }

            ProtoTime rxTime(hdr.ts);
            if (analytic->Update(rxTime, msg.GetMsgLen(), ProtoTime(msg.GetTxTime()), msg.GetSeqNum()))
            {
                // MgenFlow* nextFlow = flow_list.Head();
                // while (NULL != nextFlow)
                // {
                //     if (nextFlow->GetReportAnalytics())
                //         nextFlow->UpdateAnalyticReport(*analytic);
                //     nextFlow = flow_list.GetNext(nextFlow);
                // }       
                const MgenAnalytic::Report& report = analytic->GetReport(rxTime);
                // if (NULL != controller)
                //     controller->OnUpdateReport((unsigned long)hdr.ts.tv_sec, report);
                report.Log(outfile, rxTime, rxTime, false);
                /*// locally print updated report info (TBD - do REPORT log event for received flows)
                time_t tvSec = theTime.sec();
                struct tm* timePtr = gmtime(&tvSec);
                fprintf(stdout, "%02d:%02d:%02d.%06lu ", timePtr->tm_hour, 
                                 timePtr->tm_min, timePtr->tm_sec, theTime.usec());
                fprintf(stdout, "%s/%hu->", theMsg->GetSrcAddr().GetHostString(), theMsg->GetSrcAddr().GetPort());
                fprintf(stdout, "%s/%hu,%lu ", theMsg->GetDstAddr().GetHostString(), theMsg->GetDstAddr().GetPort(), (unsigned long)theMsg->GetFlowId());
                fprintf(stdout, "rate>%lg kbps ", analytic->GetReportRateAverage()*8.0e-03);
                fprintf(stdout, "loss>%lg ", analytic->GetReportLossFraction()*100.0);
                fprintf(stdout, "latency>%lg,%lg,%lg\n", analytic->GetReportLatencyAverage(),
                                analytic->GetReportLatencyMin(), analytic->GetReportLatencyMax());*/
            }
        }

        // Should we make "flush" true by default?
        msg.LogRecvEvent(outfile, false, false, log_rx, false, true, udpPkt.AccessPayload(), false, hdr.ts);        
    }  // end while (pcap_next())
    
    if (stdin != infile) fclose(infile);
    if (stdout != outfile) fclose(outfile);
    return 0;
}  // end main()

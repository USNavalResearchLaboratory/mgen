

// Makes an MGEN text log file from a pcap trace file
// (the pcap log time is used as MGEN "recv" time) 
// Assumes UDP packets in tcpdump trace file (pcap file) are
// MGEN packets and parses to build an MGEN log file


#include <stdio.h>
#include <pcap.h>
#include "protoPktETH.h" // for Ethernet frame parsing
#include "protoPktIP.h"  // for IP packet parsing
#include "protoPktARP.h"
#include "mgenMsg.h"

void Usage()
{
    fprintf(stderr, "pcap2mgen [trace][pcapInputFile [mgenOutputFile]]\n");
}

int main(int argc, char* argv[])
{
    bool trace = false;
    // First pull out any "options" from command line
    int argOffset = 0;
    for (int i = 1; i < argc; i++)
    {
        if (0 == strcmp("trace", argv[i]))
        {
            trace = true;  // enables prepending of MAC src/dst addr info
        }
        else
        {
            // It's not an option so assume it's a file name
            argOffset = i - 1;
            break;
        }
    }
    
    
    // Use stdin/stdout by default
    FILE* infile = stdin;
    FILE* outfile = stdout;
    switch(argc - argOffset)
    {
        case 1:
            // using default stdin/stdout
            break;
        case 2:
            // using named input pcap file and stdout
            if (NULL == (infile = fopen(argv[1 + argOffset], "r")))
            {
                perror("pcap2mgen: error opening input file");
                return -1;
            }
            break;
        case 3:
            // use name input and output files
            if (NULL == (infile = fopen(argv[1 + argOffset], "r")))
            {
                perror("pcap2mgen: error opening input file");
                return -1;
            }
            if (NULL == (outfile = fopen(argv[2 + argOffset], "w+")))
            {
                perror("pcap2mgen: error opening output file");
                return -1;
            }
            break;
        default:
            fprintf(stderr, "pcap2mgen: error: too many arguments!\n");
            Usage();
            return -1;
    }  // end switch(argc)
    
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
        if (!msg.Unpack((char*)udpPkt.GetPayload(), udpPkt.GetPayloadLength(), false))
        {
            fprintf(stderr, "pcap2mgen warning: UDP packet not an MGEN packet?\n");
            continue;
        }
        msg.SetProtocol(UDP);
        srcAddr.SetPort(udpPkt.GetSrcPort());
        msg.SetSrcAddr(srcAddr);
        
        if (trace)
        {
            ProtoAddress ethAddr;
            ethPkt.GetSrcAddr(ethAddr);
            fprintf(outfile, "esrc>%s ", ethAddr.GetHostString());
            ethPkt.GetDstAddr(ethAddr);
            fprintf(outfile, "edst>%s ", ethAddr.GetHostString());
        }
        
        msg.LogRecvEvent(outfile, false, false, (char*)udpPkt.AccessPayload(), false, hdr.ts);        
    }  // end while (pcap_next())
    
}  // end main()

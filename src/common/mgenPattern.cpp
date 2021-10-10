#include "mgenGlobals.h"
#include "mgen.h"
#include "mgenMsg.h"
#include "mgenPattern.h"
#include "mgenEvent.h"

#include <string.h>
#include <stdio.h>   
#include <time.h>    // for gmtime()
#include <ctype.h>   // for toupper()

MgenPattern::MgenPattern()
    : interval_remainder(0.0),burst_pattern(NULL),unlimitedRate(false),
    flowPaused(false)
#ifdef HAVE_PCAP
  ,pcap_device(NULL),
  file_type(INVALID_FILETYPE),repeat_count(-1)
#endif //HAVE_PCAP
{
#ifdef HAVE_PCAP
  clone_fname[0] = '\0';
#endif//HAVE_PCAP
}

MgenPattern::~MgenPattern()
{
    if (burst_pattern) 
    {
        delete burst_pattern;  
        burst_pattern = NULL;
    } 
}

const StringMapper MgenPattern::TYPE_LIST[] = 
{
    {"PERIODIC", PERIODIC},
    {"POISSON", POISSON},
    {"BURST", BURST},
    {"JITTER", JITTER},
#ifdef HAVE_PCAP
    {"CLONE", CLONE},
#endif //HAVE_PCAP
    {"XXXX", INVALID_TYPE}   
};


MgenPattern::Type MgenPattern::GetTypeFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
    size_t len = strlen(string);
    len = len < 15 ? len : 15;
    unsigned int i;
    for (i =0 ; i < len; i++)
        upperString[i] = toupper(string[i]);
    upperString[i] = '\0';
    unsigned int matchCount = 0;
    Type patternType = INVALID_TYPE;
    const StringMapper* m = TYPE_LIST;
    while (INVALID_TYPE != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            patternType = ((Type)((*m).key));
            matchCount++;
        }
        m++; 
    }
    if (matchCount > 1)
    {
        DMSG(0, "MgenPattern::GetTypeFromString() error: ambiguous pattern type\n");    
        return INVALID_TYPE;
    }
    else
    {
        return patternType;
    }
}  // end MgenPattern::GetTypeFromString()

const StringMapper MgenPattern::BURST_LIST[] = 
{
    {"REGULAR", REGULAR},
    {"RANDOM",  RANDOM},
    {"XXXX",    INVALID_BURST} 
};

MgenPattern::Burst MgenPattern::GetBurstTypeFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
    size_t len = strlen(string);
    len = len < 15 ? len : 15;
    unsigned int i;
    for (i =0 ; i < len; i++)
        upperString[i] = toupper(string[i]);
    upperString[i] = '\0';
    unsigned int matchCount = 0;
    Burst burstType = INVALID_BURST;
    const StringMapper* m = BURST_LIST;
    while (INVALID_BURST != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            burstType = ((Burst)((*m).key));
            matchCount++;
        }
        m++;
    }
    if (matchCount > 1)
    {
        DMSG(0, "MgenPattern::GetBurstTypeFromString() Error: ambiguous burst type\n");
        return INVALID_BURST;
    }
    else
    {
        return burstType;
    }
}  // end MgenPattern::GetBurstTypeFromString()

const StringMapper MgenPattern::DURATION_LIST[] = 
{
    {"FIXED",       FIXED},
    {"EXPONENTIAL", EXPONENTIAL},
    {"XXXX",        INVALID_DURATION} 
};

MgenPattern::Duration MgenPattern::GetDurationTypeFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
    size_t len = strlen(string);
    len = len < 15 ? len : 15;
    unsigned int i;
    for (i =0 ; i < len; i++)
        upperString[i] = toupper(string[i]);
    upperString[i] = '\0';
    unsigned int matchCount = 0;
    Duration durationType = INVALID_DURATION;
    const StringMapper* m = DURATION_LIST;
    while (INVALID_DURATION != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            durationType = ((Duration)((*m).key));
            matchCount++;
        }
        m++; 
    }
    if (matchCount > 1)
    {
        DMSG(0, "MgenPattern::GetDurationTypeFromString() Error: ambiguous duration type\n");
        return INVALID_DURATION;
    }
    else
    {
        return durationType;
    }
}  // end MgenPattern::GetDurationTypeFromString()

#ifdef HAVE_PCAP
const StringMapper MgenPattern::CLONE_FILE_LIST[] =
  {
    {"TCPDUMP", TCPDUMP},
    {"XXXX",    INVALID_FILETYPE}
  };
MgenPattern::FileType MgenPattern::GetFileTypeFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
    unsigned int len = strlen(string);
    len = len < 15 ? len : 15;
    unsigned int i;
    for (i =0 ; i < len; i++)
      upperString[i] = toupper(string[i]);
    upperString[i] = '\0';
    unsigned int matchCount = 0;
    MgenPattern::FileType patternType = MgenPattern::INVALID_FILETYPE;
    const StringMapper* m = MgenPattern::CLONE_FILE_LIST;
    while (MgenPattern::INVALID_FILETYPE != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
	  patternType = ((MgenPattern::FileType)((*m).key));
            matchCount++;
        }
        m++; 
    }
    if (matchCount > 1)
    {
        DMSG(0, "MgenPattern::GetFileTypeFromString() error: ambiguous pattern type\n");    
        return MgenPattern::INVALID_FILETYPE;
    }
    else
    {
      return patternType;
    }

} // end MgenPattern::GetFileTypeFromString()
#endif //HAVE_PCAP

bool MgenPattern::InitFromString(MgenPattern::Type theType, const char* string, Protocol protocol)
{
    flowPaused = false;
    type = theType;
    switch (type)
    {
        case PERIODIC:  // form "<aveRate> <pktSize>"
        case POISSON:
        {

            double aveRate;
            char sizeText[257];
            if (2 != sscanf(string, "%lf %256s", &aveRate, sizeText))
            {
                DMSG(0, "MgenPattern::InitFromString(PERIODIC/POISSON) error: invalid parameters.\n");
                return false; 
            }
            // Look for colon delimiter to indicate variable packet size
            if (NULL != strchr(sizeText, ':'))
            {
                if (2 != sscanf(sizeText, "%u:%u", &pkt_size_min, &pkt_size_max))
                {
                    DMSG(0, "MgenPattern::InitFromString(PERIODIC/POISSON) error: invalid  variable <size> parameter.\n");
                    return false;
                }
                else if (pkt_size_min > pkt_size_max)
                {
                    unsigned int temp = pkt_size_min;
                    pkt_size_min = pkt_size_max;
                    pkt_size_max = temp;
                }
            }
            else if (1 != sscanf(sizeText, "%u", &pkt_size_min))
            {
                DMSG(0, "MgenPattern::InitFromString(PERIODIC/POISSON) error: invalid <size> parameter.\n");
                return false; 
            }        
            else
            {
                pkt_size_max = pkt_size_min;
            }   
            if (aveRate < 0.0)
            {
                if (-1.0 != aveRate)
                {
                    DMSG(0, "MgenPattern::InitFromString(PERIODIC/POISSON) error: invalid packet rate.\n");
                    return false;
                }
                else
                {
		          unlimitedRate = true;
		          interval_ave = 0.0;   
                }
            }
            else if (aveRate > 0.0)
            {
                interval_ave = 1.0 / aveRate;
            }
            else
            {
	            interval_ave = -1.0;
                flowPaused = true;
            }
	        if (UDP != protocol)
		    {
                // unlimited message size for non-UDP protocols
                if ((pkt_size_min < MIN_FRAG_SIZE))
                {
                    DMSG(0,"MgenPattern::InitFromString(PERIODIC/POISSON) error: packet size must be greater than the minimum fragment size: %d.\n",MIN_FRAG_SIZE);
                    return false;
                }
		    } 
	        else 
            {
                // Typically operating systems limit UDP datagrams to 8192 byte payloads?
                if ((pkt_size_max > MAX_SIZE) || (pkt_size_min < MIN_SIZE))
                {
                    DMSG(0, "MgenPattern::InitFromString(PERIODIC/POISSON) error: invalid message size.\n");
                    return false;
                }
		    }
            break;
        }
        case JITTER:  // form "<aveRate> <pktSize> <jitterFraction>"
        {
            double aveRate, jitterFraction;
            interval_remainder = 0.0;
            char sizeText[257];
            if (3 != sscanf(string, "%lf %256s %lf", &aveRate, sizeText, &jitterFraction))
            {
                DMSG(0, "MgenPattern::InitFromString(JITTER) error: invalid parameters.\n");
                return false; 
            }
            // Look for colon delimiter to indicate variable packet size
            if (NULL != strchr(sizeText, ':'))
            {
                if (2 != sscanf(sizeText, "%u:%u", &pkt_size_min, &pkt_size_max))
                {
                    DMSG(0, "MgenPattern::InitFromString(JITTER) error: invalid  variable <size> parameter.\n");
                    return false;
                }
                else if (pkt_size_min > pkt_size_max)
                {
                    unsigned int temp = pkt_size_min;
                    pkt_size_min = pkt_size_max;
                    pkt_size_max = temp;
                }
            }
            else if (1 != sscanf(sizeText, "%u", &pkt_size_min))
            {
                DMSG(0, "MgenPattern::InitFromString(JITTER) error: invalid <size> parameter.\n");
                return false; 
            }        
            else
            {
                pkt_size_max = pkt_size_min;
            }   
            if ((jitterFraction < 0.0) || (jitterFraction > 0.5))
            {
                DMSG(0, "MgenPattern::InitFromString(JITTER) error: invalid jitter fraction.\n");
                return false; 
            }
            if (aveRate < 0.0)
            {
                if (-1.0 != aveRate)
                {
                    DMSG(0, "MgenPattern::InitFromString(JITTER) error: invalid packet rate.\n");
                    return false;
                }
                else
                {
                    interval_ave = 0.0;   
                }
            }
            else if (aveRate > 0.0)
            {
                interval_ave = 1.0 / aveRate;
            }
            else
            {
                interval_ave = -1.0;
                flowPaused = true;
            }
            if (interval_ave > 0.0)
            {
                jitter_min = interval_ave * (1.0 - jitterFraction);
                jitter_max = interval_ave * (1.0 + jitterFraction);
            }
            else
            {
                jitter_min = jitter_max = interval_ave;
            }
            if (UDP != protocol)
		    {
                // unlimited message size for non-UDP protocols
                if ((pkt_size_min < MIN_FRAG_SIZE))
                {
                    DMSG(0,"MgenPattern::InitFromString(JITTER) error: packet size must be greater than the minimum fragment size: %d.\n",MIN_FRAG_SIZE);
                    return false;
                }
		    } 
	        else 
            {
                // Typically operating systems limit UDP datagrams to 8192 byte payloads?
                if ((pkt_size_max > MAX_SIZE) || (pkt_size_min < MIN_SIZE))
                {
                    DMSG(0, "MgenPattern::InitFromString(JITTER) error: invalid message size.\n");
                    return false;
                }
		    }
            break;
        }

        case BURST: // form "<burstType> <aveInterval> <pattern> [<params>] 
                    //       <durationDistribution> <aveDuration>"
        {
            char fieldBuffer[Mgen::SCRIPT_LINE_MAX+1];
            const char* ptr = string;
            // Strip leading white space
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: missing burst type.\n");
                return false;
            }
            burst_type = GetBurstTypeFromString(fieldBuffer);
            if (INVALID_BURST == burst_type)
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: invalid burst type.\n");
                return false;
            } 
            ptr += strlen(fieldBuffer);   
            // Strip leading white space
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: missing burst interval.\n");
                return false;
            }   
            if (1 != sscanf(fieldBuffer, "%lf", &interval_ave))
            {
                 DMSG(0, "MgenPattern::InitFromString(BURST) error: invalid burst interval.\n");
                 return false;
            }   
            if (interval_ave < 0.0)
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: invalid burst interval.\n");
                return false;
            }
            ptr += strlen(fieldBuffer);   
           // Strip leading white space
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: missing burst pattern.\n");
                return false;
            }   
            Type patternType = GetTypeFromString(fieldBuffer); 
            ptr += strlen(fieldBuffer);
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            // Point to beginning of pattern parameters
            const char* pptr = ptr;
            // Find end of pattern parameter set
            unsigned int nested = 1;
            while (nested)
            {
                ptr++;
                if ('[' == *ptr) 
                    nested++;   
                else if (']' == *ptr)
                    nested--;
                if ('\0' == *ptr)
                {
                    DMSG(0, "MgenPattern::InitFromString(BURST) Error: non-terminated <patternParams>\n");
                    return false;
                }
            }
            ptr = strchr(ptr, ']');
            if ((*pptr != '[') || !ptr)
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) Error: missing <patternParams>\n");
                return false;   
            }
            if (!burst_pattern) 
            {
                if (!(burst_pattern = new MgenPattern()))
                {
                    DMSG(0, "MgenPattern::InitFromString(BURST) Error: pattern allocation: %s\n",
                            GetErrorString());
                    return false;
                }
            }
            strncpy(fieldBuffer, pptr+1, ptr - pptr - 1);
            fieldBuffer[ptr - pptr - 1] = '\0';
            if (!burst_pattern->InitFromString(patternType, fieldBuffer,protocol))
            {
                DMSG(0, "MgenPattern::InitFromString() Error: invalid <patternParams>\n");
                return false;    
            }
            // Set ptr to next field, skipping any white space
            ptr++;
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: missing burst duration type.\n");
                return false;
            }
            burst_duration_type = GetDurationTypeFromString(fieldBuffer);
            if (INVALID_DURATION == burst_duration_type)
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: invalid burst duration type.\n");
                return false;
            }
            ptr += strlen(fieldBuffer);
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            if (1 != sscanf(ptr, "%lf", &burst_duration_ave))
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: invalid burst duration.\n");
                return false;
            }    
            if (burst_duration_ave < 0.0)
            {
                DMSG(0, "MgenPattern::InitFromString(BURST) error: invalid burst duration.\n");
                return false;
            }
            switch (burst_duration_type)
            {
                case FIXED:
                    burst_duration = burst_duration_ave;
                    break;
                case EXPONENTIAL:
                    burst_duration = ExponentialRand(burst_duration_ave);
                    break;
                case INVALID_DURATION:
                    ASSERT(0);  // can't get here
                    break;   
            } 
            interval_remainder = burst_duration; 
            last_time.tv_sec = last_time.tv_usec = 0;
            break;
        }
#ifdef HAVE_PCAP
        case CLONE:
	  {
	    char fieldBuffer[Mgen::SCRIPT_LINE_MAX+1];
	    const char* ptr = string;
	    // String leading white space
	    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
	    if (1 != sscanf(ptr, "%s", fieldBuffer))
	      {
		DMSG(0,"MgenPattern::InitFromString(CLONE) error: missing file type.\n");
		return false;
	      }
	    file_type = GetFileTypeFromString(fieldBuffer);
	    if (INVALID_FILETYPE == file_type)
	      {
		DMSG(0,"MgenPattern::InitFromString(CLONE) error: invalid file type.\n");
		return false;
	      }
	    ptr += strlen(fieldBuffer);
	    // String leading white space
	    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
	    if (1 != sscanf(ptr,"%s", fieldBuffer))
	      {
		DMSG(0,"MgenPattern::InitFromString(CLONE) error: invalid file name.\n");
		return false;
	      }
	    strcpy(clone_fname,fieldBuffer);
	    ptr += strlen(fieldBuffer);
	    // Strip leading white sapce
	    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
	    // Point to beginning of pattern parameters
	    const char* pptr = ptr;
	    // Find end of pattern parameter set
	    unsigned int nested = 1;
	    while (nested)
            {
                ptr++;
                if ('[' == *ptr) 
                    nested++;   
                else if (']' == *ptr)
                    nested--;
                if ('\0' == *ptr)
                {
                    break;
                }
            }
            ptr = strchr(ptr, ']');
            if ((*pptr != '[') || !ptr)
            {
	      // no optional pattern
	      break;   
            }
	    pptr++;
	    if (1 != sscanf(pptr++, "%d", &repeat_count))
	      {
		DMSG(0,"MgenPattern::InitFromString(CLONE) error: invalid repeat count.\n");
	      }
	    break;
	  }
#endif //HAVE_PCAP
        case INVALID_TYPE:
            DMSG(0, "MgenPattern::InitFromString() unsupported pattern type\n");
            ASSERT(0);
            return false;        
    }  // end switch(type)
    return true;
}  // end MgenPattern::InitFromString()
#ifdef HAVE_PCAP
bool MgenPattern::OpenPcapDevice()
{
  char errbuf[PCAP_ERRBUF_SIZE+1];
  errbuf[0] = '\0';

  pcap_device = pcap_open_offline(clone_fname,errbuf);
  if (NULL == pcap_device)
    {
      DMSG(0,"MgenPattern::OpenPcapDevice() pcap_open_offline error: %s\n",errbuf);
      return false; 
    }
  if (0 != strlen(errbuf))
    DMSG(0,"MgenPattern::OpenPcapDevice() pcap_open_live() warning: %s\n",errbuf);
 
  return true;

} // end MgenPattern::OpenPcapDevice()

double MgenPattern::RestartPcapRead(double& prevTime)
{
    struct pcap_pkthdr header;
    const u_char *packet = NULL;

    if (!OpenPcapDevice())
        return -1; // open failed

    // Get interval between 1st & 2nd pkts (A).  Schedule 2nd packet
    // in file to be sent (A) seconds after the last pkt in the file
    if (pcap_next(pcap_device,&header))  // Get 1st pkt in file
    {
      // Save the time as a double for interval calculation
      prevTime = (header.ts.tv_sec * 1.0 + header.ts.tv_usec * 1.0e-6);

      packet = pcap_next(pcap_device,&header); // Get 2nd pkt in file
    }

    if (packet)
    {
      // force minimum mgen length.  (check for ipv6 ultimately)
      pkt_size_min = pkt_size_max = (header.len - 42) > 28 ? (header.len - 42) : 28;;

      // calculate interval (A)
      double interval = ((header.ts.tv_sec * 1.0 + header.ts.tv_usec * 1.0e-6) - prevTime);

      // Save the time as a double for interval calculation
      prevTime = (header.ts.tv_sec * 1.0 + header.ts.tv_usec * 1.0e-6);

      return interval;
    }
  return -1; // error

} // end MgenPattern::RestartPcapRead()
#endif // HAVE_PCAP

double MgenPattern::GetPktInterval()
{ 
  switch (type)
  {
    case PERIODIC: 
         return interval_ave; 
    case POISSON: 
         return (interval_ave > 0.0) ? ExponentialRand(interval_ave) : interval_ave; 
    case JITTER:
    {
         double pktInterval = UniformRand(jitter_min, jitter_max);  
         double result = pktInterval + interval_remainder;
         interval_remainder = (interval_ave - pktInterval); 
         return result;  
    }  
    case BURST:
    {
        // 
        double pktInterval = burst_pattern->GetPktInterval();
        if (pktInterval <= 0.0)
        {
            struct timeval currentTime;
            ProtoSystemTime(currentTime);
            if ((0 == last_time.tv_sec) && (0 == last_time.tv_usec)) last_time = currentTime;
            double deltaTime = (double)(currentTime.tv_sec - last_time.tv_sec);
            if (currentTime.tv_usec > last_time.tv_usec)
                deltaTime += 1.0e-06*(double)(currentTime.tv_usec - last_time.tv_usec);
            else
                deltaTime -= 1.0e-06*(double)(last_time.tv_usec - currentTime.tv_usec);
            if (interval_remainder > deltaTime)
            {
                last_time = currentTime;
                interval_remainder -= deltaTime;
                return pktInterval;   
            }
            last_time.tv_sec = last_time.tv_usec = 0;
        }
        else if (interval_remainder > pktInterval)
        {
            interval_remainder -= pktInterval;
            return pktInterval; 
        }
        // Prev burst has finished, schedule next burst
        double burstInterval = -1.0;
        switch (burst_type)
        {
            case REGULAR:
                burstInterval = interval_ave;
                break;
            case RANDOM:
                burstInterval = ExponentialRand(interval_ave);
                break;
            case INVALID_BURST:
                ASSERT(0);
                return burstInterval;   
        }
        if (burstInterval > burst_duration) 
            burstInterval = burstInterval - burst_duration + interval_remainder;
        else
            burstInterval = interval_remainder;
        if (burstInterval > pktInterval) pktInterval = burstInterval;
    
        // Now pick next burst duration.
        switch(burst_duration_type)
        {
            case FIXED:
                burst_duration = burst_duration_ave;
                break;
            case EXPONENTIAL:
                burst_duration = ExponentialRand(burst_duration_ave);
                break;
            case INVALID_DURATION:
                ASSERT(0);
                return -1.0;       
        }
        interval_remainder = burst_duration;
        return pktInterval;
    }
#ifdef HAVE_PCAP
    case CLONE:
      {
          // add switch on file type  ljt
          static double prevTime, interval = 0;
          static bool firstPacket;
          struct pcap_pkthdr header;
          const u_char *packet;
          
          if (pcap_device == NULL) // Initial file read
          {
              if (OpenPcapDevice()) 
              {
                  firstPacket = true;
                  // flow starting, pkt will be sent immediately
                  // regardless of interval returned...
                  interval = 1; 
              }
              else
                return -1; // File read failed
          }
          
          packet = pcap_next(pcap_device,&header);
          if (packet)
          {
              // force minimum mgen length.  (check for ipv6 ultimately)
              pkt_size_min = pkt_size_max = (header.len - 42) > 28 ? (header.len -42) : 28;;
              
              if (firstPacket)
                firstPacket = false;
              else
                interval = ((header.ts.tv_sec * 1.0 + header.ts.tv_usec * 1.0e-6) - prevTime);
              
              // Save the time as a double for interval calculation
              prevTime = (header.ts.tv_sec * 1.0 + header.ts.tv_usec * 1.0e-6);
              
              return interval;
          }
          else
          {
              if (!firstPacket && interval == 1)
              {
                  DMSG(0,"MgenPattern::GetPktInterval() Error: clone file must contain at least 2 records!\n");
                  pcap_close(pcap_device);
                  return -1;
              }
          }
          
          if (NULL == packet) // EOF
          {
              pcap_close(pcap_device);
              pcap_device = NULL;
              if (repeat_count)
              {
                  if (repeat_count > 0)
                    repeat_count--;
                  
                  return RestartPcapRead(prevTime);
                  
              }
          }
          
          return -1;
          break;
      }
#endif //HAVE_PCAP
    case INVALID_TYPE:
        ASSERT(0);
        break;
  }  // end switch(type)
  return -1.0;
}  // end MgenPattern::GetPktInterval()

unsigned int MgenPattern::GetPktSize()
{
    switch (type)
    {
        case PERIODIC:
        case POISSON:
        case JITTER:
            if (pkt_size_min != pkt_size_max)
                return UniformRandUnsigned(pkt_size_min, pkt_size_max); 
            else
                return pkt_size_min;
            break;
        
#ifdef HAVE_PCAP
        case CLONE:
#endif //HAVE_PCAP
            return pkt_size_min;
        case BURST:
            return burst_pattern->GetPktSize();
        case INVALID_TYPE:
            ASSERT(0);
            break;
    }
    return 0;
}  // end MgenPattern::GetPktSize()

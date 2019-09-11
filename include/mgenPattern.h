#ifndef _MGEN_PATTERN
#define _MGEN_PATTERN

#include <stdlib.h>    // for rand(), RAND_MAX
#include <math.h>      // for log()
#ifdef _HAVE_PCAP
#include <pcap.h>      // for CLONE tcpdump files
#include <limits.h>    // for CLONE file size
#endif //_HAVE_PCAP
#include "protoDefs.h" // to get proper struct timeval def
#include "mgenGlobals.h" // can't forward declare enum's
/**
 * @class StringMapper
 * @brief Helper class to build tables to map strings to values
*/
class StringMapper
{
    public:
        const char* string;
        int         key;
};  // end class StringMapper
/**
 * @class MgenPattern
 * @brief Defines an MgenFlow traffic pattern.
 */
class MgenPattern
{
    public:
        MgenPattern();
        ~MgenPattern();
#ifdef _HAVE_PCAP	
        enum Type {INVALID_TYPE, PERIODIC, POISSON, BURST, JITTER, CLONE};
	enum FileType {INVALID_FILETYPE, TCPDUMP};
	static const StringMapper CLONE_FILE_LIST[];
	static FileType GetFileTypeFromString(const char* string);
#else
        enum Type {INVALID_TYPE, PERIODIC, POISSON, BURST, JITTER};    
#endif
        static Type GetTypeFromString(const char* string);

        bool InitFromString(MgenPattern::Type theType, const char* string,Protocol protocol);
                
        double GetPktInterval();        
        double GetIntervalAve() {return interval_ave;}
        unsigned int GetPktSize();
        void InvalidatePattern() {type = INVALID_TYPE;};
	MgenPattern::Type GetType() { return type;};
	bool UnlimitedRate() {return unlimitedRate;}
#ifdef _HAVE_PCAP
	bool ReadingPcapFile() {return pcap_device;}
#endif
  private:
        static const StringMapper TYPE_LIST[]; 
        enum Burst {INVALID_BURST, REGULAR, RANDOM};  
        static const StringMapper BURST_LIST[];
        static Burst GetBurstTypeFromString(const char* string);
        enum Duration {INVALID_DURATION, FIXED, EXPONENTIAL};  
        static const StringMapper DURATION_LIST[];   
        static Duration GetDurationTypeFromString(const char* string);
        
        static double UniformRand(double min, double max)
        {
            double range = max - min;
            return (((((double)rand()) * range) / ((double)RAND_MAX)) + min); 
        }
         
        static double ExponentialRand(double mean)
        {
//           return (-(log(1.0 - ( ((double)rand())/((double)RAND_MAX)) )) * mean);
             return(-log(((double)rand())/((double)RAND_MAX))*mean);
        }
#ifdef _HAVE_PCAP	
        double RestartPcapRead(double &prevTime);
        bool OpenPcapDevice();
#endif
        Type            type;
        double          interval_ave;
        double          interval_remainder;
        unsigned int    pkt_size;
        double          jitter_min;  // = jitterFraction * interval_ave
        double          jitter_max;  // = interval_ave + jitterFraction
        Burst           burst_type;
        MgenPattern*    burst_pattern;
        Duration        burst_duration_type;
        double          burst_duration_ave;
        double          burst_duration;
        struct timeval  last_time;
	bool            unlimitedRate;

#ifdef _HAVE_PCAP
        pcap_t*         pcap_device;
        FileType        file_type; // clone file type
        char            clone_fname[PATH_MAX+NAME_MAX];
        int             repeat_count;
#endif //_HAVE_PCAP
};  // end class MgenPattern


#endif //_MGEN_PATTERN

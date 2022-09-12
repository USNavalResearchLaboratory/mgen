#include "mgenGlobals.h" // can't forward declare enum's
#include "mgenEvent.h"
#include "mgen.h"  // for Mgen::SCRIPT_LINE_MAX
#include "protoString.h"  // for ProtoTokenator

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>   // for toupper()

MgenBaseEvent::MgenBaseEvent(Category theCategory)
 : category(theCategory), event_time(-1.0),
   prev(NULL), next(NULL)
{
}

MgenEvent::MgenEvent()
 : MgenBaseEvent(MGEN), flow_id(0), event_type(INVALID_TYPE), src_port(0),
   payload(0), count(-1), keep_alive(true),
   protocol(INVALID_PROTOCOL), tos(0), ttl(255),
   retry_count(0), retry_delay(0),
   df(DF_DEFAULT), option_mask(0), queue(0), connect(false), 
   report_analytics(false), report_feedback(false), flow_status()
{
    interface_name[0] = '\0';
    flow_label = 0;
    option_mask = 0;
}

MgenEvent::~MgenEvent() {
	if (payload != NULL) delete [] payload;
}

const StringMapper MgenEvent::TYPE_LIST[] = 
{
    {"ON", ON},
    {"MOD", MOD},
    {"OFF", OFF},
    {"XXXX", INVALID_TYPE}   
};

MgenEvent::Type MgenEvent::GetTypeFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
    size_t len = strlen(string);
    len = len < 15 ? len : 15;
    unsigned int i;
    for (i =0 ; i < len; i++)
        upperString[i] = toupper(string[i]);
    upperString[i] = '\0';
    const StringMapper* m = TYPE_LIST;
    unsigned matchCount = 0;
    Type eventType = INVALID_TYPE;
    while (INVALID_TYPE != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {        
            matchCount++;
            eventType = ((Type)((*m).key));
        }
        m++;
    }
    if (matchCount > 1)
    {
        DMSG(0, "MgenEvent::GetTypeFromString() Error: ambiguous event type\n");
        return INVALID_TYPE;   
    }
    else
    {
        return eventType;
    }    
}  // end MgenEvent::GetTypeFromString()

const StringMapper MgenBaseEvent::PROTOCOL_LIST[] = 
{
    {"UDP",     UDP}, 
    {"TCP",     TCP},
    {"SINK",    SINK},
    {"XXX",     INVALID_PROTOCOL}   
};

Protocol MgenBaseEvent::GetProtocolFromString(const char* string)
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
    Protocol protocolType = INVALID_PROTOCOL;
    const StringMapper* m = PROTOCOL_LIST;
    while (INVALID_PROTOCOL != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            protocolType = ((Protocol)((*m).key));
            matchCount++;
        }
        m++;  
    }
    if (matchCount > 1)
    {
        DMSG(0, "Event::GetProtocolFromString() Error: ambiguous protocol\n");
        return INVALID_PROTOCOL;
    }
    else
    {
        return protocolType;
    }
}  // end MgenBaseEvent::GetProtocolFromString()

const char* MgenBaseEvent::GetStringFromProtocol(Protocol protocol)
{
    const StringMapper* m = PROTOCOL_LIST;
    while (INVALID_PROTOCOL != (*m).key)
    {
        if (protocol == (*m).key)
            return (*m).string;  
	m++;
    }
    return "UNKNOWN";
}  // end MgenBaseEvent::GetStringFromProtocol()

const StringMapper MgenEvent::OPTION_LIST[] = 
{
    {"PROTOCOL", PROTOCOL},
    {"DST", DST},
    {"SRC", SRC},
    {"PATTERN", PATTERN},
    {"BROADCAST", BROADCAST},
    {"TOS", TOS},
    {"RSVP", RSVP},
    {"INTERFACE", INTERFACE},
    {"TTL", TTL},
    {"DF",DF},
    {"SEQUENCE", SEQUENCE},
    {"LABEL",LABEL},
    {"TXBUFFER",TXBUFFER},
    {"DATA",DATA},
    {"COUNT",COUNT},
    {"QUEUE",QUEUE},
    {"CONNECT",CONNECT},
    {"REPORT", REPORT},
    {"FEEDBACK", FEEDBACK},
    {"SUSPEND", SUSPEND},
    {"RESUME", RESUME},
    {"RESET", RESET},
    {"RETRY", RETRY},
    {"PAUSE", PAUSE},
    {"RECONNECT", RECONNECT},
    {"XXXX", INVALID_OPTION}   
}; // end MgenEvent::OPTION_LIST

const unsigned int MgenEvent::ON_REQUIRED_OPTIONS = 
    (MgenEvent::PROTOCOL | MgenEvent::DST | MgenEvent::PATTERN);

MgenEvent::Option MgenEvent::GetOptionFromString(const char* string)
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
    Option optionType = INVALID_OPTION;
    const StringMapper* m = OPTION_LIST;
    while (INVALID_OPTION != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            optionType = ((Option)((*m).key));
            matchCount++;
        }
        m++;
    }
    if (matchCount > 1)
    {
        DMSG(0, "MgenEvent::GetOptionFromString() Error: ambiguous option\n");        
        return INVALID_OPTION;
    }
    else
    {
        return optionType;
    }
}  // end MgenEvent::GetOptionFromString()

const char* MgenEvent::GetStringFromOption(Option option)
{
    const StringMapper* m = OPTION_LIST;
    while (INVALID_OPTION != m->key)
    {
        if (option == (Option)(m->key))
            return m->string;
        else
            m++;  
    }
    return "INVALID";
} // end MgenEvent::GetStringFromOption()

bool MgenEvent::IsInternalCmd()
{
    // TODO: Create list of valid internal commands
    if ((OptionIsSet(MgenEvent::PAUSE)
         ||
         OptionIsSet(MgenEvent::RECONNECT)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * format: <time> <flowId> <type> [options ...]
 */
bool MgenEvent::InitFromString(const char* string)
{
    // format: <time> <flowId> <type> [options ...]
    const char* ptr = string;
    // Skip any leading white space
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    
    // 1) Get the first string (eventTime or eventType)
    char fieldBuffer[Mgen::SCRIPT_LINE_MAX];
    if (1 != sscanf(ptr, "%s", fieldBuffer))
    {
        DMSG(0, "MgenEvent::InitFromString() Error: empty string\n");
        return false;   
    }
    
    // Check for eventTime
    if (1 == sscanf(fieldBuffer, "%lf", &event_time))
    {
        // Read next field (eventType), skipping any white space
        ptr += strlen(fieldBuffer);
        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
        if (1 != sscanf(ptr, "%s", fieldBuffer))
        {
            DMSG(0, "MgenEvent::InitFromString() Error: missing <eventType>\n");
            return false;
        }
    }
    else
    {
        event_time = -1.0;  // an "immediate" event
    }
    
    // 2) Determine event type
    event_type = GetTypeFromString(fieldBuffer);
    if (INVALID_TYPE == event_type)
    {
        DMSG(0, "MgenEvent::InitFromString() Error: invalid <eventType>: \"%s\"\n",
             fieldBuffer);
        return false;   
    }
    // Point to next field, skipping any white space
    ptr += strlen(fieldBuffer);
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    
    // 3) parse flow id
    if (1 != sscanf(ptr, "%s", fieldBuffer))
    {
        DMSG(0, "MgenEvent::InitFromString() Error: missing <flowId>\n");
        return false;   
    }
    unsigned long flowId;
    if (1 != sscanf(fieldBuffer, "%lu", &flowId))
    {
        DMSG(0, "MgenEvent::InitFromString() Error: invalid <flowId>\n");
        return false;
    }
    flow_id = (UINT32)flowId;
    // Point to next field, skipping any white space
    ptr += strlen(fieldBuffer);
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;

    if (!flow_status.Init())
    {
        PLOG(PL_ERROR,"MgenEvent::InitFromString() error: unable to init flow_status!\n");
        return false;
    }
    flow_status.Clear();

    
    // Finally, iterate through any options
    option_mask = 0;
    while ('\0' != *ptr)
    {
        Option option = INVALID_OPTION;
        // Read option label
        if (1 != sscanf(ptr, "%s", fieldBuffer))
        {
            DMSG(0, "MgenEvent::InitFromString() Error: event option parse error\n");
            return false;   
        }
        // Point to next field, skipping any white space
        ptr += strlen(fieldBuffer);
        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
        
        // 1) Is it an implicitly labeled "PROTOCOL" type?
        if (INVALID_PROTOCOL != GetProtocolFromString(fieldBuffer)) 
          option = PROTOCOL;
        // 2) Is it an implicitly labeled "PATTERN" spec?
        else if (MgenPattern::INVALID_TYPE != MgenPattern::GetTypeFromString(fieldBuffer))
          option = PATTERN; 
        // 3) Or is it one of our explicitly labeled options?
        else
          option = GetOptionFromString(fieldBuffer);
        
        switch (option)
        {
        case PROTOCOL:  // protocol type (UDP or TCP)
          protocol = GetProtocolFromString(fieldBuffer);
          if (INVALID_PROTOCOL == protocol)
          {
              DMSG(0, "MgenEvent::InitFromString() Error: invalid protocol type\n");
              return false;    
          }

          break;
          
        case DST:       // destination address/port
          {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <dstAddr>\n");
                  return false;   
              }
              char* portPtr= strchr(fieldBuffer, '/');
              if (portPtr)
              {
                  *portPtr++ = '\0';
              }
              else
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <dstPort>\n");
                  return false;
              } 
              if (!dst_addr.ResolveFromString(fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <dstAddr>\n");
                  return false;   
              }
              int dstPort;
              if (1 != sscanf(portPtr, "%d", &dstPort))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <dstPort>\n");
                  return false; 
              }
              if ((dstPort < 1) || (dstPort > 65535))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <dstPort>\n");
                  return false; 
              }
              dst_addr.SetPort(dstPort);
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer) + strlen(portPtr) + 1;
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
          
        case SRC:       // source port
	      {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <srcPort>\n");
                  return false;   
              }
              int srcPort;
              if (1 != sscanf(fieldBuffer, "%d", &srcPort))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <srcPort>\n");
                  return false;
              }
              // we're allowing 0 src ports now,
              // will cause OS to randomly assign one.
              if (srcPort < 0 || srcPort > 65535)
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <srcPort>\n");
                  return false; 
              }
              src_port = srcPort;
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
	      }
	    case COUNT:
	      {
              if (1 != sscanf(ptr,"%s",fieldBuffer))
              {
                  DMSG(0,"MgenEvent::InitFromString() Error: missing <countValue>\n");
                  return false;
              }
              int countValue;
              if (1 != sscanf(fieldBuffer,"%i",&countValue))
              {
                  DMSG(0,"MgenEvent::InitFromString() Error: invalid <countValue>\n");
                  return false;
              }
              count = countValue;

              // Check for keep alive attribute

              char* keepAlivePtr = strchr(fieldBuffer, ',');
              if (keepAlivePtr)
              {
                 char keepAliveValue[Mgen::SCRIPT_LINE_MAX];
                 if (1 != sscanf(keepAlivePtr, "%s", keepAliveValue))
                 {
                     DMSG(0, "MgenEvent::InitFromString() Error: invalid keep alive value\n");
                     return false;
                 }
                 
                 if ((strcmp(keepAliveValue,",off"))
                     ||
                     (strcmp(keepAliveValue,",OFF")))
                 {
                     keep_alive = false;
                 }
              }
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
	      }
        case PATTERN:
          {
              MgenPattern::Type patternType 
                = MgenPattern::GetTypeFromString(fieldBuffer);
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
                      DMSG(0, "MgenEvent::InitFromString() Error: non-terminated <patternParams>\n");
                      return false;
                  }
              }
              if ((*pptr != '[') || !ptr)
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <patternParams>\n");
                  return false;   
              }
              strncpy(fieldBuffer, pptr+1, ptr - pptr - 1);
              fieldBuffer[ptr - pptr - 1] = '\0';
              if (!pattern.InitFromString(patternType, fieldBuffer,protocol))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <patternParams>\n");
                  return false;    
              }
              // Set ptr to next field, skipping any white space
              ptr++;
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }  
        case BROADCAST:  // broadcast on or off
        {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing {on|off}\n");
                  return false;   
              }
              bool broadcastValue;
              size_t len = strlen(fieldBuffer);
              unsigned int i;
              for (i = 0 ; i < len; i++)
                  fieldBuffer[i] = toupper(fieldBuffer[i]);
              fieldBuffer[i] = '\0';
              if(!strncmp("ON", fieldBuffer, len))
                  broadcastValue = true;
              else if(!strncmp("OFF", fieldBuffer, len))
                  broadcastValue = false;
              else
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: %s is neither on nor off\n", fieldBuffer);
                  return false;
              }
              broadcast = broadcastValue;
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
        case DF:  // df/fragmentation on or off
        {
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "MgenEvent::InitFromString() DF Error: missing {on|off}\n");
                return false;   
            }
            FragmentationStatus dfValue;
            size_t len = strlen(fieldBuffer);
            unsigned int i;
            for (i = 0 ; i < len; i++)
                fieldBuffer[i] = toupper(fieldBuffer[i]);
            fieldBuffer[i] = '\0';
            if(!strncmp("ON", fieldBuffer, len))
                dfValue = DF_ON;
            else if(!strncmp("OFF", fieldBuffer, len))
                dfValue = DF_OFF;
            else
            {
                DMSG(0, "MgenEvent::InitFromString() DF Error: %s is neither on nor off\n", fieldBuffer);
                return false;
            }
            df = dfValue;
            // Set ptr to next field, skipping any white space
            ptr += strlen(fieldBuffer);
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            break;
        } // df

        case TOS:       // tos spec
          {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <tosValue>\n");
                  return false;   
              }
              int tosValue;
              if (1 != sscanf(fieldBuffer, "%i", &tosValue))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <tosValue>\n");
                  return false;
              }
              if ((tosValue < 0) || (tosValue > 255))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <tosValue>\n");
                  return false;
              }
              tos = tosValue;
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
        case LABEL:     // IPv6 traffic class/ flow label
          {  
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <flowLabel>\n");
                  return false;   
              }
              int flowLabel; 
              if (1 != sscanf(fieldBuffer, "%i", &flowLabel))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <flowLabel>\n");
                  return false;
              }
              // (TBD) check validity
              flow_label = flowLabel; 
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
        case TXBUFFER:
          
          if (1 != sscanf(ptr, "%s", fieldBuffer))
          {
              DMSG(0, "MgenEvent::InitFromString() Error: missing <socket Tx  buffer size>\n");
              return false;   
          }
          unsigned int txBuffer; 
          
          if (1 != sscanf(fieldBuffer, "%u", &txBuffer))
          {
              DMSG(0, "MgenEvent::InitFromString() Error: invalid <socket Tx buffer size>\n");
              return false;
          }
          
          // (TBD) check validity
          tx_buffer_size = txBuffer; 
          // Set ptr to next field, skipping any white space
          ptr += strlen(fieldBuffer);
          while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
          
          break;
          
        case RSVP:
          // (TBD)
          DMSG(0, "MgenEvent::InitFromString() Error: RSVP option not yet supported\n");
          return false;
          break;
          
        case INTERFACE:  // multicast interface name
          if (1 != sscanf(ptr, "%s", fieldBuffer))
          {
              DMSG(0, "MgenEvent::InitFromString() Error: missing <interfaceName>\n");
              return false;   
          }
          strncpy(interface_name, fieldBuffer, 15);
          interface_name[15] = '\0';
          // Set ptr to next field, skipping any white space
          ptr += strlen(fieldBuffer);
          while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
          break;
          
        case TTL:     // ttl value
          {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: finding <ttlValue>\n");
                  return false;   
              }
              unsigned int ttlValue;
              if (1 != sscanf(fieldBuffer, "%u", &ttlValue))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <ttlValue>\n");
                  return false;
              }
              if (ttlValue > 255)
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <multicastTtlValue>\n");
                  return false;
              }
              ttl = ttlValue;
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
        case SEQUENCE:  // sequence number initialization
          {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: finding <seqNum>\n");
                  return false;   
              }
              unsigned long seqTemp;
              if (1 != sscanf(fieldBuffer, "%lu", &seqTemp))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <seqNum>\n");
                  return false;
              }
              sequence = (UINT32)seqTemp;
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
          
        case DATA:  // payload initialization
          {
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
                      DMSG(0, "MgenEvent::InitFromString() Error: non-terminated <data>\n");
                      return false;
                  }
              }
              if ((*pptr != '[') || !ptr)
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: missing <dataParams>\n");
                  return false;
              }
              strncpy(fieldBuffer, pptr+1, ptr - pptr - 1);
              fieldBuffer[ptr - pptr - 1] = '\0';
              if (payload != NULL) delete [] payload;
              payload = new char[strlen(fieldBuffer) + 1];
              strcpy(payload,fieldBuffer);
              ptr++;
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }
	    case QUEUE:
          {
              if (1 != sscanf(ptr,"%s",fieldBuffer))
              {
                  DMSG(0,"MgenEvent::InitFromString() Error: missing <queue limit>\n");
                  return false;
              }
              int queueLimit;
              if (1 != sscanf(fieldBuffer,"%d",&queueLimit))
              {
                  DMSG(0,"MgenEvent::InitFromString() Error: invalid <queue limit>\n");
                  return false;
              }
              queue = queueLimit;
              // Set ptr to next field, skipping any white space
              ptr += strlen(fieldBuffer);
              while (0 != isspace(*ptr)) ptr++;
              break;
          }
        case CONNECT:
          {
              connect = true;
              break;

          }
        case REPORT:
          {
              report_analytics = true;
              break;

          }
        case FEEDBACK:
          {
              report_analytics = true;
              report_feedback = true;
              break;

          }
        case PAUSE:
        case RECONNECT:
        {
            break;
        }
        case SUSPEND:
        case RESUME:
        case RESET:
        {
            if (1 != sscanf(ptr,"%s", fieldBuffer))
            {
                PLOG(PL_ERROR,"MgenEvent::InitFromString() error: missing command remote flowId list\n");
                return false;
            }
            // Use ProtoTokenator to parse flow id list of comma-delimited ranges and items
            ProtoTokenator tk1(fieldBuffer, ',');
            const char* item;
            while (NULL != (item = tk1.GetNextItem()))
            {
                ProtoTokenator tk2(item, '-');
                const char* flowIdText = tk2.GetNextItem();
                unsigned int startId = 0;
                if ((NULL == flowIdText) || (1 != sscanf(flowIdText, "%u", &startId)) ||
                    (0 == startId))
                {
                    PLOG(PL_ERROR, "MgenEvent::InitFromString() error: invalid command remote flowId list\n");
                    return false;
                }
                flowIdText = tk2.GetNextItem();
                unsigned int endId = 0;
                if (NULL == flowIdText)
                {
                    endId = startId;
                }
                else if ((1 != sscanf(flowIdText, "%u", &endId)) || (endId < startId))
                {
                    PLOG(PL_ERROR, "MgenEvent::InitFromString() error: invalid command remote flowId list\n");
                    return false;
                }
                MgenFlowCommand::Status status;
                switch (option)
                {
                    case SUSPEND:
                        status = MgenFlowCommand::FLOW_SUSPEND;
                        break;
                    case RESUME:
                        status = MgenFlowCommand::FLOW_RESUME;
                        break;
                    case RESET:
                        status = MgenFlowCommand::FLOW_RESET;
                        break;
                    default:
                        status = MgenFlowCommand::FLOW_UNCHANGED;  // won't occur
                        break;
                }
                flow_status.SetRange(startId, endId, status);
            }
          // Set ptr to next field, skipping any white space
          ptr += strlen(fieldBuffer);
          while (0 != isspace(*ptr)) ptr++;
          break;
        }
        case RETRY:     // tcp retry [<cnt>/[<delay>]]
          {
              if (1 != sscanf(ptr, "%s", fieldBuffer))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: finding <retryCountValue>\n");
                  return false;   
              }
              int retryCountValue;
              if (1 != sscanf(fieldBuffer, "%d", &retryCountValue))
              {
                  DMSG(0, "MgenEvent::InitFromString() Error: invalid <retryCountValue>\n");
                  return false;
              }
              retry_count = retryCountValue;
              char* delayPtr = strchr(fieldBuffer, '/');
              if (delayPtr)
              {
                 *delayPtr++ = '\0';
                 int retryDelayValue;
                 if (1 != sscanf(delayPtr, "%u", &retryDelayValue))
                 {
                     DMSG(0, "MgenEvent::InitFromString() Error: invalid retry delay value\n");
                     return false;
                 }
                 retry_delay = retryDelayValue;
                 ptr += strlen(fieldBuffer) + strlen(delayPtr) + 1;
              }
              else
              {
                  ptr += strlen(fieldBuffer);
              }
              // Set ptr to next field, skipping any white space
              while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
              break;
          }

        case INVALID_OPTION:
          DMSG(0, "MgenEvent::InitFromString() Error: invalid event option: \"%s\"\n",
               fieldBuffer);
          return false;
        }  // end switch (option)
        option_mask |= option;
    }  // end while ('\0' != *ptr)
    
    // Validate option_mask depending upon event_type
    switch (event_type)
    {
    case ON:
      {            
          unsigned int mask = option_mask & ON_REQUIRED_OPTIONS;
          if (mask != ON_REQUIRED_OPTIONS)
          {
              DMSG(0, "MgenEvent::InitFromString() Error: incomplete \"ON\" event:\n"
                   "          (missing");
              // Incomplete "ON" event.  What are we missing?
              // mask = ON_REQUIRED_OPTIONS - option_mask
              mask = ON_REQUIRED_OPTIONS & ~option_mask;
              unsigned int flag = 0x01;
              while (mask)
              {
                  if (0 != (flag & mask))
                  {
                      const char* optionString = GetStringFromOption((Option)flag);
                      mask &= ~flag;   
                      DMSG(0, ": %s ", optionString);
                  }   
                  flag <<= 1;
              }
              DMSG(0, ")\n");
              return false;   
          }
          // TBD check against ON_VALID_OPTIONS
          break;
      }
      
    case MOD:
      // TBD check against MOD_VALID_OPTIONS
      break;
      
    case OFF:   
      break;
      
    case INVALID_TYPE:
      ASSERT(0);  // this should never occur
      return false;
    }
    return true;
}  // end MgenEvent::InitFromString()


DrecEvent::DrecEvent()
 : MgenBaseEvent(DREC), event_type(INVALID_TYPE), protocol(INVALID_PROTOCOL),
   port_count(0), port_list(NULL), rx_buffer_size(0)
{
    interface_name[0] = '\0';
    group_addr.Invalidate();
    source_addr.Invalidate();
}

const StringMapper DrecEvent::TYPE_LIST[] = 
{
    {"JOIN",    JOIN},
    {"LEAVE",   LEAVE},
    {"LISTEN",  LISTEN},
    {"IGNORE",  IGNORE_},
    {"XXXX",    INVALID_TYPE}   
};

DrecEvent::Type DrecEvent::GetTypeFromString(const char* string)
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
    Type eventType = INVALID_TYPE;
    const StringMapper* m = TYPE_LIST;
    while (INVALID_TYPE != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            eventType = ((Type)((*m).key));
            matchCount++;
        }
        m++;  
    }
    if (matchCount > 1)
    {
        DMSG(0, "DrecEvent::GetTypeFromString() Error: ambiguous event type\n");
        return INVALID_TYPE;
    }
    else
    {
        return eventType;
    }
}  // end DrecEvent::GetTypeFromString()

const StringMapper DrecEvent::OPTION_LIST[] = 
{
    {"INTERFACE", INTERFACE},
    {"PORT",      PORT},
    {"RXBUFFER",  RXBUFFER},
    {"SRC",       SRC},
    {"XXXX",      INVALID_OPTION}  
};

DrecEvent::Option DrecEvent::GetOptionFromString(const char* string)
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
    Option optionType = INVALID_OPTION;
    const StringMapper* m = OPTION_LIST;
    while (INVALID_OPTION != (*m).key)
    {
        if (!strncmp(upperString, (*m).string, len))
        {
            optionType = ((Option)((*m).key));
            matchCount++;
        }
        m++;  
    }
    if (matchCount > 1)
    {
        DMSG(0, "DrecEvent::GetOptionFromString() Error: ambiguous option\n");
        return INVALID_OPTION;
    }
    else
    {
        return optionType;
    }
}  // end DrecEvent::GetOptionFromString()


bool DrecEvent::InitFromString(const char* string)
{
    // script lines are of format "<eventTime> <eventType> [params ...]"
    const char* ptr = string;
    // Skip any leading white space
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    
    // First, read first string
    char fieldBuffer[Mgen::SCRIPT_LINE_MAX];
    if (1 != sscanf(ptr, "%s", fieldBuffer))
    {
        DMSG(0, "DrecEvent::InitFromString() Error: empty string\n");
        return false;   
    }
    // Check for eventTime
    if (1 == sscanf(fieldBuffer, "%lf", &event_time))
    {
        // Read next field (eventType), skipping any white space
        ptr += strlen(fieldBuffer);
        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
        if (1 != sscanf(ptr, "%s", fieldBuffer))
        {
            DMSG(0, "DrecEvent::InitFromString() Error: missing <eventType>\n");
            return false;   
        }
    }
    else    
    {
        event_time = -1.0;  // an "immediate" event
    }
    
    event_type = GetTypeFromString(fieldBuffer);
    if (INVALID_TYPE == event_type)
    {
        DMSG(0, "DrecEvent::InitFromString() Error: invalid <eventType>\n");
        return false;   
    }
    // Point to next field, skipping any white space
    ptr += strlen(fieldBuffer);
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    
    switch(event_type)
    {
        case JOIN:    // "{JOIN|LEAVE} <groupAddr> [SRC <source-address>] [INTERFACE <interfaceName>][PORT <port>]"
        case LEAVE:
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "DrecEvent::InitFromString() Error: missing <groupAddress>\n");
                return false;
            }
            if (!group_addr.ResolveFromString(fieldBuffer) ||
                !group_addr.IsMulticast())
            {
                DMSG(0, "DrecEvent::InitFromString() Error: invalid <groupAddress>\n"); 
                return false; 
            }
            // Point to next field, skipping any white space
            ptr += strlen(fieldBuffer);
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            
            // Look for options [SRC <sourceAddress>] [INTERFACE <interfaceName>] or [PORT <port>/<portList>]
            while ('\0' != *ptr)
            {
                // Read option label
                if (1 != sscanf(ptr, "%s", fieldBuffer))
                {
                    DMSG(0, "MgenEvent::InitFromString() Error invalid JOIN/LEAVE option.\n");
                    return false;   
                }
                Option option = GetOptionFromString(fieldBuffer);
                // Point to next field, skipping any white space
                ptr += strlen(fieldBuffer);
                while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
                switch (option)
                {
                    case INTERFACE:
                        // Read <interfaceName>
                        if (1 != sscanf(ptr, "%s", fieldBuffer))
                        {
                            DMSG(0, "DrecEvent::InitFromString() Error: missing <interfaceName>\n");
                            return false;   
                        }
                        strncpy(interface_name, fieldBuffer, 15);
                        interface_name[15] = '\0';  // 16 char max
                        // Point to next field, skipping any white space
                        ptr += strlen(fieldBuffer);
                        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
                        break;
                        
                    case PORT:
                        // Read <portNumber>/<portList>
                        if (1 != sscanf(ptr, "%s", fieldBuffer))
                        {
                            DMSG(0, "DrecEvent::InitFromString() Error: missing <portNumber>/<portList>\n");
                            return false;   
                        }
                        // Parse port list and build array
                        if (!(port_list = CreatePortArray(fieldBuffer, &port_count)))
                        {
                            DMSG(0,"DrecEvent::InitFromString() Error: missing <port>/<portList>\n");
                            return false;
                        }
                        // Point to next field, skipping any white space
                        ptr += strlen(fieldBuffer);
                        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
                        break;

                    case SRC: // Source address for SSM

                        if (1 != sscanf(ptr, "%s", fieldBuffer))
                        {
                            DMSG( 0, "DrecEvent::InitFromString() Error: "
                                  "missing <sourceAddress>\n" );
                            return false;
                        }

                        // Source address must not be unspecified, multicast
                        // or a broadcast address
                        if (!source_addr.ResolveFromString(fieldBuffer) ||
                            source_addr.IsUnspecified() ||
                            source_addr.IsMulticast() ||
                            source_addr.IsBroadcast() )
                        {
                            DMSG( 0, "DrecEvent::InitFromString() Error: "
                                  "invalid <sourceAddress>\n" ); 
                            return false; 
                        }

                        // Compare type of source-address and group-address,
                        // both must be same..
                        if ( group_addr.GetType() != source_addr.GetType() ) {
                            DMSG( 0, "DrecEvent::InitFromString() Error: "
                                  "<groupAddress> and <sourceAddress> are "
                                  " not of same type\n" ); 
                            return false; 
                        }

                        // Point to next field, skipping any white space
                        ptr += strlen(fieldBuffer);
                        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            
                        break;



		    case RXBUFFER:
                        break; //RXBUFFER is not a leave option, however this case gets rid of a compiler warning.                     
                    case INVALID_OPTION:
                        DMSG(0, "DrecEvent::InitFromString() Error: invalid option: %s\n",
                                fieldBuffer);
                        return false;
                        break;
                }
            }  // end while ('\0' != *ptr)
            break;
            
        case LISTEN:  // "{LISTEN|IGNORE} <protocol> <portList>"
        case IGNORE_:  
            // Get <protocol>
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "DrecEvent::InitFromString() Error: missing <protocolType>\n");
                return false;
            }
            protocol = GetProtocolFromString(fieldBuffer);
            if (INVALID_PROTOCOL == protocol)
            {
                DMSG(0, "DrecEvent::InitFromString() Error: invalid <protocolType>\n");
                return false;
            }
            // Point to next field, skipping any white space
            ptr += strlen(fieldBuffer);
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
            // Get <portList>
            if (1 != sscanf(ptr, "%s", fieldBuffer))
            {
                DMSG(0, "DrecEvent::InitFromString() Error: missing <portList>\n");
                return false;
            }
            // Parse port list and build array
            if (!(port_list = CreatePortArray(fieldBuffer, &port_count)))
            {
                DMSG(0, "DrecEvent::InitFromString() Error: invalid <portList>\n");
                return false;
            }

            // Point to next field, skipping any white space
            ptr += strlen(fieldBuffer);
            while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;

            //Is rx socket buffer size specified?
            if (1 != sscanf(ptr, "%s", fieldBuffer))
                break;  // No Rx socket buffer size specified.

            if (!strcmp("RXBUFFER",fieldBuffer))
	        {
                if (1 != sscanf(ptr, "%s", fieldBuffer))
                {
                    DMSG(0, "MgenEvent::InitFromString() Error: missing <socket Rx  buffer size>\n");
                    return false;   
                }

                // Point to next field, skipping any white space
                ptr += strlen(fieldBuffer);
                while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;

                unsigned int rxBuffer; 
                if (1 != sscanf(ptr, "%u", &rxBuffer))
                { 
                    DMSG(0, "MgenEvent::InitFromString() Error: invalid <socket rx buffer size>\n");
                    return false;
                }

                // (TBD) check validity
                rx_buffer_size = rxBuffer;
	        }
            break;
        case INVALID_TYPE:
            ASSERT(0);  // this should never occur
            return false;
    }  // end switch(event_type)
    return true;
}  // end DrecEvent::InitFromString()


UINT16* DrecEvent::CreatePortArray(const char* portList,
                                   UINT16*     portCount)
{
    UINT16 count = 0;
    unsigned int arraySize = 0;
    UINT16* array = NULL; 
    const char* ptr = portList;
    while (*ptr != '\0')
    {
        // 1) Find and copy the next field
        const char* commaPtr = strchr(ptr, ',');
        long len;
        if (commaPtr)
        {
            len = commaPtr - ptr;
            commaPtr++; 
        }
        else
        {
            len = strlen(ptr);
            commaPtr = ptr + len;   
        }
        char fieldBuffer[32];
        len = len < 31 ? len : 31;
        strncpy(fieldBuffer, ptr, len);
        fieldBuffer[len] = '\0';
        ptr = commaPtr;
       
        // 2) Is this a value or a range?
        char* dashPtr = strchr(fieldBuffer, '-');
        if (dashPtr) *dashPtr++ = '\0';
            
        UINT16 lowValue;
        if (1 != sscanf(fieldBuffer, "%hu", &lowValue))
        {
            DMSG(0, "DrecEvent::CreatePortArray() Error: invalid <portList>\n");
            *portCount = 0;
            return (UINT16*)NULL;   
        }
        UINT16 highValue;
        if (dashPtr)
        {
            if (1 != sscanf(dashPtr, "%hu", &highValue))
            {
                DMSG(0, "DrecEvent::CreatePortArray() Error: invalid <portList> upper range bound\n");
                *portCount = 0;
                return (UINT16*)NULL;   
            }   
        }
        else
        {
            highValue = lowValue;   
        }
        if (highValue < lowValue)
        {
            DMSG(0, "DrecEvent::CreatePortArray() Error: invalid <portList> range\n");
            *portCount = 0;
            return (UINT16*)NULL;
        }
        UINT16 range = highValue - lowValue + 1;
        
        if (range > (arraySize - count))
        {
            arraySize += range;
            UINT16* newArray = new UINT16[arraySize];
            if (!newArray)
            {
                delete array;
                DMSG(0, "DrecEvent::CreatePortArray() Error: memory allocation error: %s\n",
                        GetErrorString());
                *portCount = 0;
                return (UINT16*)NULL; 
            }
            memcpy(newArray, array, count*sizeof(UINT16));
            delete array;
            array = newArray;
        }
        for (UINT16 p = lowValue; p <= highValue; p++)
            array[count++] = p;
    }  // end while (*ptr |= '\0')
    *portCount = count;
    return array;
}  // end DrecEvent::CreatePortArray()

MgenEvent::FlowStatus::FlowStatus()
{
}

MgenEvent::FlowStatus::~FlowStatus()
{
    status_lo.Destroy();
    status_hi.Destroy();
}

bool MgenEvent::FlowStatus::Init()
{
    if (!status_lo.Init(MAX_FLOW))
    {
        PLOG(PL_ERROR, "MgenEvent::FlowStatus::Init() error: %s\n", GetErrorString());
        return false;
    }
    if (!status_hi.Init(MAX_FLOW))
    {
        PLOG(PL_ERROR, "MgenEvent::FlowStatus::Init() error: %s\n", GetErrorString());
        status_lo.Destroy();
        return false;
    }
    return true;
}  // end MgenEvent::FlowStatus::Init()

void MgenEvent::FlowStatus::SetRange(UINT32 start, UINT32 end, MgenFlowCommand::Status status)
{
    if (0 != (0x01 & status))
        status_lo.SetBits(start - 1, end - start + 1);
    else
        status_lo.UnsetBits(start - 1, end - start + 1);
    if (0 != (0x02 & status))
        status_hi.SetBits(start - 1, end - start + 1);
    else
        status_hi.UnsetBits(start - 1, end - start + 1);
}  // end  MgenEvent::FlowStatus::SetRange()


MgenEventList::MgenEventList()
 : head(NULL), tail(NULL)
{
    
}

MgenEventList::~MgenEventList()
{
    Destroy();
}

void MgenEventList::Destroy()
{
    MgenBaseEvent* next = head;
    while (next)
    {
       MgenBaseEvent* current = next;
        next = next->next;
        delete current;   
    }   
    head = tail = NULL;
}  // end MgenEventList::Destroy()

/**
 * time-ordered insertion of event
 */
void  MgenEventList::Insert(MgenBaseEvent* theEvent)
{
    MgenBaseEvent* next = head;
    double eventTime = theEvent->GetTime();
    while(next)
    {
        if (eventTime < next->GetTime())
        {
            theEvent->next = next;
            if ((theEvent->prev = next->prev))
                theEvent->prev->next = theEvent;
            else
                head = theEvent;
            next->prev = theEvent;
            return;
        }
        else
        {
            next = next->next;
        }
    }
    // Fell to end of list
    if ((theEvent->prev = tail))
    {
        tail->next = theEvent;
    }
    else
    {
        head = theEvent;
        theEvent->prev = NULL;
    }
    tail = theEvent;
    theEvent->next = NULL;
}  // end MgenEventList::Insert()

/**
 * This places "theEvent" _before_ "nextEvent" in the list.
 * (If "nextEvent" is NULL, "theEvent" goes to the end of the list)
 */
void MgenEventList::Precede(MgenBaseEvent* nextEvent, 
                            MgenBaseEvent* theEvent)
{
    if (NULL != (theEvent->next = nextEvent))
    {
        if (NULL != (theEvent->prev = nextEvent->prev))
            theEvent->prev->next = theEvent;
        else
            head = theEvent;
        nextEvent->prev = theEvent;
    }
    else
    {
        if (NULL != (theEvent->prev = tail))
            tail->next = theEvent;
        else
            head = theEvent;
        tail = theEvent;   
    }
}  // end MgenEventList::Precede()

void MgenEventList::Remove(MgenBaseEvent* theEvent)
{
    if (theEvent->prev)
        theEvent->prev->next = theEvent->next;
    else
        head = theEvent->next;
    if (theEvent->next)
        theEvent->next->prev = theEvent->prev;
    else 
        tail = theEvent->prev;
}  // end MgenEventList::Remove()


/*********************************************************************
 *
 * AUTHORIZATION TO USE AND DISTRIBUTE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: 
 *
 * (1) source code distributions retain this paragraph in its entirety, 
 *  
 * (2) distributions including binary code include this paragraph in
 *     its entirety in the documentation or other materials provided 
 *     with the distribution, and 
 *
 * (3) all advertising materials mentioning features or use of this 
 *     software display the following acknowledgment:
 * 
 *      "This product includes software written and developed 
 *       by Brian Adamson and Joe Macker of the Naval Research 
 *       Laboratory (NRL)." 
 *         
 *  The name of NRL, the name(s) of NRL  employee(s), or any entity
 *  of the United States Government may not be used to endorse or
 *  promote  products derived from this software, nor does the 
 *  inclusion of the NRL written and developed software  directly or
 *  indirectly suggest NRL or United States  Government endorsement
 *  of this product.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ********************************************************************/

#include "mgen.h"
#include "mgenPayloadMgr.h"
#include "mgenPayloadMgrApp.h"

MgenPayloadMgr::MgenPayloadMgr(ProtoTimerMgr& timerMgr,
			       ProtoSocket::Notifier& socketNotifier,
			       Mgen& mgen)
  : timer_mgr(timerMgr),
    mgen(mgen),
    started(false),stopped(false),
    start_hour(0), start_min(0), start_sec(-1.0),
    start_gmt(false),start_time_lock(false),
    log_file(NULL), log_binary(false),local_time(false),log_flush(false),
    log_open(false),log_empty(true)
{
  SetSocketNotifier(&socketNotifier);

  // TBD: Create a timer to keep our app alive
  payload_timer.SetListener(this,&MgenPayloadMgr::OnTxTimeout);

}   

MgenPayloadMgr::~MgenPayloadMgr()
{
  Stop();
}

bool MgenPayloadMgr::OnTxTimeout(ProtoTimer& /*theTimer*/)
{
  DMSG(2,"MgenPayloadMgrApp::OnTxTimeout() 5 second heartbeat...\n");
  return true;
}

bool MgenPayloadMgr::Start()
{
  if (!mgen.Start())
    {
      DMSG(0,"MgenPayloadMgr::Start() Error: Error starting Mgen!\n");
    }

  // TODO: Heartbeat tbd
  payload_timer.SetInterval(5.0);
  payload_timer.SetRepeat(-1);
  OnTxTimeout(payload_timer);
  if (!payload_timer.IsActive()) timer_mgr.ActivateTimer(payload_timer);

  started = true;
  return true;
}
void MgenPayloadMgr::Stop()
{
  if (payload_timer.IsActive()) payload_timer.Deactivate();

  // TODO: cleanup
}

bool MgenPayloadMgr::SendMgenCommand(const char* cmd, const char* val)
{  

    if (!mgen.OnCommand(Mgen::GetCommandFromString(cmd),val,false))
	{
	  if (mgen.IsStarted())           
	    DMSG(0,"MgenPayloadMgr::SendMgenCommand() Error: processing mgen command: %s\n",val);
	  else
	    DMSG(0,"MgenPayloadMgr::SendMgenCommand() Error: processing mgen command: %s Mgen not started.\n",val);


	  return false;
	}
    return true;

} // end MgenPayloadMgr::SendMgenCommand

// This tells if the command is valid and whether args are expected
MgenPayloadMgr::CmdType MgenPayloadMgr::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    char upperCmd[32];  // all commands < 32 characters
    unsigned int len = strlen(cmd);
    len = len < 31 ? len : 31;
    unsigned int i;
    for (i = 0; i < 31; i++)
        upperCmd[i] = toupper(cmd[i]);
    
    bool matched = false;
    const StringMapper* m = COMMAND_LIST;
    CmdType type = CMD_INVALID;
    while (INVALID_COMMAND != (*m).key)
    {
        if (!strncmp(upperCmd, (*m).string+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == (*m).string[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        m++;
    }
    return type; 

} // end MgenPayloadMgr::GetCmdType()

// Global command processing
const StringMapper MgenPayloadMgr::COMMAND_LIST[] =
{
  // Mgen global commands:
  {"+START",                  START},
  {"+EVENT",                  EVENT},
  {"+INPUT",                  INPUT},
  {"+MPMLOG",                 MPMLOG},
  {"+OVERWRITE_MPMLOG",       OVERWRITE_MPMLOG},
  {"+MGENLOG",                MGENLOG},
  {"+OVERWRITE_MGENLOG",      OVERWRITE_MGENLOG},
  {"+DEBUG",                  DEBUG},
  {"-TXLOG",	              TXLOG},
  {"-LOCALTIME",              LOCALTIME},
  {"-NOLOG",                  NOLOG},
  {"-BINARY",                 BINARY},
  {"-FLUSH",                  FLUSH},
  {"+LABEL",      LABEL},
  {"+RXBUFFER",   RXBUFFER},
  {"+TXBUFFER",   TXBUFFER},
  {"+TOS",        TOS},
  {"+TTL",        TTL},
  {"+INTERFACE",  INTERFACE},
  {"-CHECKSUM",   CHECKSUM},
  {"-TXCHECKSUM", TXCHECKSUM},
  {"-RXCHECKSUM", RXCHECKSUM}, 
  {"+SAVE",       SAVE},
  {"+OFF",        INVALID_COMMAND},  // to deconflict "offset" from "off" event
  {NULL,          INVALID_COMMAND}   
};


MgenPayloadMgr::Command MgenPayloadMgr::GetCommandFromString(const char* string)
{
    // Make comparison case-insensitive
    char upperString[16];
    unsigned int len = strlen(string);

    len = len < 16 ? len : 16;
    
    for (unsigned int i = 0 ; i < len; i++)
        upperString[i] = toupper(string[i]);
    const StringMapper* m = COMMAND_LIST;
    MgenPayloadMgr::Command theCommand = INVALID_COMMAND;
    while (NULL != (*m).string)
    {
      if (!strncmp(upperString, (*m).string+1, len)) {
	theCommand = ((Command)((*m).key));
      }
      m++;
    }
    return theCommand;
}  // end MgenPayloadMgr::GetCommandFromString()

bool MgenPayloadMgr::OnCommand(MgenPayloadMgr::Command cmd, const char* arg,bool override)
{
  DMSG(0,"Command %s\n",arg);

    switch (cmd)
    {
    case START:
      {
          if (!arg)
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: missing <startTime> \n");
              return false;
              
          }
          if (override || !start_time_lock)
          {
              // convert to upper case for case-insensitivity
              // search for "GMT" or "NOW" keywords
              
              char temp[32];
              unsigned int len = strlen(arg);
              len = len < 31 ? len : 31;
              unsigned int i;
              for (i = 0 ; i < len; i++)
                temp[i] = toupper(arg[i]);
              temp[i] = '\0';
              
              unsigned int startHour, startMin;
              double startSec;
              // arg should be "hr:min:sec[GMT]" or "NOW"
              if (3 == sscanf(temp, "%u:%u:%lf", &startHour, &startMin, &startSec))
              {
                  start_hour = startHour;
                  start_min = startMin;
                  start_sec = startSec;
                  if (strstr(temp, "GMT"))
                    start_gmt = true;
                  else
                    start_gmt = false;
              }
              else
              {
                  // Check for "NOW" keywork (case-insensitive)
                  if (strstr(temp, "NOW"))
                  {
                      // negative start_time_sec indicates immediate start
                      start_sec = -1.0; 
                  }
                  else
                  {
                      DMSG(0, "MgenPayloadMgr::OnCommand() Error: invalid START time\n");
                      return false;
                  }
              }
              start_time_lock = override; //record START command precedence
          } // end if (override || !start_time_lock)
          break;
      } // end case START
      
    case EVENT:
      {
          if (!mgen.ParseEvent(arg,0,false))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: error parsing event\n");
              return false;   
          }
          break;
      }
    case INPUT:
      {
	if (!ParseScript(arg))
	  {
	     DMSG(0,"MgenPayloadMgr::OnCommand() Error: error parsing script\n");
	     return false;
	 }
	break;
      }
    case MPMLOG:
      {
	if (!OpenLog(arg,true,false))
	  {
	    DMSG(0,"MgenPayloadMgr::OnCommand() Error: mgenPayloadMgr log file open error: %s\n", strerror(errno));
	    return false;
	  }
          break;
      }
    case OVERWRITE_MPMLOG:
      {
	if (!OpenLog(arg,false,false))
	  {
	    DMSG(0,"MgenPayloadMgr::OnCommand() Error: rapr log file open error: %s\n", strerror(errno));
	    return false;
	  }
	break;
      }
    case MGENLOG:
      {
          char tmpOutput[512];
          if (1 != sscanf(arg,"%s",tmpOutput))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: Log file name required\n");
              return false;
          }
          char buffer[8192];
          sprintf(buffer," LOG %s",tmpOutput);
          SendMgenCommand("event",buffer);
          break;
      }
    case OVERWRITE_MGENLOG:
      {
          char tmpOutput[512];
          if (1 != sscanf(arg,"%s",tmpOutput))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: Log file name required\n");
              return false;
          }
          char buffer[8192];
          sprintf(buffer," OUTPUT %s",tmpOutput);
          SendMgenCommand("event",buffer);
          break;
      }
    case SAVE:
      {
          char tmpOutput[512];
          if (1 != sscanf(arg,"%s",tmpOutput))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: Log file name required\n");
              return false;
          }
          char buffer[8192];
          sprintf(buffer," SAVE %s",tmpOutput);
          SendMgenCommand("event",buffer);
          break;
      }
    case DEBUG:
      {
          SetDebugLevel(atoi(arg));
          
          break;
      }
    case TXLOG:
      {
          char buffer[8192];
          sprintf(buffer," TXLOG");
          SendMgenCommand("event",buffer);
          break;
      }
    case LOCALTIME:
      {
          char buffer [8192];
          sprintf(buffer," LOCALTIME");
	  local_time = true;
          SendMgenCommand("LOCALTIME",buffer);
          break;
      }
    case NOLOG:
      {
          char buffer[8192];
          sprintf(buffer," NOLOG");
          SendMgenCommand("event",buffer);
          break;
      }
    case BINARY:
      {
          char buffer[8192];
          sprintf(buffer," BINARY");
          SendMgenCommand("event",buffer);
          break;
      }
    case FLUSH:
      {
          // Set RAPR log file to flush
          log_flush = true;
          
          // Set MGEN log file to flush
          char buffer[8192];
          sprintf(buffer," FLUSH");
          SendMgenCommand("event",buffer);
          break;
      }
    case LABEL:
      {
          DMSG(0,"MgenPayloadMgr::OnCommand() Error: Label command not implemented.\n");
          return false;
          
      }
    case RXBUFFER:
      {
          unsigned int sizeTemp;
          if (1 != sscanf(arg,"%u",&sizeTemp))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: invalid rx buffer size\n");
              return false;
          }
          char buffer [8192];
          sprintf(buffer," RXBUFFER %d",sizeTemp);
          SendMgenCommand("event",buffer);
          break;
      }
    case TXBUFFER:
      {
          unsigned int sizeTemp;
          if (1 != sscanf(arg,"%u",&sizeTemp))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: invalid tx buffer size\n");
              return false;
          }
          char buffer [8192];
          sprintf(buffer," TXBUFFER %d",sizeTemp);
          SendMgenCommand("event",buffer);
          break;
      }
    case TOS:
      {
          int tosTemp;
          int result = sscanf(arg,"%i",&tosTemp);
          if ((1 != result) || (tosTemp < 0) || (tosTemp > 255))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: invalid tos value\n");
              return false;
          }
          char buffer [8192];
          sprintf(buffer," TOS %i",tosTemp);
          SendMgenCommand("event",buffer);
          break;
      }
    case TTL:
      {
          int ttlTemp;
          int result = sscanf(arg,"%i",&ttlTemp);
          if ((1 != result) || (ttlTemp < 0) || (ttlTemp > 255))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: invalid ttl value\n");
              return false;
          }
          char buffer[8192];
          sprintf(buffer," TTL %i",ttlTemp);
          SendMgenCommand("event",buffer);
          break;
      }
    case INTERFACE:
      {
          char tmpOutput[512];
          if (1 != sscanf(arg,"%s",tmpOutput))
          {
              DMSG(0,"MgenPayloadMgr::OnCommand() Error: multicast interface name required\n");
              return false;
          }
          
          char buffer[8192];
          sprintf(buffer," INTERFACE %s",tmpOutput);
          SendMgenCommand("event",buffer);
          break;
      }
    case CHECKSUM:
      {
          char buffer[8192];
          sprintf(buffer," CHECKSUM");
          SendMgenCommand("event",buffer);
          break;
      }
    case TXCHECKSUM:
      {
          char buffer[8192];
          sprintf(buffer," TXCHECKSUM");
          SendMgenCommand("event",buffer);
          break;
      }
    case RXCHECKSUM:
      {
          char buffer[8192];
          sprintf(buffer," RXCHECKSUM");
          SendMgenCommand("event",buffer);
          break;
      }
    case MGENEVENT:
      {
          DMSG(0,"MgenPayloadMgr::OnCommand() Error: Keyword is a raprApp keyword.  Program error?\n");
          return false;
      }
    case INVALID_COMMAND:
      {
          DMSG(0,"MgenPayloadMgr::OnCommand() Error: invalid command\n");
          return false;
      }
    } // end switch(cmd)
    return true;
}  // end MgenPayloadMgr::OnCommand

void MgenPayloadMgr::LogEvent(const char* cmd,const char* val)
{
  if (NULL == log_file) return;

  if (log_file)
    {
      struct timeval currentTime;
      ProtoSystemTime(currentTime);
      struct tm* timePtr;
      if (local_time)
	timePtr = localtime((time_t*)&currentTime.tv_sec);
      else
	timePtr = gmtime((time_t*)&currentTime.tv_sec);
      
      fprintf(log_file,"%02d:%02d:%02d.%06lu app>%s %s\n",
	      timePtr->tm_hour,timePtr->tm_min,
	      timePtr->tm_sec,(unsigned long) currentTime.tv_usec,
	      cmd,val);
      if (log_flush) fflush(log_file);
    }
}
bool MgenPayloadMgr::OpenLog(const char* path, bool append, bool binary)
{
    CloseLog();
    if (append)
    {
        struct stat buf;
        if (stat(path, &buf))   // zero return value == success
          log_empty = true;  // error -- assume file is empty
        else if (buf.st_size == 0)
          log_empty = true;
        else
          log_empty = false;
    }
    else
    {
        log_empty = true;   
    }
    const char* mode;
    if (binary)
    {
        mode = append ? "ab" : "wb+";
        log_binary = true;
    }
    else
    {
        mode = append ? "a" : "w+";
        log_binary = false;
    }
    if (!(log_file = fopen(path, mode)))
    {
        DMSG(0, "MgenPayloadMgr::OpenLog() fopen() Error: %s\n", strerror(errno));   
        return false;    
    }   
    log_open = true;
    return true;
}  // end MgenPayloadMgr::OpenLog()

void MgenPayloadMgr::SetLogFile(FILE* filePtr)
{
    CloseLog();
    log_file = filePtr;

}  // end MgenPayloadMgr::SetLogFile()

void MgenPayloadMgr::CloseLog()
{
    if (log_file)
    {
        if ((stdout != log_file) && (stderr != log_file))
            fclose(log_file);
        log_file = NULL;
    }
}  // end MgenPayloadMgr::CloseLog()

bool MgenPayloadMgr::ParseEvent(const char* lineBuffer,unsigned int lineCount)
{
    const char * ptr = lineBuffer;
    if (!ptr) return true;
    
    lineBuffer = ptr;
    
    // Strip leading white space
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    // Check for comment line (leading '#')
    if ('#' == *ptr) return true;
    char fieldBuffer[SCRIPT_LINE_MAX+1];
    // Script lines are in form {<globalCmd>|<eventTime>} ...
    if (1 != sscanf(ptr, "%s", fieldBuffer))
    {
        // Blank line?
        return true;   
    }
    // Set ptr to next field in line, stripping white space
    ptr += strlen(fieldBuffer);
    while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;
    
    Command cmd = GetCommandFromString(fieldBuffer);
    if (EVENT == cmd)
    {
        // read in <eventTime> or <eventType>
        if (1 != sscanf(ptr, "%s", fieldBuffer))
        {
	  DMSG(0, "MgenPayloadMgr::ParseEvent() Error: empty EVENT command %s\n",fieldBuffer);
            return false;    
        }
        // Set ptr to next field in line, stripping white space
        ptr += strlen(fieldBuffer);
        while ((' ' == *ptr) || ('\t' == *ptr)) ptr++;          
    }

    // If it's not a "global command", assume it's an event.
    if (INVALID_COMMAND == cmd)
    {
        cmd = EVENT;
    }
    
    switch (cmd)
    {
    case EVENT:
      {
	DMSG(0,"MgenPayloadMgr::ParseEvent()");

	if (!mgen.ParseEvent(lineBuffer,lineCount,false))
	  {
	    DMSG(0,"MgenPaylogMgr::ParseEvent() mgen.ParseEvent error %s %d",lineBuffer,lineCount);
	  }
          break;
      } // end case EVENT
    default:
      // Is it a global command
      if (INVALID_COMMAND != cmd)
      {
          if (!OnCommand(cmd,ptr))
          {
              DMSG(0,"MgenPayloadMgr::ParseEvent() Error: Bad global command: %s\n",lineBuffer);
              return false;
          }
      } 
      else
      {
          DMSG(0,"MgenPayloadMgr::ParseEvent() Error: invalid command: %s\n",lineBuffer);
          return false;
      }
      break;
    }
    
    return true;
} // end MgenPayloadMgr::ParseEvent()

// Parse a MgenPayloadMgr script
bool MgenPayloadMgr::ParseScript(const char* path)
{
    // Open script file
    FILE* scriptFile = fopen(path,"r");
    if (!scriptFile)
    {
        DMSG(0,"MgenPayloadMgr::ParseScript() fopen() Error: %s\n",strerror(errno));
        return false;
    }
    
    // Read script file line by line using FastReader
    FastReader reader;
    unsigned int lineCount = 0;
    unsigned int lines = 0;
    while (1)
    {
        lineCount += lines;  // for grouped (continued) lines
        char lineBuffer[SCRIPT_LINE_MAX+1];
        unsigned int len = SCRIPT_LINE_MAX;
        switch (reader.ReadlineContinue(scriptFile, lineBuffer, &len, &lines))
        {
        case FastReader::OK:
          lineCount++;
          lines--;
          break;
        case FastReader::DONE:
          fclose(scriptFile);
          return true; // done with script file
        case FastReader::ERROR_:
          DMSG(0, "MgenPayloadMgr::ParseScript() Error: script file read error\n");
          fclose(scriptFile);
          return false;
        }
        
        // check for comment line (leading '#')
        if ('#' == *lineBuffer) continue;
        
	if (!ParseEvent(lineBuffer, lineCount))
	  {
	    DMSG(0, "MgenPayloadMgr::ParseScript() Error: invalid script line: %lu\n", lineCount);
	    fclose(scriptFile);
	    return false;   
	  }
    }  // end while (1)
    return true;
    
} // end MgenPayloadMgr::ParseScript

////////////////////////////////////////////////////////////////
// MgenPayloadMgr::FastReader implementation

MgenPayloadMgr::FastReader::FastReader()
  : savecount(0)
{
    
}

MgenPayloadMgr::FastReader::Result MgenPayloadMgr::FastReader::Read(FILE*           filePtr, 
                                                char*           buffer, 
                                                unsigned int*   len)
{
    unsigned int want = *len;   
    if (savecount)
    {
        unsigned int ncopy = want < savecount ? want : savecount;
        memcpy(buffer, saveptr, ncopy);
        savecount -= ncopy;
        saveptr += ncopy;
        buffer += ncopy;
        want -= ncopy;
    }
    while (want)
    {
        unsigned int result = fread(savebuf, sizeof(char), BUFSIZE, filePtr);
        if (result)
        {
            unsigned int ncopy= want < result ? want : result;
            memcpy(buffer, savebuf, ncopy);
            savecount = result - ncopy;
            saveptr = savebuf + ncopy;
            buffer += ncopy;
            want -= ncopy;
        }
        else  // end-of-file
        {
            if (ferror(filePtr))
            {
                if (EINTR == errno) continue;   
            }
            *len -= want;
            if (*len)
              return OK;  // we read at least something
            else
              return DONE; // we read nothing
        }
    }  // end while(want)
    return OK;
}  // end MgenPayloadMgr::FastReader::Read()

// An OK text readline() routine (reads what will fit into buffer incl. NULL termination)
// if *len is unchanged on return, it means the line is bigger than the buffer and 
// requires multiple reads


MgenPayloadMgr::FastReader::Result MgenPayloadMgr::FastReader::Readline(FILE*         filePtr, 
                                                    char*         buffer, 
                                                    unsigned int* len)
{   
    unsigned int count = 0;
    unsigned int length = *len;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int one = 1;
        switch (Read(filePtr, ptr, &one))
        {
        case OK:
          if (('\n' == *ptr) || ('\r' == *ptr))
          {
              *ptr = '\0';
              *len = count;
              return OK;
          }
          count++;
          ptr++;
          break;
          
        case ERROR_:
          return ERROR_;
          
        case DONE:
          return DONE;
        }
    }
    // We've filled up the buffer provided with no end-of-line 
    return ERROR_;
}  // end MgenPayloadMgr::FastReader::Readline()

// This reads a line with possible ending '\' continuation character.
// Such "continued" lines are returned as one line with this function
//  and the "lineCount" argument indicates the actual number of lines
//  which comprise the long "continued" line.
//
// (Note: Lines ending with an even number '\\' are considered ending 
//        with one less '\' instead of continuing.  So, to actually 
//        end a line with an even number of '\\', continue it with 
//        an extra '\' and follow it with a blank line.)

MgenPayloadMgr::FastReader::Result MgenPayloadMgr::FastReader::ReadlineContinue(FILE*         filePtr, 
                                                            char*         buffer, 
                                                            unsigned int* len,
                                                            unsigned int* lineCount)
{   
    unsigned int lines = 0;
    unsigned int count = 0;
    unsigned int length = *len;
    char* ptr = buffer;
    while (count < length)
    {
        unsigned int space = length - count;
        switch (Readline(filePtr, ptr, &space))
        {
        case OK:
          {
              lines++;
              // 1) Doesn't the line continue?
              char* cptr = space ? ptr + space - 1 : ptr;
              // a) skip trailing white space
              while (((' ' == *cptr) ||  ('\t' == *cptr)) && (cptr > ptr)) 
              {
                  space--;
                  cptr--;
              }
              
              // If line "continues" to a blank line, skip it
              if ((cptr == ptr) && ((*cptr == '\0') || isspace(*cptr))) 
                continue;
              
              if ('\\' == *cptr)
              {
                  // Make sure line ends with odd number of '\' to continue
                  bool lineContinues = false;
                  while ((cptr >= ptr) && ('\\' == *cptr))
                  {
                      cptr--;
                      lineContinues = lineContinues ? false : true;    
                  }  
                  // lose trailing '\' (continuation or extra '\' char)
                  *(ptr+space-1) = '\0';
                  space -= 1;
                  if (lineContinues)
                  {
                      // get next line to continue
                      count += space;
                      ptr += space;
                      continue;
                  }
              }
              *len = count + space;
              if (lineCount) *lineCount = lines;
              return OK;  
              break;
          }
          
        case ERROR_:
          return ERROR_;
          
        case DONE:
          return DONE;
        }
    }
    // We've filled up the buffer provided with no end-of-line 
    return ERROR_;
}  // end MgenPayloadMgr::FastReader::Readline()

/* End MgenPayloadMgr::FastReader Implementation */

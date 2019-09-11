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

#ifndef _MGEN_GLOBALS
#define _MGEN_GLOBALS
enum LogEventType
  {
    INVALID_EVENT = 0,
    RECV_EVENT,
    RERR_EVENT,
    SEND_EVENT,
    LISTEN_EVENT,
    IGNORE_EVENT,
    JOIN_EVENT,
    LEAVE_EVENT,
    START_EVENT,
    STOP_EVENT,
    ON_EVENT,
    ACCEPT_EVENT,
    DISCONNECT_EVENT,
    CONNECT_EVENT,
    OFF_EVENT,
    SHUTDOWN_EVENT

  };

/**
 * Possible protocol types 
 */
enum Protocol
  {
    INVALID_PROTOCOL,
    UDP,
    TCP,
    SINK
  }; 

enum 
  {
    MIN_SIZE = 28,
    MAX_SIZE = 8192,
    MSG_LEN_SIZE = 2,
    // TX_BUFFER_SIZE is the tcp tx buffer size, for now same as udp max_size
    TX_BUFFER_SIZE = 8192,
    MAX_FRAG_SIZE = 65535, // TCP max fragment size
    MIN_FRAG_SIZE = 76     // ljt what should this be? 
                           // we're going with IPV6 + gps max for now
  };

#endif // _MGEN_GLOBALS

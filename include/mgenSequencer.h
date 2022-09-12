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

#ifndef _MGEN_SEQUENCER
#define _MGEN_SEQUENCER

#include <stdio.h>
#include <time.h>
#include <string.h>

/**
 * @class MgenSequencer
 *
 * @brief Maintains mgen message sequence numbers.
 */
class MgenSequencer {
   public:
      static void Initialize();
      static void SetSequence(int inFlow, int Seq);
      static int GetSequence(int inFlow);
      static int GetNextSequence(int inFlow);
      static int GetPrevSequence(int inFlow);
      static void FileOutput(FILE *inStream);
      static void FileInput(FILE *inStream);
      static bool Compare(struct timeval time1,struct timeval time2);
      static const int OK_FLAG = 0xF0F0;
   private:
      static int *seq_space;
      static struct timeval last_seq_time;
      static int size;
      static void resize(int inSize);
};

#endif //_MGEN_SEQUENCER


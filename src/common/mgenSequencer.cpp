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
 
#include "mgenSequencer.h"
#include "protoDefs.h" // to get proper struct timeval def

int *MgenSequencer::seq_space = new int[10];
struct timeval MgenSequencer::last_seq_time;
int MgenSequencer::size = 10;

//Initialize variables
void MgenSequencer::Initialize() {
    for (int i=0;i < size;i++) {
        *(seq_space + i) = 0;
    }
    last_seq_time.tv_sec = 0;
    last_seq_time.tv_usec = 0;
}

/**
 * Set the sequence for a particular flow id
 */
void MgenSequencer::SetSequence(int inFlow, int Seq) {
    resize(inFlow);
    *(seq_space+inFlow) = Seq;
}

/** 
 * Get the sequence number of a flow id
 */
int MgenSequencer::GetSequence(int inFlow) {
    int ret = 0;
    resize(inFlow);
    ret = *(seq_space+inFlow);
    return ret;
}

/**
 * Increment the sequence number and return it
 */
int MgenSequencer::GetNextSequence(int inFlow) {
    int ret = 0;
    resize(inFlow);
    ret = ++seq_space[inFlow];
    return ret;
}

/**
 * Decrement the sequence number and return it
 */
int MgenSequencer::GetPrevSequence(int inFlow) {
    int ret = 0;
    resize(inFlow);
    ret = --*(seq_space+inFlow);
    return ret;
}

/**
 * Write binary data to the file
 */
void MgenSequencer::FileOutput(FILE *inStream) {
    struct timeval time1;
    ProtoSystemTime(time1);
    int flag;
    flag = OK_FLAG;
    if (inStream == NULL) return;
    //timestamp
    fwrite((void *)&time1,sizeof(struct timeval),1,inStream);
    //size
    fwrite((void *)&size,sizeof(int),1,inStream);
    //data
    fwrite((void *)seq_space,sizeof(int),size,inStream);
    //validation flag
    fwrite((void *)&flag,sizeof(int),1,inStream);
}

/**
 * Read previous journal file back into memory
 */
void MgenSequencer::FileInput(FILE *inStream) {
    int *tmp;
    int tmpSize;
    struct timeval time1;
    if (inStream == NULL) return;
    //read the timestamp
    fread((void *)&time1,sizeof(struct timeval),1,inStream);
    //read the size
    fread((void *)&tmpSize,sizeof(int),1,inStream);
    tmp = new int[tmpSize];
    //read in data
    fread((void *)tmp,sizeof(int),tmpSize,inStream);
    int flag;
    //read in ok flag
    fread((void *)&flag,sizeof(int),1,inStream);
    //check data integrity
    if (flag == OK_FLAG) {
        //check whether this is the latest and greatest
        if (Compare(time1,last_seq_time)) {
            if (seq_space != NULL) {
                delete seq_space;
            }
            //copy the data
            seq_space = new int[tmpSize];
            memcpy(&last_seq_time,&time1,sizeof(struct timeval));
            memcpy(seq_space,tmp,sizeof(int)*size);
            size = tmpSize;
        }
    }
    delete tmp;
}

/**
 * Compare two struct timeval if time1 > time2
 */
bool MgenSequencer::Compare(struct timeval time1,struct timeval time2) {
   if (time1.tv_sec > time2.tv_sec) return true;
   if (time1.tv_sec == time2.tv_sec) {
      if (time1.tv_usec > time2.tv_usec) return true;
   }
   return false;
}

/**
 * resize the sequence space to support the new flow id
 */
void MgenSequencer::resize(int inFlow) {
    if (inFlow >= size) {
        int *tmp = new int[inFlow+1];
        memcpy(tmp,seq_space,size*sizeof(int));
        *(tmp+inFlow)=0;
        delete seq_space;
        seq_space = tmp;
        size=inFlow+1;
    }
}


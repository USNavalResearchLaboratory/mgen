#include "gpsPub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h> // for permissions flags

#include <unistd.h>  // for unlink()

static const char* GPS_DEFAULT_KEY_FILE = "/tmp/gpskey";

/**
 * Upon success, this returns a pointer for
 * storage of published GPS position
 */
extern "C" char* GPSMemoryInit(const char* keyFile, unsigned int size)
{
    if (!keyFile) keyFile = GPS_DEFAULT_KEY_FILE;
    char* posPtr = ((char*)-1);
    int id = -1;
    
    // First read file to see if shared memory already active
    // If active, try to use it
    FILE* filePtr = fopen(keyFile, "r");
    
    if (filePtr)
    {
        if (1 == fscanf(filePtr, "%d", &id))
        {
            if (((char*)-1) != (posPtr = (char*)shmat(id, 0, 0)))
            {
                // Make sure pre-existing shared memory is right size
                unsigned int theSize;
                memcpy(&theSize, posPtr, sizeof(unsigned int));
                if (size != theSize)
                {
                    GPSPublishShutdown((GPSHandle)posPtr, keyFile);
                    posPtr = (char*)-1;   
                }
            }
            else
            {
               perror("GPSPublishInit(): shmat() warning");       
            }
        }
        fclose(filePtr);
    }
    
    if (((char*)-1) == posPtr)
    {
        // Create new shared memory segment
        // and advertise its "id" in the keyFile
        id = shmget(0, (int)(size+sizeof(unsigned int)), IPC_CREAT | 
                    SHM_R| S_IRGRP | S_IROTH | SHM_W);
        if (-1 == id)
        {
            perror("GPSPublishInit(): shmget() error");
            return NULL;
        }
        if (((char*)-1) ==(posPtr = (char*)shmat(id, 0, 0)))
        {
            perror("GPSPublishInit(): shmat() error");
            struct shmid_ds ds;
            if (-1 == shmctl(id, IPC_RMID, &ds))
            {  
                perror("GPSPublishInit(): shmctl(IPC_RMID) error");
            }
            return NULL;
        }
        // Write "id" to "keyFile"
        if ((filePtr = fopen(keyFile, "w+")))
        {
            if (fprintf(filePtr, "%d", id) <= 0)
                perror("GPSPublishInit() fprintf() error");
            fclose(filePtr);
            memset(posPtr+sizeof(unsigned int), 0, size);
            memcpy(posPtr, &size, sizeof(unsigned int));
            return (posPtr + sizeof(unsigned int));
        }
        else
        {
            perror("GPSPublishInit() fopen() error");
        }
        if (-1 == shmdt(posPtr)) 
            perror("GPSPublishInit() shmdt() error");
        struct shmid_ds ds;
        if (-1 == shmctl(id, IPC_RMID, &ds))
            perror("GPSPublishInit(): shmctl(IPC_RMID) error");
        return NULL;
    }
      
    if (((char*)-1) == posPtr) 
        return NULL;
    else
        return (posPtr + sizeof(unsigned int));
}  // end GPSPublishInit()

extern "C" void GPSPublishShutdown(GPSHandle gpsHandle, const char* keyFile)
{
    char* ptr = (char*)gpsHandle - sizeof(unsigned int);
    if (!keyFile) keyFile = GPS_DEFAULT_KEY_FILE;
    if (-1 == shmdt(ptr)) 
        perror("GPSPublishShutdown() shmdt() error");
    FILE* filePtr = fopen(keyFile, "r");
    if (filePtr)
    {
        int id;
        if (1 == fscanf(filePtr, "%d", &id))
        {
            struct shmid_ds ds;
            if (-1 == shmctl(id, IPC_RMID, &ds))
                perror("GPSPublishShutdown(): shmctl(IPC_RMID) error");
        }
        fclose(filePtr);
        if (unlink(keyFile)) 
            perror("GPSPublishShutdown(): unlink() error");
    }
    else
    {
        perror("GPSPublishShutdown(): fopen() error");
    }   
}  // end GPSPublishShutdown();


/**
 * Upon success, this returns a pointer for
 * storage of published GPS position
 */
extern "C" GPSHandle GPSSubscribe(const char* keyFile)
{
   if (!keyFile) keyFile = GPS_DEFAULT_KEY_FILE;
    char* posPtr = ((char*)-1);
    int id = -1;
    // First read file to see if shared memory already active
    // If active, try to use it
    FILE* filePtr = fopen(keyFile, "r");
    if (filePtr)
    {
        if (1 == fscanf(filePtr, "%d", &id))
        {
            if (((char*)-1) == (posPtr = (char*)shmat(id, 0, SHM_RDONLY)))
            {
               perror("GPSSubscribe(): shmat() error"); 
               fclose(filePtr);
               return NULL;      
            }
            else
            {
                fclose(filePtr);
                return (posPtr + sizeof(unsigned int));   
            }
        }
        else
        {
            perror("GPSSubscribe(): fscanf() error");  
            fclose(filePtr);
            return NULL; 
        }
    }
    else
    {
        //fprintf(stderr, "GPSSubscribe(): Error opening %s.\n", keyFile);
        //perror("GPSSubscribe(): fopen() error"); 
        return NULL;      
    }
}  // end GPSSubscribe()

extern "C" void GPSUnsubscribe(GPSHandle gpsHandle)
{
    char* ptr = (char*)gpsHandle - sizeof(unsigned int);
    if (-1 == shmdt(ptr)) 
        perror("GPSUnsubscribe() shmdt() error");
}  // end GPSUnsubscribe()

extern "C" void GPSPublishUpdate(GPSHandle gpsHandle, const GPSPosition* currentPosition)
{
    memcpy((char*)gpsHandle, (char*)currentPosition, sizeof(GPSPosition));   
}  // end GPSPublishUpdate()

extern "C" void GPSGetCurrentPosition(GPSHandle gpsHandle, GPSPosition* currentPosition)
{
    memcpy((char*)currentPosition, (char*)gpsHandle, sizeof(GPSPosition));
}  // end GPSGetCurrentPosition()

extern "C" unsigned int GPSSetMemory(GPSHandle gpsHandle, unsigned int offset, 
                            const char* buffer, unsigned int len)
{
    char* ptr = (char*)gpsHandle - sizeof(unsigned int);
    unsigned int size;
    memcpy(&size, ptr, sizeof(unsigned int));
    
    // Make sure request fits into available shared memory
    if ((offset+len) > size)
    {
        fprintf(stderr, "GPSSetMemory() Request exceeds allocated shared memory!\n");
        unsigned int delta = offset + len - size;
        if (delta > len)
            return 0;
        else
            len -= delta;        
    }
    ptr = (char*)gpsHandle + offset;
    memcpy(ptr, buffer, len);
    return len;
}  // end GPSSetMemory()

extern "C" unsigned int GPSGetMemorySize(GPSHandle gpsHandle)
{
    char* ptr = (char*)gpsHandle - sizeof(unsigned int);
    unsigned int size;
    memcpy(&size, ptr, sizeof(unsigned int));
    return size;   
}

extern "C" const char* GPSGetMemoryPtr(GPSHandle       gpsHandle, 
                                       unsigned int    offset)
{
    char* ptr = (char*)gpsHandle - sizeof(unsigned int);
    unsigned int size;
    memcpy(&size, ptr, sizeof(unsigned int));
    if (offset < size)
    {
        return (char*)gpsHandle + offset;
    }
    else
    {
        fprintf(stderr, "GPSGetMemory() Invalid request!\n");
        return (const char*)NULL;
    }
}  // end GPSGetMemoryPtr()

extern "C" unsigned int GPSGetMemory(GPSHandle gpsHandle, unsigned int offset, 
                                     char* buffer, unsigned int len)
{
    char* ptr = (char*)gpsHandle - sizeof(unsigned int);
    unsigned int size;
    memcpy(&size, ptr, sizeof(unsigned int));
    if (size < (offset+len))
    {
        unsigned int delta = offset + len - size;
        if (delta > len)
        {
            fprintf(stderr, "GPSGetMemory() Invalid request!\n");
            return 0;
        }
        else
        {
            len -= delta;
        }   
    }
    ptr = (char*)gpsHandle + offset;
    memcpy(buffer, ptr, len);
    return len;
}  // end GPSGetMemory()

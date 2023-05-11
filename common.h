#ifndef COMMON_H_
#define COMMON_H_
#include <pthread.h>

#define UNIX_PATH_MAX 108
#define MAX_PATH_LENGTH 256
#define BUFFERSIZE 265
#define SOCKNAME "./farm.sck"

typedef struct queueElementName
{
    char *filename;
    struct queueElementName *next;
} qElem;

typedef struct sendQueueElement
{
    char *filename;
    long val;
    struct sendQueueElement *next;
} sqElement;

extern int errorRetValue; 
extern int retValue; 

//variabili condivise
extern qElem *queueHead;
extern int queueSize;
extern pthread_mutex_t mtx;
extern pthread_cond_t queueFull;
extern pthread_cond_t queueEmpty;
extern int masterExitReq;

extern sqElement *sqHead;
extern int sqSize;
extern pthread_mutex_t sendermtx;
extern pthread_cond_t sqEmpty;

#endif
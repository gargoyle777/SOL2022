#ifndef WORKERTHREAD_H_
#define WORKERTHREAD_H_
#include <pthread.h>

#define UNIX_PATH_MAX 256
#define BUFFERSIZE 265
#define SOCKNAME "./farm.sck"

typedef struct queueElementName
{
    char *filename;
    struct queueElementName *next;
} qElem;


//variabili condivise
extern qElem *queueHead;
extern int queueSize;
extern pthread_mutex_t mtx;
extern pthread_cond_t queueFull;
extern pthread_cond_t queueEmpty;
extern int masterExitReq;


void* worker(void *arg);

#endif

#include <pthread.h>
#ifndef WORKERTHREAD_H_
#define WORKERTHREAD_H_

struct queueEl
{
    char *filename;
    struct queueEl *next;
};

//variabili condivise
extern struct queueEl *queueHead;
extern int queueSize;
extern pthread_mutex_t mtx;
extern pthread_cond_t queueNotFull;
extern pthread_cond_t queueNotEmpty;
extern int masterExitReq;

void* worker(void *arg);

#endif

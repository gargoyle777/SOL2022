#ifndef WORKERTHREAD_H_
#include <pthread.h>
#define WORKERTHREAD_H_

struct queueEl
{
    char *filename;
    struct queueEl *next;
};

struct sharedholder{
//variabili condivise
	struct queueEl *queueHead;
	int queueSize;
	pthread_mutex_t mtx;
	pthread_cond_t queueNotFull;
	pthread_cond_t queueNotEmpty;
	int masterExitReq;
};
void* worker(void *arg);

#endif

#ifndef WORKERTHREAD_H_
#define WORKERTHREAD_H_
#include <pthread.h>

typedef struct queueElementName
{
    char *filename;
    struct queueElementName *next;
} qElem;

void* worker(void *arg);

#endif

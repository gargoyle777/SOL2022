#ifndef SENDERTHREAD_H_
#define SENDERTHREAD_H_

#include <pthread.h>

typedef struct sendQueueElement
{
    char *filename;
    long val;
    struct sendQueueElement *next;
} sqElement;

void* worker(void *arg);

#endif
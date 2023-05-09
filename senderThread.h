#ifndef SENDERTHREAD_H_
#define SENDERTHREAD_H_

#include <pthread.h>

#define UNIX_PATH_MAX 256
#define BUFFERSIZE 265
#define SOCKNAME "./farm.sck"

typedef struct sendQueueElement
{
    char *filename;
    long val;
    struct sendQueueElement *next;
} sqElement;

//variabili condivise
extern sqElement *sqHead;
extern int sqSize;
extern pthread_mutex_t sendermtx;
extern pthread_cond_t sqEmpty;


void* worker(void *arg);

#endif
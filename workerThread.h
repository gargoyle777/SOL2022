#ifndef WORKERTHREAD_H_
#define WORKERTHREAD_H_

struct queueEl
{
    char *filename;
    struct queueEl *next;
};
typedef struct queueEl node;

struct arguments
{
    struct queueEl *queueHead;
    int *queueSize;
    pthread_mutex_t *mtx;
    pthread_cond_t *queueNotFull;
    pthread_cond_t *queueNotEmpty;
    int *exitReq;
};

void* worker(void *arg);

#endif
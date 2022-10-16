#include <pthread.h>

typedef struct queueEl{
    char* filename;
    struct queueEl * next;
} node;

typedef struct arguments 
{
    node* queueHead;
    int *queueSize;
    pthread_mutex_t mtx;
    pthread_cond_t queueNotFull;
    pthread_cond_t queueNotEmpty;
}   workerArgs;

static void* worker(void * arg){
    node target;
    node* head = ((workerArgs*) arg)->queueHead;
    pthread_mutex_t mtx = ((workerArgs*) arg)->mtx;
    int* queueSize = ((workerArgs*) arg)->queueSize;
    pthread_cond_t queueNotFull = ((workerArgs*) arg)->queueNotFull;
    pthread_cond_t queueNotEmpty = ((workerArgs*) arg)->queueNotEmpty;
    while(1)
    {
        Pthread_mutex_lock (&mtx);
        while(queueSize==0)
        {
            Pthread_cond_wait(&queueNotEmpty,&mtx);
        }
        target=*head;
        head = head->next;
        queueSize--;
        Pthread_cond_signal(&queueNotFull);
        Pthread_mutex_unlock(&mtx);
        //elaborate_file(target);
        //comunicate result;
    }
}



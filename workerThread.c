#include <pthread.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/un.h>
#include <errno.h>
#define UNIX_PATH_MAX 108
#define SOCKNAME "./mysock"
#define N 100

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
    int * exitReq;
}   workerArgs;

static void* worker(void * arg)
{
    int fdSKT, fdC;
    char buf[N];
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    node target;
    node* head = ((workerArgs*) arg)->queueHead;
    pthread_mutex_t mtx = ((workerArgs*) arg)->mtx;
    int* queueSize = ((workerArgs*) arg)->queueSize;
    pthread_cond_t queueNotFull = ((workerArgs*) arg)->queueNotFull;
    pthread_cond_t queueNotEmpty = ((workerArgs*) arg)->queueNotEmpty;

    //CONNECT TO THE COLLECTOR:
    fdSKT = socket(AF_UNIX,SOCK_STREAM, 0);
    while( connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa)) ==-1 )
    {
        if (errno == ENOENT)    sleep(1);
        else exit(EXIT_FAILURE);
    }
    //ready to write and read

    while(1)
    {
        Pthread_mutex_lock (&mtx);
        while(queueSize==0)
        {
            Pthread_cond_wait(&queueNotEmpty,&mtx);
        }
        target = *head;
        head = head->next;
        queueSize--;
        Pthread_cond_signal(&queueNotFull);
        Pthread_mutex_unlock(&mtx);
        //elaborate_file(target);
        write(fdSKT, target.filename, strlen(target.filename));
        write(fdSKT, result, 8);    //long int in 8 bytes
        write(fdSKT, "\n", 1);  //chosen as End of input
        if(* ((workerArgs*) arg)->exitReq)
        {
            pthread_exit((void *) 0);
        }
    }
}



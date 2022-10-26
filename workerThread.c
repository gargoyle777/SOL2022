#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/stat.h>
#define UNIX_PATH_MAX 255
#define SOCKNAME "./mysock"

typedef struct queueEl
{
    char* filename;
    struct queueEl * next;
} node;

typedef struct arguments 
{
    node* queueHead;
    int* queueSize;
    pthread_mutex_t *mtx;
    pthread_cond_t *queueNotFull;
    pthread_cond_t *queueNotEmpty;
    int* exitReq;
}   workerArgs;

static void cleanup_handler(void* arg)
{
    free(arg);
}

static void lock_cleanup_handler(void* arg)
{
    Pthread_mutex_unlock(arg);
}

static void target_cleanup_handler(void* arg)
{
    free(((node*) arg)->filename);
    free(arg);
}

static void socket_cleanup_handler(void* arg)
{
    close(arg);
}

void* worker(void* arg)
{
    int endFlag = 0;
    char charLong[21];
    char *tmpString;
    long result;
    int fdSKT;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    node target;
    node* head = ((workerArgs*) arg)->queueHead;
    pthread_mutex_t *mtx = ((workerArgs*) arg)->mtx;
    int* queueSize = ((workerArgs*) arg)->queueSize;
    pthread_cond_t *queueNotFull = ((workerArgs*) arg)->queueNotFull;
    pthread_cond_t *queueNotEmpty = ((workerArgs*) arg)->queueNotEmpty;

    //CONNECT TO THE COLLECTOR:
    fdSKT = socket(AF_UNIX,SOCK_STREAM, 0);
    while( connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa)) ==-1 )
    {
        if (errno == ENOENT)    sleep(1);
        else exit(EXIT_FAILURE);
    }
    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);
    //ready to write and read

    while(!endFlag)
    {
        Pthread_mutex_lock(mtx);
        pthread_cleanup_push(lock_cleanup_handler, mtx);
        while(queueSize==0)
        {
            Pthread_cond_wait(queueNotEmpty,mtx);
        }
        if( *(((workerArgs*) arg)->exitReq) >= queueSize)   endFlag = 1;
        target = *head;
        head = head->next;
        queueSize--; 
        pthread_cleanup_push(target_cleanup_handler, &target);
        Pthread_cond_signal(queueNotFull);
        Pthread_mutex_unlock(mtx);

        pthread_cleanup_pop(0);
        result = fileCalc(target.filename);
        ltoa(result,charLong,10);
        tmpString = calloc( (strlen(target.filename)+strlen(charLong)+2),sizeof(char));
        pthread_cleanup_push(cleanup_handler, tmpString);
        strcat(tmpString,target.filename);
        strcat(tmpString,'/');
        strcat(tmpString,charLong);
        write(fdSKT, tmpString, strlen(tmpString));    
        pthread_cleanup_pop(1); //true per fare il free della stringa
        pthread_cleanup_pop(1); //true per fare il free del node
    }
    close(fdSKT);
    pthread_cleanup_pop(0);
    pthread_exit((void *) 0);
}

long fileCalc(char* fileAddress)
{
    FILE *file;
    struct stat st;
    long tmp;
    int i=0;
    long result = 0;
    if(stat(fileAddress, &st) != 0)
    {
        //check error
    }
    file = fopen(fileAddress, "rb"); 
    while(fread(&tmp, sizeof(long),1,file) == sizeof(long))
    {
        result = result + (tmp * i);
        i++;
    }
    //check if eof or error
    return result;
}
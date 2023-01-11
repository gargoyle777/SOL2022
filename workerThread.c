#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#define UNIX_PATH_MAX 255
#define SOCKNAME "./farm.sck"
#include "workerThread.h"
#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); exit(EXIT_FAILURE); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror(m); exit(EXIT_FAILURE); }
#define ec_zero(s,m) \
    if((s) != 0) { perror(m); exit(EXIT_FAILURE); }

typedef struct arguments workerArgs;

static void cleanup_handler(void* arg)
{
    free(arg);
}

static void lock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(arg),"wroker's unlock failed during cleanup");
}

static void target_cleanup_handler(void* arg)
{
    free(((node*) arg)->filename);
    free(arg);
}

static void socket_cleanup_handler(void* arg)
{
    ec_meno1(close(*((int*) arg)),errno);
}

static void file_cleanup_handler(void *arg)
{
    ec_zero(fclose(arg),errno);
}

static long fileCalc(char* fileAddress)
{
    FILE *file;
    struct stat st;
    long tmp;
    int i=0;
    long result = 0;
    ec_meno1(stat(fileAddress, &st),errno);
    
    file = fopen(fileAddress, "rb"); 
    ec_null(file,errno);
    pthread_cleanup_push(file_cleanup_handler, &file);
    while(fread(&tmp, sizeof(long),1,file) == sizeof(long))
    {
        result = result + (tmp * i);
        i++;
    }
    pthread_cleanup_pop(1); //true per fare il fclose
    //check if eof or error
    return result;
}

void* worker(void* arg)
{
    int masterexitcode;
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
    ec_meno1(fdSKT,errno);
    ec_meno1(connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa)),errno );
    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);
    //ready to write and read

    while(1)
    {
        ec_zero(pthread_mutex_lock(mtx),"worker's lock failed");
        pthread_cleanup_push(lock_cleanup_handler, mtx);
        masterexitcode= *(((workerArgs*) arg)->exitReq);
        while(*queueSize==0 && masterexitcode==0)
        {
            ec_zero(pthread_cond_wait(queueNotEmpty,mtx),"worker's cond wait on queueNotEmpty failed");
        }
        if (*queueSize ==0) break;  //TODO check if clean up is done anyway

        if( masterexitcode==2)   break; //TODO check clean up, for sigusr1

        target = *head;
        head = head->next;
        (*queueSize)--; 
        pthread_cleanup_push(target_cleanup_handler, &target);
        ec_zero(pthread_cond_signal(queueNotFull),"worker's signal on queueNotFull failed");
        ec_zero(pthread_mutex_unlock(mtx),"worker's unlock failed");

        pthread_cleanup_pop(0);
        result = fileCalc(target.filename);

        sprintf(charLong,"%ld",result);

        tmpString = calloc( (strlen(target.filename)+strlen(charLong)+2),sizeof(char));
        ec_null(tmpString,"calloc on tmpString failed in worker");
        pthread_cleanup_push(cleanup_handler, tmpString);
        strcat(tmpString,target.filename);
        strcat(tmpString,"/");
        strcat(tmpString,charLong);
        ec_meno1(write(fdSKT, tmpString, strlen(tmpString)),errno);    
        pthread_cleanup_pop(1); //true per fare il free della stringa
        pthread_cleanup_pop(1); //true per fare il free del node
    }
    ec_meno1(close(fdSKT),errno);
    pthread_cleanup_pop(0);
    pthread_exit((void *) 0);
}


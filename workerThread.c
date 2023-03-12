#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
    ec_zero(pthread_mutex_unlock(arg),"worker's unlock failed during cleanup");
}

static void target_cleanup_handler(void* arg)
{
    free(((struct queueEl*) arg)->filename);
    free(arg);
}

static void socket_cleanup_handler(void* arg)
{
    ec_meno1(close(*((int*) arg)),errno);
}

static void file_cleanup_handler(void *arg)
{
    ec_zero(fclose(*(FILE**)arg),errno);
}

long fileCalc(char* fileAddress)
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
    void* buffer_write;
    char charLong[21];
    char *tmpString;
    long result;
    int fdSKT;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    struct queueEl target;

    //CONNECT TO THE COLLECTOR
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    ec_meno1(fdSKT,errno);
    ec_meno1(connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa)),errno );
    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);
    //ready to write and read

    while(1)
    {
        ec_zero(pthread_mutex_lock(mtx),"worker's lock failed");
        pthread_cleanup_push(lock_cleanup_handler, &mtx);
        while(queueSize==0 && masterExitReq==0)
        {
            ec_zero(pthread_cond_wait(&queueNotEmpty,&mtx),"worker's cond wait on queueNotEmpty failed");
        }
        if (queueSize ==0) break;  //TODO check if clean up is done anyway

        if(masterExitReq==2)   break; //TODO check clean up, for sigusr1

        target = *queueHead;
        queueHead = queueHead->next;
        queueSize--; 
        pthread_cleanup_push(target_cleanup_handler, &target);
        ec_zero(pthread_cond_signal(&queueNotFull),"worker's signal on queueNotFull failed");
        ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed");

        pthread_cleanup_pop(0);
        result = fileCalc(target.filename);

        sprintf(charLong,"%ld",result);

        //sending the value
        buffer_write = malloc(strlen(target.filename)+8); 
        ec_null(buffer_write,"malloc on buffer_write failed in worker");
        pthread_cleanup_push(cleanup_handler, buffer_write);

        memcpy(buffer_write, target.filename, strlen(target.filename));      //does memcpy copy the terminator? no because strlen doesnt count it
        memcpy(&(buffer_write[strlen(tmpString)]), result,8); 

        ec_meno1(write(fdSKT, buffer_write, messageLength),errno);    //now i should write buffer_write
        //end of sending

        pthread_cleanup_pop(1); //true per fare il free del buffer
        pthread_cleanup_pop(1); //true per fare il free del node
    }
    ec_meno1(close(fdSKT),errno);
    pthread_cleanup_pop(0);
    pthread_exit((void *) 0);
}


#include "common.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

//sender extern
sqElement *sqHead=NULL;
int sqSize=0;
pthread_mutex_t sendermtx;
pthread_cond_t sqEmpty;

//worker extern
pqElement *queueHead=NULL;
int pqSize=0;
pthread_mutex_t producermtx;
pthread_cond_t pqFull;
pthread_cond_t pqEmpty;

pthread_mutex_t requestmtx;
int masterExitReq=0;

void senderlock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&sendermtx),"worker's unlock failed during cleanup");
}

void producerlock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&producermtx),"worker's unlock failed during cleanup");
}

void requestlock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&requestmtx),"sender's unlock failed during cleanup");
}

void workstruct_cleanup_handler(void* arg)
{
    free((*(pqElement**) arg)->filename);
    free(*(pqElement**) arg);
}

void file_cleanup_handler(void *arg)
{
    ec_zero(fclose(*(FILE**)arg),(strerror(errno)));
}

void senderstruct_cleanup_handler(void* arg)
{
    free((*(sqElement**) arg)->filename);
    free(*(sqElement**)arg);
}

void socket_cleanup_handler(void* arg)
{
    ec_meno1(close(*(int*) arg),strerror(errno));
}

void checked_realloc(void ***ptr, int length, size_t size)
{
    errno=0;
    if(length==1) 
    {
        //printf("provo malloc \n");
        *ptr=malloc(length*size);
    }
    else 
    {
        //printf("provo realloc \n");
        *ptr=realloc(*ptr, length*size);
    }
    ec_null(*ptr,"checked_realloc fallita");
    //printf("riuscita\n");
}
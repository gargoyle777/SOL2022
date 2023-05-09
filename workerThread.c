//CHECK HOW FLAGWORK IS SET TO 0

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
#include "workerThread.h"
#include "senderThread.h"

int errorRetValue=1;
int retValue=0;

#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); pthread_exit(&errorRetValue); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror("WORKER"); pthread_exit(&errorRetValue); }
#define ec_zero(s,m) \
    if((s) != 0) { perror("WORKER"); pthread_exit(&errorRetValue); }

qElem *queueHead=NULL;
int queueSize=0;
pthread_mutex_t mtx;
pthread_cond_t queueFull;
pthread_cond_t queueEmpty;
int masterExitReq=0;


static void sender_lock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&sendermtx),"worker's unlock failed during cleanup");
}

static void producer_lock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed during cleanup");
}

static void target_cleanup_handler(void* arg)
{
    free((*(qElem**) arg)->filename);
    free(*(qElem**) arg);
}

static void file_cleanup_handler(void *arg)
{
    ec_zero(fclose(*(FILE**)arg),(strerror(errno)));
}

static long fileCalc(char* fileAddress)
{
    FILE *file;
    long tmp;
    int i=0;
    long result = 0;
    printf("worker sta per accedere a: %s\n",fileAddress);//testing

    pthread_cleanup_push(file_cleanup_handler, &file);
    file = fopen(fileAddress, "rb"); 
    ec_null(file,(strerror(errno)));
    
    while(fread(&tmp, sizeof(long),1,file) == sizeof(long))
    {
        result = result + (tmp * i);
        i++;
    }
    pthread_cleanup_pop(1); //true per fare il fclose
    return result;
}

static void safeDeposit(sqElement* target)
{
    sqElement* tmp; 
    
    pthread_cleanup_push(sender_lock_cleanup_handler);  //lock del sender
    ec_zero(pthread_mutex_lock(&sendermtx),"worker's lock for write failed"); 

    if( sqSize == 0)
    {
        sqHead = target;
        pthread_cond_signal(sqEmpty);
    }
    else
    {
        tmp = sqHead;
        while(tmp->next != NULL) tmp = tmp->next;
        tmp->next = target;
    }
    sqSize++;
    pthread_cleanup_pop(1); //rilascio il lock
}

void* worker(void* arg)
{
    printf("worker avviato\n");
    int accums;
    int bytesWritten=0;
    char buffer_write[265];
    int flagwork=1;
    char charLong[21];
    char *tmpString;
    long result;
    int fdSKT;
    struct sockaddr_un sa;
    char ackHolder[4];
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    int nread;
    sqElement *sqePointer;
    qElem* target;
    
    while(flagwork==1)
    {
        sqePointer = NULL;
        ec_zero(pthread_mutex_lock(&mtx),"worker's lock failed");
        pthread_cleanup_push(producer_lock_cleanup_handler);       //spingo cleanup per lock

        while(queueSize==0 && masterExitReq==0)
        {
        	printf("worker in attesa a causa di lista vuota\n");
            ec_zero(pthread_cond_wait(&queueEmpty,&mtx),"worker's cond wait on queueEmpty failed");
        }
        
        printf("worker fuori dal loop con wait, queuesize= %d e masterExitReq=%d\n",queueSize, masterExitReq);

        if(queueSize==0)
        {
        	printf("worker esce, 0 elementi nella queuesize e masterexitreq settato a 1\n");
            pthread_exit(&retValue);
	    }

        if(masterExitReq==2)
        {
           	printf("worker esce, masterexitreq settato a 2\n");
           	pthread_exit(&retValue);
        }

        if(masterExitReq==1 && queueSize>0)
        {
            printf("worker fa un ultimo giro\n");
            flagwork=0;
        }

        printf("worker cerca di raccogliere l'elemento\n");
        target = queueHead;
        queueHead = queueHead->next;
        queueSize--; 
        ec_zero(pthread_cond_signal(&queueFull),"worker's signal on queueFull failed");
        ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed");
        pthread_cleanup_pop(0); //tolgo per cleanup del lock
        pthread_cleanup_push(target_cleanup_handler, &target);      //spingo clean up per target
        printf("worker ha lavorato su %s\n",target->filename);

        sqePointer=malloc(sizeof(sqElement));
        ec_null(sqePointer,"worker failed to do a malloc");
        sqePointer->filename = malloc(sizeof(char)*(strnlen(target->filename,UNIX_PATH_MAX)+1));
        ec_null(sqePointer->filename,"worker failed to do a malloc");
        strncpy(sqePointer->filename,target->filename,UNIX_PATH_MAX);
        sqePointer->val = fileCalc(target->filename);
        sqePointer->next = NULL;

        safeDeposit(sqePointer);

        pthread_cleanup_pop(1); //tolgo per clean up del target con true
    }
    pthread_exit(&retValue);
}


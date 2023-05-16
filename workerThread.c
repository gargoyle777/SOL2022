//CHECK HOW FLAGWORK IS SET TO 0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "workerThread.h"
#include "common.h"
#include <unistd.h>

#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); pthread_exit(&errorRetValue); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror(m); pthread_exit(&errorRetValue); }
#define ec_zero(s,m) \
    if((s) != 0) { perror(m); pthread_exit(&errorRetValue); }




static void sender_lock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&sendermtx),"worker's unlock failed during cleanup");
}

static void producer_lock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed during cleanup");
}

static void requestlock_cleanup_handler(void* arg)
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
    //printf("worker sta per accedere a: %s\n",fileAddress);//testing

    pthread_cleanup_push(file_cleanup_handler, &file);
    file = fopen(fileAddress, "rb"); 
    ec_null(file,(strerror(errno)));
    
    while(fread(&tmp, sizeof(long),1,file) == 1)
    {
        result = result + (tmp * (long)i);
        i++;
    }
    pthread_cleanup_pop(1); //true per fare il fclose
    //printf("worker ha calcolato il valore: %ld\n", result);
    return result;
}

static void safeDeposit(sqElement* target)
{
    sqElement* tmp; 
    
    pthread_cleanup_push(sender_lock_cleanup_handler,NULL);  //lock del sender
    ec_zero(pthread_mutex_lock(&sendermtx),"worker's lock for write failed"); 

    //printf("worker ha il sender lock, cerca di depositare\n");
    if( sqSize == 0)
    {
        sqHead = target;
        pthread_cond_signal(&sqEmpty);
    }
    else
    {
        tmp = sqHead;
        while(tmp->next != NULL) tmp = tmp->next;
        tmp->next = target;
    }
    sqSize++;
    //printf("worker ha depositato, rilascia il lock\n");
    pthread_cleanup_pop(1); //rilascio il lock
}

void* producerWorker(void* arg)
{
    //printf("worker avviato\n");
    int flagwork=1;
    sqElement *sqePointer;
    qElem* target;
    int exitreqval = 0;
    
    while(flagwork==1)
    {
        pthread_cleanup_push(requestlock_cleanup_handler, NULL);
        pthread_mutex_lock(&requestmtx);
        if(masterExitReq!=0) exitreqval = masterExitReq;
        pthread_mutex_unlock(&requestmtx);
        pthread_cleanup_pop(0);
        sqePointer = NULL;
        ec_zero(pthread_mutex_lock(&mtx),"worker's lock failed");
        pthread_cleanup_push(producer_lock_cleanup_handler,NULL);       //spingo cleanup per lock

        while(queueSize==0 && exitreqval==0)
        {
        	//printf("worker in attesa a causa di lista vuota\n");
            ec_zero(pthread_cond_wait(&queueEmpty,&mtx),"worker's cond wait on queueEmpty failed");
            pthread_cleanup_push(requestlock_cleanup_handler, NULL);
            pthread_mutex_lock(&requestmtx);
            if(masterExitReq!=0) exitreqval = masterExitReq;
            pthread_mutex_unlock(&requestmtx);
            pthread_cleanup_pop(0);
        }
        
        //printf("worker fuori dal loop con wait, queuesize= %d e masterExitReq=%d\n",queueSize, masterExitReq);

        if(queueSize==0)
        {
        	//printf("worker esce, 0 elementi nella queuesize e masterexitreq settato a 1\n");
            pthread_exit(&retValue);
	    }

        if(exitreqval==2)
        {
           	//printf("worker esce, masterexitreq settato a 2\n");
           	pthread_exit(&retValue);
        }

        if(exitreqval==1 && queueSize>0)
        {
            //printf("worker fa un ultimo giro\n");
            flagwork=0;
        }

        //printf("worker cerca di raccogliere l'elemento\n");
        target = queueHead;
        queueHead = queueHead->next;
        queueSize--; 
        ec_zero(pthread_cond_signal(&queueFull),"worker's signal on queueFull failed\n");
        pthread_cleanup_pop(1); //tolgo per cleanup del lock
        pthread_cleanup_push(target_cleanup_handler, &target);      //spingo clean up per target
        //printf("worker sta lavorando su %s\n",target->filename);

        sqePointer=malloc(sizeof(sqElement));
        ec_null(sqePointer,"worker failed to do a malloc");
        sqePointer->filename = malloc(sizeof(char)*(strnlen(target->filename,MAX_PATH_LENGTH)+1));
        ec_null(sqePointer->filename,"worker failed to do a malloc");
        strncpy(sqePointer->filename,target->filename,strlen(target->filename)+1);
        sqePointer->val = fileCalc(target->filename);
        sqePointer->next = NULL;

        safeDeposit(sqePointer);
        printf("worker ha depositato %s\n",sqePointer->filename);
        //printf("worker ha finito di dare in pasto a sender");
        pthread_cleanup_pop(1); //tolgo per clean up del target con true
    }
    pthread_exit(&retValue);
}


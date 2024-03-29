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


static long fileCalc(char* fileAddress)
{
    FILE *file;
    long tmp;
    int i=0;
    long result = 0;
    //printf("worker sta per accedere a: %s\n",fileAddress);//testing

    pthread_cleanup_push(file_cleanup_handler, &file);
    file = fopen(fileAddress, "rb"); 
    ec_null(file,"worker failed to open file\n");
    
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
    pthread_cleanup_push(senderlock_cleanup_handler,NULL);  //lock del sender
    ec_zero(pthread_mutex_lock(&sendermtx),"workerfailed to get sender lock\n"); 

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
    sqElement *sqePointer;
    pqElement* target;
    int exitreqval = 0;
    
    while(1)
    {
        pthread_cleanup_push(requestlock_cleanup_handler, NULL);
        pthread_mutex_lock(&requestmtx);
        if(masterExitReq!=0) exitreqval = masterExitReq;
        pthread_mutex_unlock(&requestmtx);
        pthread_cleanup_pop(0);
        sqePointer = NULL;
        pthread_cleanup_push(producerlock_cleanup_handler,NULL);       //spingo cleanup per lock
        ec_zero(pthread_mutex_lock(&producermtx),"workerfailed to gain production lock\n");     

        while(pqSize==0 && exitreqval==0)
        {
        	//printf("worker in attesa a causa di lista vuota\n");
            ec_zero(pthread_cond_wait(&pqEmpty,&producermtx),"worker's cond wait on pqEmpty failed\n");
            pthread_cleanup_push(requestlock_cleanup_handler, NULL);
            pthread_mutex_lock(&requestmtx);
            if(masterExitReq!=0) exitreqval = masterExitReq;
            pthread_mutex_unlock(&requestmtx);
            pthread_cleanup_pop(0);
        }
        
        //printf("worker fuori dal loop con wait, pqSize= %d e masterExitReq=%d\n",pqSize, masterExitReq);

        if(pqSize==0)
        {
        	//printf("worker esce, 0 elementi nella pqSize e masterexitreq settato a 1\n");
            pthread_exit(NULL);
	    }

        if(exitreqval==2)
        {
           	//printf("worker esce, masterexitreq settato a 2\n");
           	pthread_exit(NULL);
        }

        //printf("worker cerca di raccogliere l'elemento\n");
        target = queueHead;
        queueHead = queueHead->next;
        pqSize--; 
        ec_zero(pthread_cond_signal(&pqFull),"worker's signal on pqFull failed\n");
        pthread_cleanup_pop(1); //tolgo per cleanup del lock
        pthread_cleanup_push(workstruct_cleanup_handler, &target);      //spingo clean up per target
        //printf("worker sta lavorando su %s\n",target->filename);
        errno=0;
        sqePointer=malloc(sizeof(sqElement));
        ec_null(sqePointer,"worker failed to do a malloc\n");
        errno=0;
        sqePointer->filename = malloc(sizeof(char)*(strnlen(target->filename,MAX_PATH_LENGTH)+1));
        ec_null(sqePointer->filename,"worker failed to do a malloc\n");
        strncpy(sqePointer->filename,target->filename,strlen(target->filename)+1);
        sqePointer->val = fileCalc(target->filename);
        sqePointer->next = NULL;
        //printf("worker sta depositato %s\n",sqePointer->filename);
        safeDeposit(sqePointer);

        //printf("worker ha finito di dare in pasto a sender");
        pthread_cleanup_pop(1); //tolgo per clean up del target con true
    }
    //should not end up here, code here for defensive programming
    if(target != NULL)
    {
        free(target->filename);
        free(target);
    }
    pthread_exit(NULL);
}


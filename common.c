#include "common.h"
#include <stdio.h>
#include <pthread.h>

//sender extern
sqElement *sqHead=NULL;
int sqSize=0;
pthread_mutex_t sendermtx;
pthread_cond_t sqEmpty;

//worker extern
qElem *queueHead=NULL;
int queueSize=0;
pthread_mutex_t mtx;
pthread_cond_t queueFull;
pthread_cond_t queueEmpty;
int masterExitReq=0;

//TODO REMOVE WHEN TESTEST 
int errorRetValue=1; 
int retValue=0; 
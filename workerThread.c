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
    ec_meno1(close(*((int*) arg)),(strerror(errno)));
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
    return 0; //testing
    file = fopen(fileAddress, "rb"); 
    ec_null(file,(strerror(errno)));
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

//variabili condivise
struct queueEl *queueHead;
int queueSize;
pthread_mutex_t mtx;
pthread_cond_t queueNotFull;
pthread_cond_t queueNotEmpty;
int masterExitReq = 0;

void* worker(void* arg)
{
	printf("worker avviato\n");//testing
    char charLong[21];
    char *tmpString;
    long result;
    int fdSKT;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    struct queueEl target;

    //CONNECT TO THE COLLECTOR
    fdSKT = socket(AF_UNIX,SOCK_STREAM, 0);
    ec_meno1(fdSKT,(strerror(errno)));
    printf("worker will try to connect to the socket\n"); //testing
    while(connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa))==-1 )
    {
    	if(errno==ENOENT)
    	{
    		printf("worker dorme aspettando di connettersi\n");
    		sleep(1);
    	}
    	else
    	{
    		exit(EXIT_FAILURE);
    	}
    }
    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);
    //ready to write and read
	printf("worker connesso al collector\n");//testing
    while(1)
    {
        ec_zero(pthread_mutex_lock(&mtx),"worker's lock failed");
        pthread_cleanup_push(lock_cleanup_handler, &mtx);
        while(queueSize==0 && masterExitReq==0)
        {
        	printf("worker in attesa a causa di lista vuota\n");
            ec_zero(pthread_cond_wait(&queueNotEmpty,&mtx),"worker's cond wait on queueNotEmpty failed");
        }
        printf("worker fuori dal loop con wait, queuesize= %d\n",queueSize);
        if (queueSize ==0)
        {
        	printf("worker esce, 0 elementi nella e masterexitreq settato\n");
        	break;  //TODO check if clean up is done anyw
	}
        if(masterExitReq==2)
        {
           	printf("worker esce, masterexitreq settato a 2\n");
           	break; //TODO check clean up, for sigusr1
	}
	printf("worker cerca di raccogliere l'elemento\n");
        target = *queueHead;
        queueHead = queueHead->next;
        queueSize--; 
        pthread_cleanup_push(target_cleanup_handler, &target);
        ec_zero(pthread_cond_signal(&queueNotFull),"worker's signal on queueNotFull failed");
        ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed");

        pthread_cleanup_pop(0);
        printf("%s",target.filename);
        result = fileCalc(target.filename);

        sprintf(charLong,"%ld",result);

        tmpString = calloc( (strlen(target.filename)+strlen(charLong)+2),sizeof(char));
        ec_null(tmpString,"calloc on tmpString failed in worker");
        pthread_cleanup_push(cleanup_handler, tmpString);
        strcat(tmpString,target.filename);
        strcat(tmpString,"/");
        strcat(tmpString,charLong);
        ec_meno1(write(fdSKT, tmpString, strlen(tmpString)),(strerror(errno)));    
        pthread_cleanup_pop(1); //true per fare il free della stringa
        pthread_cleanup_pop(1); //true per fare il free del node
    }
    ec_meno1(close(fdSKT),(strerror(errno)));
    pthread_cleanup_pop(0);
    pthread_exit((void *) 0);
}


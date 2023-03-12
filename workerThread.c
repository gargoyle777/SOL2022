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
    if((s) == -1) { perror("worker"); exit(EXIT_FAILURE); }    
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

long fileCalc(char* fileAddress)
{
    FILE *file;
    long tmp;
    int i=0;
    long result = 0;
    printf("worker sta per accedere a: %s\n",fileAddress);//testing

    file = fopen(fileAddress, "rb"); 
    ec_null(file,(strerror(errno)));
    pthread_cleanup_push(file_cleanup_handler, &file);
    while(fread(&tmp, sizeof(long),1,file) == sizeof(long))
    {
        result = result + (tmp * i);
        i++;
    }
    pthread_cleanup_pop(1); //true per fare il fclose
    return result;
}



void* worker(void* arg)
{

    char* buffer_write;
    int flagwork=1;
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

    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);   //spingo cleanup per socket
    //ready to write and read
	printf("worker connesso al collector\n");//testing
    while(flagwork==1)
    {
        ec_zero(pthread_mutex_lock(&mtx),"worker's lock failed");
        pthread_cleanup_push(lock_cleanup_handler, &mtx);       //spingo cleanup per lock

        while(queueSize==0 && masterExitReq==0)
        {
        	printf("worker in attesa a causa di lista vuota\n");
            ec_zero(pthread_cond_wait(&queueNotEmpty,&mtx),"worker's cond wait on queueNotEmpty failed");
        }
        printf("worker fuori dal loop con wait, queuesize= %d\n",queueSize);
        if(queueSize ==0)
        {
        	printf("worker esce, 0 elementi nella e masterexitreq settato\n");
        	flagwork=0;
	    }
        if(masterExitReq==2)
        {
           	printf("worker esce, masterexitreq settato a 2\n");
           	flagwork=0;		//TODO check clean up, for sigusr1
        }


        printf("worker cerca di raccogliere l'elemento\n");
        target = *queueHead;
        queueHead = queueHead->next;
        queueSize--; 
        ec_zero(pthread_cond_signal(&queueNotFull),"worker's signal on queueNotFull failed");
        ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed");

        pthread_cleanup_pop(0); //tolgo per cleanup del lock
        pthread_cleanup_push(target_cleanup_handler, &target);      //spingo clean up per target
        printf("worker ha lavorato su %s\n",target.filename);
        result = fileCalc(target.filename);

        sprintf(charLong,"%ld",result);

        //sending the value
        buffer_write = malloc(strlen(target.filename)+8); 
        ec_null(buffer_write,"malloc on buffer_write failed in worker");
        pthread_cleanup_push(cleanup_handler, buffer_write);        //spingo cleanup per buffer_write

        memcpy(buffer_write, target.filename, strlen(target.filename));      //does memcpy copy the terminator? no because strlen doesnt count it
        memcpy(&(buffer_write[strlen(tmpString)]), &result,8); 

        ec_meno1(write(fdSKT, buffer_write, strlen(buffer_write)),errno);    //now i should write buffer_write
        //end of sending

        pthread_cleanup_pop(1); //tolgo per clean up buffer_write con true
        pthread_cleanup_pop(1); //tolgo per clean up del target cin true

        //chiudo fdsKT????
        ec_meno1(close(fdSKT),errno);
        pthread_cleanup_pop(0);     //tolgo per clean up del socket
    }
    pthread_exit((void *) 0);
}


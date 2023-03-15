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

#define UNIX_PATH_MAX 255
#define SOCKNAME "./farm.sck"

int errorRetValue=1;
int retValue=0;

#define ec_meno1(s,m) \
    if((s) == -1) { perror("worker EC_MENO1"); pthread_exit(&errorRetValue); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror("WORKER"); pthread_exit(&errorRetValue); }
#define ec_zero(s,m) \
    if((s) != 0) { perror("WORKER"); pthread_exit(&errorRetValue); }

struct queueEl *queueHead=NULL;
int queueSize=0;
pthread_mutex_t mtx;
pthread_cond_t queueNotFull;
pthread_cond_t queueNotEmpty;
int masterExitReq=0;

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
    printf("worker avviato\n");
    char* buffer_write;
    int flagwork=1;
    char charLong[21];
    char *tmpString;
    long result;
    int fdSKT,fd;
    struct sockaddr_un sa;
    char ackHolder[4];
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    int nread;

    struct queueEl target;

    //CONNECT TO THE COLLECTOR
    printf("worker inizia la routine di connessione\n");
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    errno=0;
    ec_meno1(fdSKT,errno);
    printf("worker socket() worked\n");
    errno=0;
    ec_meno1(connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa)),errno);

    printf("worker connesso al collector\n");//testing
    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);   //spingo cleanup per socket
    //ready to write and read
	
    
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

        if(queueSize==0)
        {
        	printf("worker esce, 0 elementi nella queuesize e masterexitreq settato\n");
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
        target = *queueHead;
        queueHead = queueHead->next;
        queueSize--; 
        ec_zero(pthread_cond_signal(&queueNotFull),"worker's signal on queueNotFull failed");
        ec_zero(pthread_mutex_unlock(&mtx),"worker's unlock failed");
        pthread_cleanup_pop(0); //tolgo per cleanup del lock
        pthread_cleanup_push(target_cleanup_handler, &target);      //spingo clean up per target
        printf("worker ha lavorato su %s\n",target.filename);
        result = fileCalc(target.filename);

        //sending the value
        buffer_write = malloc(strlen(target.filename)+8); 
        ec_null(buffer_write,"malloc on buffer_write failed in worker");
        pthread_cleanup_push(cleanup_handler, buffer_write);        //spingo cleanup per buffer_write

        memcpy(buffer_write, target.filename, strlen(target.filename));      //does memcpy copy the terminator? no because strlen doesnt count it
        memcpy(&(buffer_write[strlen(tmpString)]), &result,8); 

        ec_meno1(write(fdSKT, buffer_write, strlen(buffer_write)),errno);    //now i should write buffer_write
        //end of sending

        pthread_cleanup_pop(1); //tolgo per clean up buffer_write con true
        pthread_cleanup_pop(1); //tolgo per clean up del target con true

        nread=0;
        memset(ackHolder,0,4);
        do
        {
            errno=0;
            nread=read(fd,ackHolder,4);
            ec_meno1(nread,errno);
        } while(nread!=0);

    }
    //chiudo fdsKT???? 
    ec_meno1(close(fdSKT),errno);
    pthread_cleanup_pop(0);     //tolgo per clean up del socket
    pthread_exit(&retValue);
}


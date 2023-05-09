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
#include "senderThread.h"


#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); pthread_exit(&errorRetValue); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror("WORKER"); pthread_exit(&errorRetValue); }
#define ec_zero(s,m) \
    if((s) != 0) { perror("WORKER"); pthread_exit(&errorRetValue); }

sqElement *sqHead=NULL;
int sqSize=0;
pthread_mutex_t sendermtx;
pthread_cond_t sqEmpty;

static void lock_cleanup_handler(void* arg)
{
    ec_zero(pthread_mutex_unlock(&sendermtx),"sender's unlock failed during cleanup");
}

static void target_cleanup_handler(void* arg)
{
    free((*(sqElement**) arg)->filename);
    free(*(sqElement**)arg);
}

static void socket_cleanup_handler(void* arg)
{
    ec_meno1(close(*(int*) arg),strerror(errno));
}

static int safeConnect() //return the socket file descriptor
{
    int fdSKT; //file descriptor socket
    int counter; //counts conncet() tries
    int checker; //check connect output
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    printf("sender inizia la routine di connessione\n");
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    errno=0;
    ec_meno1(fdSKT,(strerror(errno)));
    printf("sender socket() worked\n");
    counter=0;
    checker=0;
    while(counter<5)
    {
        errno=0;
        checker=0;
        if(checker=connect(fdSKT, (struct sockaddr*) &sa, sizeof(sa)) ==-1)
        {
            printf(" sender failed to connect: %d",counter);
            sleep(1);
            counter+=1;
        }
        else counter = 5;
    }

    ec_meno1(checker,(strerror(errno)));
    printf("sender connesso al collector\n");
    return fdSKT;
}

static void safeACK(int socketFD)
{
    char ackHolder[3];
    int bytesRead = 0;
    int totalBytesRead = 0;
    while( totalBytesRead < 3 )
    {
        errno=0;
        bytesRead = 0;
        c_meno1(bytesRead=read(socketFD,ackHolder,3),"sender dead on ack read\n");
        totalBytesRead+= bytesRead;
        printf("sender ha ricevuto un ACK\n");
    }
}

static void safeWrite(int socketFD, void* file, uint8_t size)
{
    int bytesWritten = 0;
    int totalBytesWritten = 0;
    while( totalBytesWritten < size )
    {
        errno = 0;
        bytesWritten = 0;
        ec_meno1(bytesWritten=write(socketFD, file, size),"sender dead on write!!\n"); 
        totalBytesWritten+= bytesWritten;
        printf("sender ha scritto %d/%d\n",totalBytesWritten,size);
    }
}

static void safeSend(int socketFD, sqElement element)
{
    uint8_t nameLength; //null termination not counted, max size is 256
    nameLength = strnlen(element.filename,UNIX_PATH_MAX);
    safeWrite(socketFD,&nameLength,1u);
    safeACK(socketFD);
    safeWrite(socketFD,element.filename,nameLength);
    safeACK(socketFD);
    safeWrite(socketFD,element.val,8u);
    safeACK(socketFD);
}

static void safeExtract(sqElement** target)
{
    ec_zero(pthread_mutex_lock(&sendermtx),"sender's lock failed");
    pthread_cleanup_push(lock_cleanup_handler, NULL); 

    while( sqSize <= 0)
    {
        printf("sender in attesa a causa di lista vuota\n");
        ec_zero(pthread_cond_wait(&sqEmpty,&sendermtx),"sender's cond wait on sqEmpty failed");
    }

    printf("sender fuori dal loop di wait, si prepara all'estrazione");

    *target=sqHead;
    sqHead=sqHead->next;
    sqSize--;
    pthread_cleanup_pop(1); //free the lock using true value as parameter
}

void* worker(void* arg)
{
    int fdSKT; //file descriptor socket
    sqElement* target;

    fdSKT = safeConnect();

    pthread_cleanup_push(socket_cleanup_handler, &fdSKT);
    while( 1 )
    {
        safeExtract(&target);

        pthread_cleanup_push(target_cleanup_handler, &target); 
        
        safeSend(fdSKT,*target);

        pthread_cleanup_pop(1); // faccio il free dei valori
    }

    pthread_cleanup_pop(1);

    pthread_exit(&retValue);
}
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#import <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <sys/select.h>
#include "workerThread.h"

#define SOCKNAME "./farm.sck"
#define BUFFERSIZE 277
#define UNIX_PATH_MAX 255

typedef struct queueEl node;
typedef struct arguments workerArgs;


volatile sig_atomic_t flagEndReading= 0;

typedef struct supp
{
    long value;
    char *name;
}   res;

int compare( const void* a, const void* b)
{
    return ( ((res*)a)->value - ((res*)b)->value );
}

void sigusr2_handler(int signum)
{
    flagEndReading= 1;
}

int main(int argc, char* argv[])
{
    sigset_t mask;
    //signal handling for sigusr2
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);

    sigprocmask(SIG_BLOCK, &mask, NULL);
    struct sigaction sa;
    sa.sa_handler = sigusr2_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    //fai in modo che esegua sempre unlink(SOCKNAME);
    int maxworkers = atoi(argv[1]); //should be setted up when launched to the max workers number
    int actualworkers = 0;
    int i = 0; //counter
    int fdSKT;
    int fdC; 
    int fd;
    fd_set set,rdset;
    struct sockaddr_un sa;

    res *resultArray;
    char **filenameArray;
    int arraySize;

    int nread;
    char buffer[BUFFERSIZE];
    long tmplong;
    char *tmpname;

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(SOCKNAME) //should make sure the socket file is gone when closing
    bind(fdSKT, (struct sockaddr *) &sa, sizeof(sa));
    listen(fdSKT, maxworkers); //somaxconn should be set on worker number
    if(fdSKT > actualworkers)   actualworkers=fdSKT;
    FD_ZERO(&set);
    FD_SET(fdSKT, &set);
    while(!flagEndReading)
    {
        rdset=set;
        if( select(actualworkers+1,&rdset,NULL,NULL,NULL)==-1 )
        {
            //gestione errore
        }
        else
        {
            for(fd=0;fd<=actualworkers; fd++)
            {
                if(FD_ISSET(fd,&rdset))
                {
                    if(fd == fdSKT)     //socket connect ready
                    {
                        fdC = accept(fdSKT, NULL, 0);
                        FD_SET(fdC, &set);
                        if(fdC>actualworkers)   actualworkers=fdC;
                    }
                    else        //IO socket ready
                    {   
                        if(flagEndReading)
                        {
                            break;  //flag end reading setted mean the collector needs to stop using the socket and just print the result
                        }
                        nread=read(fd,buffer,BUFFERSIZE);   //do per scontato che sizeof(long sia 8)
                        if(nread!=0)
                        {
                            printf("collector ha ricevuto: %s",buffer);
                            for(i = nread-1;i>=0;i--)
                            {
                                if(buffer[i] == '/')    //fine numero
                                {
                                    arraySize++;
                                    resultArray = realloc(resultArray,arraySize * sizeof(res));
                                    resultArray[arraySize-1].value = atol(buffer+(i+1));
                                    resultArray[arraySize - 1].name = (char*) calloc(i+1,sizeof(1));
                                    strncpy(filenameArray[arraySize - 1],buffer,i);
                                }
                            }
                        }
                        else
                        {
                            //EOF
                        }
                    }
                }
            }
        }
    }
    close(fdSKT);
    qsort(resultArray,arraySize,sizeof(res),compare);
    for(i=0;i<arraySize;i++)
    {
        printf("%ld %s",resultArray[i].value,resultArray[i].name);
        free(resultArray[i].name);
        free(resultArray);
    }
    free(resultArray);
}
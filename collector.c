#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <sys/select.h>
#include "workerThread.h"

#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); exit(EXIT_FAILURE); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror(m); exit(EXIT_FAILURE); }
#define ec_zero(s,m) \
    if((s) != 0) { perror(m); exit(EXIT_FAILURE); }

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
    ec_meno1(sigemptyset(&mask),errno);
    ec_meno1(sigaddset(&mask, SIGUSR2),errno);

    ec_meno1(sigprocmask(SIG_BLOCK, &mask, NULL),errno);
    struct sigaction siga;
    siga.sa_handler = sigusr2_handler;
    ec_meno1(sigemptyset(&siga.sa_mask),errno);
    siga.sa_flags = 0;
    ec_meno1(sigaction(SIGUSR2, &siga, NULL),errno);

    ec_meno1(sigprocmask(SIG_UNBLOCK, &mask, NULL),errno);

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
    ec_meno1(fdSKT,errno); 
    ec_meno1(unlink(SOCKNAME),errno); //should make sure the socket file is gone when closing
    ec_meno1(bind(fdSKT, (struct sockaddr *) &sa, sizeof(sa)),errno);
    ec_meno1(listen(fdSKT, maxworkers),errno); //somaxconn should be set on worker number
    if(fdSKT > actualworkers)   actualworkers=fdSKT;
    FD_ZERO(&set);
    FD_SET(fdSKT, &set);
    while(!flagEndReading)
    {
        rdset=set;
        ec_meno1(select(actualworkers+1,&rdset,NULL,NULL,NULL),errno);
        for(fd=0;fd<=actualworkers; fd++)
        {
            if(FD_ISSET(fd,&rdset))
            {
                if(fd == fdSKT)     //socket connect ready
                {
                    fdC = accept(fdSKT, NULL, 0);
                    ec_meno1(fdC,errno);
                    FD_SET(fdC, &set),errno;
                    if(fdC>actualworkers)   actualworkers=fdC;
                }
                else        //IO socket ready
                {   
                    if(flagEndReading)
                    {
                        break;  //flag end reading setted mean the collector needs to stop using the socket and just print the result
                    }
                    nread=read(fd,buffer,BUFFERSIZE);   //do per scontato che sizeof(long sia 8)
                    ec_meno1(nread,errno);
                    if(nread!=0)
                    {
                        for(i = nread-1;i>=0;i--)
                        {
                            if(buffer[i] == '/')    //fine numero
                            {
                                arraySize++;
                                resultArray = realloc(resultArray,arraySize * sizeof(res));
                                ec_null(resultArray,"collector's realloc for resultArray failed");

                                resultArray[arraySize-1].value = atol(buffer+(i+1));
                                resultArray[arraySize - 1].name = (char*) calloc(i+1,sizeof(1));
                                ec_null(resultArray[arraySize - 1].name,"collector calloc failed for file names");
                                strncpy(filenameArray[arraySize - 1],buffer,i);
                            }
                        }
                    }
                }
            }
        }
    }
    ec_meno1(close(fdSKT),errno);
    qsort(resultArray,arraySize,sizeof(res),compare);
    for(i=0;i<arraySize;i++)
    {
        printf("%ld %s\n",resultArray[i].value,resultArray[i].name);
    }
    for(i=0;i<arraySize;i++)
    {
        free(resultArray[i].name);
    }
    free(resultArray);
}
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>


volatile sig_atomic_t flagEndReading= 0;

typedef struct supp
{
    long value;
    char *name;
}   res;

int compare( const void* a, const void* b)
{
   if( ((res*)a)->value < ((res*)b)->value )
   {
    return -1;
   }
   else if( ((res*)a)->value > ((res*)b)->value )
   {
    return 1;
   }
   else
   {
    return 0;
   }
}

void sigusr2_handler(int signum)
{
    flagEndReading= 1;
}

static int sendACK(int fdC)
{
    char ack[3]={'a','c','k'};
    int bytesWritten = 0;
    int totalBytesWritten = 0;
    while( totalBytesWritten < 3 )
    {
        errno = 0;
        bytesWritten = 0;
        bytesWritten=write(fdC, ack, 3);
        if(bytesWritten == -1) return -1;
        totalBytesWritten+= bytesWritten;
        //printf("collector ha scritto %d/%d\n",totalBytesWritten,3);
    }
    return 0;
}

static int safeSocketRead(int fdC, void* buffer, int size)
{
    int byteRead=0;
    int totalByteRead=0;
    do
    {
        errno=0;
        byteRead=0;
        byteRead=read(fdC,buffer,size);
        if (byteRead==-1)
        {
            //printf("collector read ha dato errore\n");
            close(fdC);
            return -1;
        }
        if(byteRead==0)
        {
            //printf("collector read dice EOF\n");
            flagEndReading = 1;
            return 0;
        }
        totalByteRead+=byteRead;
        //printf("collector ha letto %d a questo giro, %d sommando le iterazioni, %d dovrebbero arrivare\n",byteRead,totalByteRead,size);
    } while (totalByteRead<size);

    if( sendACK(fdC) == -1 ) return -1;

    return 0;
}

static void freeResultsArray(res **resultArray, int arraySize)
{
    int i;
    for(i=0;i<arraySize;i++)
    {
        free((*resultArray)[i].name);
    }
    free(*resultArray);
}

static void printOutput(res *resultArray, int arraySize)
{
    int i;
    qsort(resultArray,arraySize,sizeof(res),compare);
    //printf("collector ha ricevuto elementi in numero: %d\n",arraySize);

    //printf("collector ha raccolto %d elementi\n",arraySize);

    for(i=0;i<arraySize;i++)
    {
        printf("%ld %s\n",resultArray[i].value,resultArray[i].name);
    }
}

int main(int argc, char* argv[])
{
	//printf("collector avviato\n");//testing

    sigset_t blockset;

    // block all signals
    sigfillset(&blockset);

    // set the signal mask to block all signals 
    sigprocmask(SIG_SETMASK, &blockset, NULL);
    //signal handling for sigusr2
    struct sigaction siga;
    siga.sa_handler = sigusr2_handler;
    ec_meno1(sigemptyset(&siga.sa_mask),(strerror(errno)));
    siga.sa_flags = 0;
    ec_meno1(sigaction(SIGUSR2, &siga, NULL),(strerror(errno)));
    
    sigemptyset(&blockset);
    sigaddset(&blockset,SIGUSR2);
    sigprocmask(SIG_UNBLOCK,&blockset,NULL);

    int fdSKT;
    int fdC; 
    struct sockaddr_un sa;

    res *resultArray;
    int arraySize=0;    //array of data

    int nameSize;
    char* fileName;
    long fileValue;
    int optionActive=1;

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    ec_meno1(fdSKT,(strerror(errno))); 

    ec_meno1(setsockopt(fdSKT,SOL_SOCKET,SO_REUSEADDR,&optionActive,sizeof(optionActive)),strerror(errno));
    //printf("collector prova a bindare\n");
    ec_meno1(bind(fdSKT, (struct sockaddr *) &sa, sizeof(sa)),(strerror(errno)));
    //printf("collector prova il listen\n");
    ec_meno1(listen(fdSKT, 1),(strerror(errno))); 

    fdC = accept(fdSKT, NULL, 0);
    ec_meno1(fdC,"collector morto sull'accept");

    //printf("collector entra nel suo loop\n");
    while(!flagEndReading)
    {
        fileName= NULL;
        nameSize=0u;
        fileValue = 0;

        if( safeSocketRead(fdC,&nameSize,sizeof(int)) == -1)
        {
            //printf("collector read fatal error,ecco output fin'ora\n");
            printOutput(resultArray,arraySize);
            freeResultsArray(&resultArray, arraySize);
            close(fdC);
            close(fdSKT);
            return 0;
            //TODO: handle error
        }

        fileName = malloc(nameSize+1);
        ec_null(fileName,"collector malloc failed for file name");
        memset(fileName,0,nameSize+1);

        ec_null(fileName,"collector failed to malloc");

        if( safeSocketRead(fdC,fileName,nameSize) == -1)
        {
            //printf("collector read fatal error\n");
            return 0;
            //TODO: handle error
        }

        if( safeSocketRead(fdC,&fileValue,8u) == -1)
        {
            //printf("collector read fatal error\n");
            return 0;
            //TODO: handle error
        }

        arraySize++;
        checked_realloc(&resultArray,arraySize, sizeof(res));     //realloc for result array

        resultArray[arraySize-1].value=fileValue;

        resultArray[arraySize-1].name = fileName;

        //printf("collector ha raccolto: %s\n",resultArray[arraySize - 1].name);
    }

    //printf("collector e' fuori dal suo loop\n");
    printOutput(resultArray,arraySize);

    close(fdC);
    close(fdSKT);
    freeResultsArray(&resultArray, arraySize);

    //printf("---collector chiude---\n");
    return 0;
}

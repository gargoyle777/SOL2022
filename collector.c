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

#define ec_meno1(s,m) \
    if((s) == -1) { perror("collector"); exit(EXIT_FAILURE); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror("collector"); exit(EXIT_FAILURE); }
#define ec_zero(s,m) \
    if((s) != 0) { perror("collector"); exit(EXIT_FAILURE); }

#define SOCKNAME "./farm.sck"
#define BUFFERSIZE 265
#define UNIX_PATH_MAX 255

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

	printf("collector avviato\n");//testing

    sigset_t mask;
    //signal handling for sigusr2
    ec_meno1(sigemptyset(&mask),(strerror(errno)));
    ec_meno1(sigaddset(&mask, SIGUSR2),(strerror(errno)));
    ec_meno1(sigprocmask(SIG_BLOCK, &mask, NULL),(strerror(errno)));
    struct sigaction siga;
    siga.sa_handler = sigusr2_handler;
    ec_meno1(sigemptyset(&siga.sa_mask),(strerror(errno)));
    siga.sa_flags = 0;
    ec_meno1(sigaction(SIGUSR2, &siga, NULL),(strerror(errno)));
    ec_meno1(sigprocmask(SIG_UNBLOCK, &mask, NULL),(strerror(errno)));

    int maxworkers = atoi(argv[1]); //should be setted up when launched to the max workers number
    int maxFD = 0;
    int i = 0; //counter
    int fdSKT;
    int fdC; 
    int fd;
    fd_set set,rdset;
    struct sockaddr_un sa;

    int numworkers=0;
    int *allWorkersFd;
	char* ack="ACK";
    res *resultArray;
    int arraySize;

    int nread;
    char buffer[BUFFERSIZE];
    long tmplong;
    char *tmpname;
    int c;

    ec_null(allWorkersFd = malloc(maxworkers*sizeof(int)),"failed to malloc allWorkersFd");
    
    for(c=0;c<maxworkers;c++)   //init to -1
    {
        allWorkersFd[c]=-1;
    }

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    ec_meno1(fdSKT,(strerror(errno))); 

    //ec_meno1(unlink(SOCKNAME),(strerror(errno))); //should make sure the socket file is gone when closing TESTING

    printf("collector prova a bindare\n");
    ec_meno1(bind(fdSKT, (struct sockaddr *) &sa, sizeof(sa)),(strerror(errno)));
    printf("collector prova il listen\n");
    ec_meno1(listen(fdSKT, maxworkers),(strerror(errno))); //somaxconn should be set on worker number
    if(fdSKT > maxFD)   maxFD=fdSKT;
    printf("collector entra nel suo loop\n");
    while(!flagEndReading)
    {
        FD_ZERO(&rdset);
        if(numworkers < maxworkers) FD_SET(fdSKT,&rdset);
        for(c=0;c<maxworkers;c++)
        {
            if(allWorkersFd[c] > -1)    FD_SET(allWorkersFd[c],&rdset);
        }

        ec_meno1(select(maxFD+1,&rdset,NULL,NULL,NULL),(strerror(errno)));
        printf("collector sopravvissuto al select\n");

        if(FD_ISSET(fdSKT,&rdset)) //socket connect ready
        {
            printf("collector accettera' una connesione\n");
            fdC = accept(fdSKT, NULL, 0);
            ec_meno1(fdC,(strerror(errno)));
            for(c=0;c<maxworkers;c++)
            {
                if(allWorkersFd[c]==-1)
                {
                    allWorkersFd[c] = fdC;
                    c=maxworkers; 
                    numworkers+=1;
                }
            }
            if(fdC > maxFD)   maxFD=fdC;
        }
        for(c=0;c<maxworkers;c++)
        {
            if(flagEndReading)
            {
                break;  //flag end reading setted mean the collector needs to stop using the socket and just print the result
            }
            if(FD_ISSET(allWorkersFd[c],&rdset))
            {
                memset(buffer, 0, sizeof(BUFFERSIZE));      //zero the memory
                while((nread=read(fd,buffer,BUFFERSIZE))!=0)
                {
                    ec_meno1(nread,errno);
                }

                arraySize++;
                resultArray = realloc(resultArray,arraySize * sizeof(res));     //realloc for result array
                ec_null(resultArray,"collector's realloc for resultArray failed");

                memcpy( &(resultArray[arraySize-1].value), &(buffer[strlen(buffer)- 8]) , 8); //value is copied in the structure
                resultArray[arraySize-1].name = (char*) malloc(strlen(buffer)-7);
                ec_null(resultArray[arraySize - 1].name,"collector malloc failed for file name");
                memset(resultArray[arraySize - 1].name, 0, sizeof(strlen(buffer)-7));   //name is zeroed
                memcpy(resultArray[arraySize - 1].name,buffer,strlen(buffer)-8);        //name is saved in the structure
                printf("colector ha raccolto %s",resultArray[arraySize - 1].name);

                //start ack
                ec_meno1(write(fdSKT, ack, strlen(ack)),errno);    
                printf("collector ha risposto %s",ack);
            }
        }
    }

    printf("collector e' fuori dal suo loop\n");
    for(c=0;c<maxworkers;c++)
    {
        if(allWorkersFd[c]!=-1) ec_meno1(close(allWorkersFd[c]),"collecotr failed to close a socket with a worker");
    }

    ec_meno1(close(fdSKT),("collecotr failed to close the socket for accepting connection"));
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
    free(allWorkersFd);
    ec_meno1(unlink(SOCKNAME),"collector error when unlinking the socket"); //should make sure the socket file is gone when closing TESTING
}

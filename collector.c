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
    if((s) == -1) { perror(m); exit(EXIT_FAILURE);}    

#define ec_null(s,m) \
    if((s) == NULL) { perror("collector ec_null"); exit(EXIT_FAILURE); }
#define ec_zero(s,m) \
    if((s) != 0) { perror("collector ec_zero"); exit(EXIT_FAILURE); }

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

void checked_realloc(res **ptr, int length, size_t size)
{
    errno=0;
    if(length==1) 
    {
        printf("provo malloc \n");
        *ptr=malloc(length*size);
    }
    else 
    {
        printf("provo realloc \n");
        *ptr=realloc(*ptr, length*size);
    }
    ec_null(*ptr,"checked_realloc fallita");
    printf("riuscita\n");
}

int main(int argc, char* argv[])
{

	printf("collector avviato\n");//testing

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
	char ack[4]="ACK";
    res *resultArray;
    int arraySize;

    int nread=1;
    char buffer[BUFFERSIZE];
    long tmplong;
    char *tmpname;
    int c;
    int accumulator=0;

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

        if(select(maxFD+1,&rdset,NULL,NULL,NULL)==-1)
        {
            //tewpo di finire
            for(c=0;c<maxworkers;c++)
            {
                if(allWorkersFd[c]!=-1)
                {
                    ec_meno1(close(allWorkersFd[c]),"collector failed to close a socket with a worker");
                    printf("collector ha chiuso il fd in posizione %d\n",c);
                }
            }

            ec_meno1(close(fdSKT),("collector failed to close the socket for accepting connection"));
            qsort(resultArray,arraySize,sizeof(res),compare);
            printf("collector ha raccolto %d elementi",arraySize);
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
            printf("---collector chiude dal select---\n");
            return 2;   //testing

        }
        printf("collector sopravvissuto al select\n");

        if(FD_ISSET(fdSKT,&rdset)) //socket connect ready
        {
            printf("collector accettera' una connesione\n");
            fdC = accept(fdSKT, NULL, 0);
            ec_meno1(fdC,"collector morto sull'accept");
            for(c=0;c<maxworkers;c++)
            {
                if(allWorkersFd[c]==-1)
                {
                    printf("nuova connessione messa in posizione %d\n",c);
                    allWorkersFd[c] = fdC;
                    c=maxworkers; 
                    numworkers+=1;
                }
                else{
                    printf(" gia' presente un file descriptor in posizione %d\n", c);
                }
            }
            if(fdC > maxFD)   maxFD=fdC;
        }
        for(c=0;c<maxworkers;c++)
        {
            printf("collector fa il check del file descriptor in posizione %d\n",c);
            if(flagEndReading)
            {
                break;  //flag end reading setted mean the collector needs to stop using the socket and just print the result
            }
            if(FD_ISSET(allWorkersFd[c],&rdset))
            {
                printf("collector si prepara a leggere dal file descriptor %d\n",c);
                memset(buffer, 0, BUFFERSIZE);      //zero the memory
                accumulator=0;
                do
                {
                    errno=0;
                    nread=read(fd,buffer,BUFFERSIZE);
                    if (nread==-1)
                    {
                        close(allWorkersFd[c]);
                        allWorkersFd[c]=-1; //close the socket with him
                        break;
                    }
                    printf("collector ha letto %d a questo giro, %d in totale",nread,accumulator );
                    accumulator+=nread;
                } while (accumulator<265 && nread>0);
                printf("collector survived read\n");
                arraySize++;
                checked_realloc(&resultArray,arraySize, sizeof(res));     //realloc for result array
                ec_null(resultArray,"collector's realloc for resultArray failed");

                memcpy( &(resultArray[arraySize-1].value), &(buffer[BUFFERSIZE - 8]) , 8); //value is copied in the structure
                resultArray[arraySize-1].name = (char*) malloc(strlen(buffer)+1); //there are at least a pair of \0 bewtween name and long value
                ec_null(resultArray[arraySize - 1].name,"collector malloc failed for file name");
                memset(resultArray[arraySize - 1].name, 0, strlen(buffer)+1);   //name is zeroed
                memcpy(resultArray[arraySize - 1].name,buffer,strlen(buffer));        //name is saved in the structure
                printf("colector ha raccolto %s",resultArray[arraySize - 1].name);

                //start ack
                ec_meno1(write(fdSKT, ack, 4),"collector morto per write fallita");    
                printf("collector ha risposto %s",ack);
            }
        }
    }

    printf("collector e' fuori dal suo loop\n");
    for(c=0;c<maxworkers;c++)
    {
        if(allWorkersFd[c]!=-1)
        {
            ec_meno1(close(allWorkersFd[c]),"collector failed to close a socket with a worker");
            printf("collector ha chiuso il fd in posizione %d\n",c);
        }
    }

    ec_meno1(close(fdSKT),("collector failed to close the socket for accepting connection"));
    qsort(resultArray,arraySize,sizeof(res),compare);
    printf("collector ha raccolto %d elementi",arraySize);
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
    printf("---collector chiude---\n");
    return 3;   //testing
}

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
#define BUFFERSIZE 277
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
	printf("collector avviato con max connessioni = %s\n",argv[1]);//testing
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
    int actualworkers = 0;
    int i = 0; //counter
    int fdSKT;
    int fdC; 
    int fd;
    fd_set set,rdset;
    struct sockaddr_un sa;

	char* ack="ok";
    res *resultArray;
    int arraySize;

    int nread;
    char buffer[BUFFERSIZE];
    long tmplong;
    char *tmpname;

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    ec_meno1(fdSKT,(strerror(errno))); 
    //ec_meno1(unlink(SOCKNAME),"collector errore s unlink del socket"); //should make sure the socket file is gone when closing TESTING
    printf("collector prova a bindare\n");
    ec_meno1(bind(fdSKT, (struct sockaddr *) &sa, sizeof(sa)),(strerror(errno)));
    printf("collector prova il listen\n");
    ec_meno1(listen(fdSKT, maxworkers),(strerror(errno))); //somaxconn should be set on worker number
    if(fdSKT > actualworkers)   actualworkers=fdSKT;
    FD_ZERO(&set);
    FD_SET(fdSKT, &set);
    printf("collector entra nel suo loop, flagendreading= %d\n",flagEndReading);
    while(!flagEndReading)
    {
        rdset=set;
        if(select(actualworkers+1,&rdset,NULL,NULL,NULL)==-1)
        {
        perror(strerror(errno));
        }
        for(fd=0;fd<=actualworkers; fd++)
        {
            if(FD_ISSET(fd,&rdset))
            {
                if(fd == fdSKT)     //socket connect ready
                {
                	printf("collector accettera' una connesione\n");
                    fdC = accept(fdSKT, NULL, 0);
                    ec_meno1(fdC,(strerror(errno)));
                    FD_SET(fdC, &set);
                    if(fdC>actualworkers)   actualworkers=fdC;
                }
                else        //IO socket ready
                {   
                    //printf("collector sta per leggere un res\n");
                    nread=read(fd,buffer,BUFFERSIZE);   //do per scontato che sizeof(long sia 8)
                    ec_meno1(nread,(strerror(errno)));
                    if(nread!=0)
                    {
                        for(i = nread-2;i>=0;i--)		//-1 sarebbe l'ultimo carattere, che e' null termination
                        {
                            if(buffer[i] == '/')    //fine numero
                            {
                                arraySize++;
                                resultArray = realloc(resultArray,arraySize * sizeof(res));
                                ec_null(resultArray,"collector's realloc for resultArray failed");

                                resultArray[arraySize-1].value = atol(buffer+(i+1));
                                resultArray[arraySize-1].name = (char*) calloc(i+1,sizeof(char));	//null termination andra' al posto di /
                                ec_null(resultArray[arraySize - 1].name,"collector calloc failed for file names");
                                strncpy(resultArray[arraySize-1].name,buffer,i);
                            }
                        }
                        printf("collector sta per mandare un ACK\n");
            		write(fd,ack,3);
                    }
                    else
                    {
                    	//printf("collector non ha letto niente\n");      	
                    }
                }
            }
        }
    }
    printf("collector e' fuori dal suo loop\n");
    for(fd=0;fd<=actualworkers;fd++)
    {
    	if(FD_ISSET(fd,&set)){
    		ec_meno1(close(fd),"fail");
    	}
    }
    ec_meno1(close(fdSKT),(strerror(errno)));
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
    ec_meno1(unlink(SOCKNAME),"collector errore s unlink del socket"); //should make sure the socket file is gone when closing TESTING
}

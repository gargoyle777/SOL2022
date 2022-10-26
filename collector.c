#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#define SOCKNAME "./mysock"
#define BUFFERSIZE 277
#define UNIX_PATH_MAX 108

typedef struct supp
{
    long value;
    char *name;
}   res;

int compare( const void* a, const void* b)
{
    return ( ((res*)a)->value - ((res*)b)->value );
}


int main(int argc, char* argv[])
{
    int maxworkers = 10; //should be setted up when launched to the max workers number
    int actualworkers = 0;
    int i = 0; //counter
    int fdSKT;
    int *fdC; //fdc should turn into an array to host multiple connections
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
    fdC = malloc(maxworkers * sizeof(int));

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(fdSKT, (struct sockaddr *) &sa, sizeof(sa));
    listen(fdSKT, maxworkers); //somaxconn should be set on worker number
    if(fdSKT > actualworkers)   actualworkers=fdSKT;
    FD_ZERO(&set);
    FD_SET(fdSKT, &set);
    while(1)
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
                        fdSET(fdC, &set);
                        if(fdC>actualworkers)   actualworkers=fdC;
                    }
                    else        //IO socklet ready
                    {   
                        nread=read(fd,buffer,BUFFERSIZE);   //do per scontato che sizeof(long sia 8)
                        if(nread!=0)
                        {
                            for(i = nread-1;i>=0;i--)
                            {
                                if(buffer[i] == '/')    //fine numero
                                {
                                    arraySize++;
                                    resultArray = realloc(arraySize * sizeof(res));
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
        free(resultArray[i]);
    }
    free(resultArray);
    free(fdC);  //controlla di aver chiuso tutto e cancella il file del socket
}
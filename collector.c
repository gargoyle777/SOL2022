#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#define SOCKNAME "./mysock"

#define UNIX_PATH_MAX 108

int main(int argc, char* argv[])
{
    int maxworkers = 10; //should be setted up when launched to the max workers number
    int i = 0; //counter
    int fdSKT;
    int *fdC; //fdc should turn into an array to host multiple connections
    const struct sockaddr_un sa;

    fdc = malloc(maxworkers * sizeof(int));

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    fdSKT = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(fd_skt, (struct sockaddr *) &sa, sizeof(sa));
    listen(fdSKT, maxworkers); //somaxconn should be set on worker number
    for(i=0;i<maxworkers;i++)
    {
        fdC[i] = accept(fdSKT,NULL,0); //check psa != null for success
    }
    
    
}
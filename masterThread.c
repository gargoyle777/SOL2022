//TODO: check for error when accessing the array

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "workerThread.h"

#define ec_meno1(s,m) \
    if((s) == -1) { perror("master"); exit(EXIT_FAILURE); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror(m); exit(EXIT_FAILURE); }
#define ec_zero(s,m) \
    if((s) != 0) { perror(m); exit(EXIT_FAILURE); }

volatile sig_atomic_t flagEndFetching= 0;
volatile sig_atomic_t flagSIGUSR1 = 0;


//error handler function
void handle_sighup(int sig)
{
    flagEndFetching = 1;
}

void handle_sigint(int sig)
{
    flagEndFetching = 1;
}

void handle_sigquit(int sig)
{
    flagEndFetching = 1;
}

void handle_sigterm(int sig)
{
    flagEndFetching = 1;
}

void handle_sigusr1(int sig)
{
    flagEndFetching = 1;
    flagSIGUSR1 = 1;
}

void* checked_realloc(void *ptr, size_t size)
{
    errno =0;
    if(ptr==NULL) return ptr=malloc(size);
    else return ptr=realloc(ptr,size);
}

void directoryDigger(char* path, char*** fileList, int* fileListSize)     //recursive approach TODO: gestione errore troppo particolareggiata
{
    DIR* openedDir;
    struct dirent* freshDir;
    char tmpString[256];
    printf("scavo in una directory\n"); //testing

    errno = 0;
    ec_null(openedDir = opendir(path),"opendir failed ");

    while ((freshDir=readdir(openedDir)) != NULL) 
    {
        memset(tmpString,0,strlen(tmpString));
        memcpy(tmpString,path,strlen(path));
        tmpString[strlen(path)] = '/';
        memcpy(&(tmpString[strlen(path) +1 ]),freshDir->d_name,strlen(freshDir->d_name));
        if(freshDir->d_type == DT_DIR)
        {
            directoryDigger(tmpString,fileList,fileListSize);            
        }
        else if(freshDir->d_type == DT_REG)
        {
            (* fileListSize) ++;
            *fileList = checked_realloc(*fileList,(* fileListSize) * sizeof(char*));
            ec_null(*fileList,"realloc fallita, fileList non allocata");
            *fileList[(* fileListSize) - 1] = malloc(strlen(tmpString));
            ec_null(*fileList[(* fileListSize) - 1],"malloc fallita, elemento di fileList non allocato");
            strcpy(*fileList[(* fileListSize) - 1], tmpString); 
            printf("testing0\n");//testing
            printf("%s\n",freshDir->d_name); //testing
        }
    }
    errno=0;
    ec_meno1(closedir(openedDir),"failure on close dir");
}

int main(int argc, char* argv[])
{
	printf("sto iniziando il main\n");//testing

    char baseDir[256];
    ec_null(getcwd(baseDir,256),"couldn't retrieve current working directory");
    //for masking
    sigset_t set;
    struct sigaction sa;

    int queueSize = 0;
    struct queueEl * tmpPointer;
    pthread_mutex_init(&mtx,NULL);
    ec_zero(pthread_cond_init(&queueNotFull, NULL),"pthread_cond_init failed on condition queueNotFull");
    ec_zero(pthread_cond_init(&queueNotEmpty, NULL),"pthread_cond_init failed on condition queueNotEmpty");


    pthread_t *tSlaves;

    int i;  //counter
    char **fileList;
    int sizeFileList = 0;
    char **dirList;
    int sizeDirList = 0;
    int ac; //counts how many argument i have checked
    int nthread = 4;       //default is 4
    char* charnthread="4";
    int qlen = 8;       //default is 8
    int dirFlag = 0;        //check for directory
    int delay = 0;        //default is 0

    //start signal masking
    ec_meno1(sigfillset(&set),(strerror(errno)));
    ec_meno1(pthread_sigmask(SIG_SETMASK,&set,NULL),(strerror(errno))); 

    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handle_sighup;
    ec_meno1(sigaction(SIGHUP,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigint;
    ec_meno1(sigaction(SIGINT,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigquit;
    ec_meno1(sigaction(SIGQUIT,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigterm;
    ec_meno1(sigaction(SIGTERM,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigusr1;
    ec_meno1(sigaction(SIGUSR1,&sa,NULL),(strerror(errno)));

    ec_meno1(sigemptyset(&set),(strerror(errno)));
    ec_meno1(pthread_sigmask(SIG_SETMASK,&set,NULL),(strerror(errno)));
    //END signal handling

	printf("sto per parsare\n");//testing
    //START parsing 

    for(ac = 1; ac<argc; ac++) //0 is filename          TODO:should check if file list is 0 only when -d is present
    {
        if( argv[ac][0] == '-')
        {
            switch ( argv[ac][1] )
            {
            case 'n':       //number of thread
                ac++;
                nthread = atoi(argv[ac]);
                charnthread=argv[ac];
                break;
            case 'q':       //concurrent line's length
                ac++;
                qlen = atoi(argv[ac]);
                break;
            case 'd':       //directory name
                dirFlag = 1;    
                break;
            case 't':       //delay (default is 0)
                ac++;
                delay = atoi(argv[ac]);
                break;            
            default:
                break;
            }
        }
        else
        {   
            if(dirFlag)
            {
                sizeDirList ++;
                dirList = checked_realloc(dirList,sizeDirList * sizeof(char*));
                ec_null(dirList,"realloc fallita, dirList non allocata");
                dirList[sizeDirList - 1] = calloc(strlen(argv[ac]) +1, sizeof(char));
                ec_null(dirList[sizeDirList - 1] ,"calloc fallita, elemento di dirList non allocato");
                strcpy(dirList[sizeDirList - 1], argv[ac]);

            }
            else
            {
            	printf("file trovato nell'argomento %d\n",ac); //testing
                sizeFileList ++;

                fileList = checked_realloc(fileList, sizeFileList * sizeof(char*));
                ec_null(fileList,"realloc fallita, fileList non allocata");
                fileList[sizeFileList - 1] = malloc(strlen(argv[ac]) + strlen(baseDir)+1);
                ec_null(fileList[sizeFileList - 1] ,"malloc fallita, elemento di fileList non allocato");

                memcpy(fileList[sizeFileList - 1],baseDir,strlen(baseDir));
                fileList[sizeFileList - 1][strlen(baseDir)] ='/';
                memcpy(&(fileList[sizeFileList-1][strlen(baseDir)+1]),argv[ac],strlen(argv[ac]));
                printf("%s\n",fileList[sizeFileList - 1]); //testing
            }
        }
    }

    //END parsing

    //START collector process
    char *collectorPath="./collector";
    char *collectorArgs[]={collectorPath,charnthread,NULL};
    int pid;
    pid= fork();
    ec_meno1(pid,(strerror(errno)));
    if(pid==0)  //figlio
    {
        execv(collectorPath,collectorArgs);
    }
    //END of collector process

    //START directory exploration

    for(i=0; i<sizeDirList; i++)
    {
    	printf("sto per iniziare a scavare");
        directoryDigger(dirList[i], &fileList, &sizeFileList);
    }

    for(i=0; i<sizeDirList; i++)
    {
        free( dirList[i]);
    }
    free(dirList);
    
    //END of directory exploration

    //START of threading
    tSlaves = malloc(nthread * sizeof(pthread_t));
    ec_null(tSlaves,"malloc fallita, tSalves non allocati");
    for(i=0; i<nthread;i++)
    {
        ec_zero(pthread_create(&(tSlaves[i]), NULL, &worker, NULL),"ptread_create failure");  

    }
    
    //START producing
	printf("master inizia a produrre,%d elementi in lista\n",sizeFileList);
    for(i=0;i<sizeFileList;i++)
    {
        ec_zero(pthread_mutex_lock(&(mtx)),"pthread_mutex_lock failed with mtx");
        while(queueSize>=qlen)  //full queue
        {
            ec_zero(pthread_cond_wait(&(queueNotFull),&(mtx)),"pthread_cond_wait failed on queueNotFull");
        }
        if(flagEndFetching)     //setted
        {
            break;
        }
        if(queueSize == 0)
        {
        	printf("master inizia a mettere un elemento in testa\n");

            queueHead = malloc(1*sizeof(struct queueEl));
            ec_null(queueHead,"malloc of queueHead failed");
            queueHead->filename = fileList[i];     //TODO: check ifshallow copy is enough
            queueSize = 1;

            printf("master ha messo un elemento in testa\n");
        }
        else
        {
            //TODO:check if this is safe enough, or we are risking stuff
            //probabilmente devo geestire tutto un puntatore indietro

            tmpPointer = queueHead->next;

            while(tmpPointer != NULL)
            {
                tmpPointer = tmpPointer->next;
            }
            tmpPointer = malloc(1*sizeof(struct queueEl));
            ec_null(tmpPointer,"malloc of an element of the queue failed");
            tmpPointer->filename = fileList[i];     //shallow copy is enough
            queueSize++;
        }
        ec_zero(pthread_cond_signal(&queueNotEmpty),"pthread_cond_signal failed with queueNotEmpty");
        printf("master ha segnalato su queue not empty\n");
        ec_zero(pthread_mutex_unlock(&mtx),"pthread_mutex_unlock failed with mtx");
        printf("master ha lasciato il lock\n");
    }
   
    ec_zero(pthread_mutex_lock(&mtx),"pthread_mutex_lock failed with mtx, before checking flagSIGUSR1");

    if(flagSIGUSR1)
    {
        masterExitReq = 2;
        kill(pid,SIGUSR2);
    }
    else{
        masterExitReq = 1;
    }
    ec_zero(pthread_cond_broadcast(&(queueNotEmpty)),"pthread_cond_broadcast failed");  //to let every thread to finish its cleaning
    ec_zero(pthread_mutex_unlock(&(mtx)),"pthread_mutex_unlock failed with mtx, after checking flgSIGUSR1");

    for(i=0; i<nthread;i++)
    {
    	printf("master sta iniziando a fare i join\n");
        ec_zero(pthread_join(tSlaves[i], NULL),"pthread_join failed");
    }
    printf("master ha finito di fare i join\n");
    for(i=0;i<sizeFileList;i++)
    {
        free(fileList[i]);
    }
    free(fileList);
    free(tSlaves);

    printf("master manda il segnale di fermarsi a collector\n");
    kill(pid,SIGUSR2);

}

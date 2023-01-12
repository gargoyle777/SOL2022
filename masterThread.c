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
    if((s) == -1) { perror(m); exit(EXIT_FAILURE); }    
#define ec_null(s,m) \
    if((s) == NULL) { perror(m); exit(EXIT_FAILURE); }
#define ec_zero(s,m) \
    if((s) != 0) { perror(m); exit(EXIT_FAILURE); }

typedef struct queueEl node;
typedef struct arguments workerArgs;

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

void directoryDigger(char* dir, char** fileList, int* fileListSize)     //recursive approach TODO: gestione errore troppo particolareggiata
{
    DIR* oDir;
    struct dirent* rDir;
    if (access(dir, F_OK) == 0) //file exist
    {
        errno = 0;
        oDir = opendir(dir);
        if( oDir == NULL)     //file exist but its not a directory
        {   
            if(errno == ENOTDIR)        //not a directory
            {
            (* fileListSize) ++;
                fileList = realloc(fileList,(* fileListSize) * sizeof(char*));
                ec_null(fileList,"realloc fallita, fileList non allocata");
                fileList[(* fileListSize) - 1] = calloc(strlen(dir) +1, sizeof(char));
                ec_null(fileList[(* fileListSize) - 1],"calloc fallita, elemento di fileList non allocato");
                strcpy(fileList[(* fileListSize) - 1], dir); 
            }       
            else        //another type of error
            {
                perror(errno);
            }  
        }
        else        //file exist and its a directory or different type of error
        {
            errno = 0;
            while( (NULL!= (rDir = readdir(oDir))) )
            {
                directoryDigger(rDir->d_name, fileList, fileListSize);
                //Check errno
            }
            if(errno == 0)  //TODO should close in any case
            {
                closedir(oDir);
            }
            
        }
    } 
    else 
    {
        perror(errno);
    }
}

int main(int argc, char* argv[])
{
    //for masking
    sigset_t set;
    struct sigaction sa;

    int queueSize = 0;
    node * tmpPointer;
    node * queueHead;
    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx,NULL);
    pthread_cond_t queueNotFull;
    pthread_cond_t queueNotEmpty;
    ec_zero(pthread_cond_init(&queueNotFull, NULL),"pthread_cond_init failed on condition queueNotFull");
    ec_zero(pthread_cond_init(&queueNotEmpty, NULL),"pthread_cond_init failed on condition queueNotEmpty");

    pthread_t *tSlaves;
    workerArgs *tSlavesArg;

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
    int masterExitReq = 0;

    //start signal masking
    ec_meno1(sigfillset(&set),errno);
    ec_meno1(pthread_sigmask(SIG_SETMASK,&set,NULL),errno); 

    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handle_sighup;
    ec_meno1(sigaction(SIGHUP,&sa,NULL),errno);
    sa.sa_handler=handle_sigint;
    ec_meno1(sigaction(SIGINT,&sa,NULL),errno);
    sa.sa_handler=handle_sigquit;
    ec_meno1(sigaction(SIGQUIT,&sa,NULL),errno);
    sa.sa_handler=handle_sigterm;
    ec_meno1(sigaction(SIGTERM,&sa,NULL),errno);
    sa.sa_handler=handle_sigusr1;
    ec_meno1(sigaction(SIGUSR1,&sa,NULL),errno);

    ec_meno1(sigemptyset(&set),errno);
    ec_meno1(pthread_sigmask(SIG_SETMASK,&set,NULL),errno);
    //END signal handling


    //START parsing 

    for(ac = 1; ac<argc; ac++) //0 is filename          should check if file list is 0 only when -d is present
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
                dirList = realloc(dirList,sizeDirList * sizeof(char*));
                ec_null(dirList,"realloc fallita, dirList non allocata");
                dirList[sizeDirList - 1] = calloc(strlen(argv[ac]) +1, sizeof(char));
                ec_null(dirList[sizeDirList - 1] ,"calloc fallita, elemento di dirList non allocato");
                strcpy(dirList[sizeDirList - 1], argv[ac]);

            }
            else
            {
                sizeFileList ++;
                fileList = realloc(fileList, sizeFileList * sizeof(char*));
                ec_null(fileList,"realloc fallita, fileList non allocata");
                fileList[sizeFileList - 1] = calloc(strlen(argv[ac]) +1, sizeof(char));
                ec_null(fileList[sizeFileList - 1] ,"calloc fallita, elemento di fileList non allocato");
                strcpy(fileList[sizeFileList - 1], argv[ac]);
            }
        }
    }

    //END parsing

    //START collector process
    char *collectorPath="./collector";
    char *collectorArgs[]={collectorPath,charnthread,NULL};
    int pid;
    pid= fork();
    ec_meno1(pid,errno);
    if(pid==0)  //figlio
    {
        execv(collectorPath,collectorArgs);
    }
    //END of collector process

    //START directory exploration

    for(i=0; i<sizeDirList; i++)
    {
        directoryDigger(dirList[i], fileList, &sizeFileList);
    }

    for(i=0; i<sizeDirList; i++)
    {
        free( dirList[i]);
    }
    free(dirList);
    
    //END of directory exploration

    //START of threading
    tSlaves = malloc(nthread * sizeof(pthread_t));
    ec_null(tSlaves,"malloc fallita, tSalves non allocato");
    tSlavesArg = malloc(nthread * sizeof(workerArgs));
    ec_null(tSlavesArg,"malloc fallita, tSalvesArg non allocato");
    for(i=0; i<nthread;i++)
    {
        tSlavesArg[i].queueHead = queueHead;
        tSlavesArg[i].queueSize = &queueSize;
        tSlavesArg[i].mtx = &mtx;
        tSlavesArg[i].queueNotEmpty = &queueNotEmpty;
        tSlavesArg[i].queueNotFull = &queueNotFull;
        tSlavesArg[i].exitReq = &masterExitReq;
        ec_zero(pthread_create(&(tSlaves[i]), NULL, &worker, &(tSlavesArg[i])),"ptread_create failure");  
    }
    

    //START producing

    for(i=0;i<sizeFileList;i++)
    {
        ec_zero(pthread_mutex_lock(&mtx),"pthread_mutex_lock failed with mtx");
        while(queueSize>=qlen)  //full queue
        {
            ec_zero(pthread_cond_wait(&queueNotFull,&mtx),"pthread_cond_wait failed on queueNotFull");
        }
        if(flagEndFetching)     //setted
        {
            break;
        }
        if(queueSize == 0)
        {
            queueHead = malloc(1*sizeof(node));
            ec_null(queueHead,"malloc of queueHead failed");
            queueHead->filename = fileList[i];     //shallow copy is enough
            queueSize = 1;
        }
        else
        {
            tmpPointer = queueHead->next;
            while(tmpPointer != NULL)
            {
                tmpPointer = tmpPointer->next;
            }
            tmpPointer = malloc(1*sizeof(node));
            tmpPointer->filename = fileList[i];     //shallow copy is enough
            queueSize++;
        }
        ec_zero(pthread_cond_signal(&queueNotEmpty),"pthread_cond_signal failed with queueNotEmpty");
        ec_zero(pthread_mutex_unlock(&mtx),"pthread_mutex_unlock failed with mtx");
    }
    if(flagEndFetching)
    {
        ec_zero(pthread_mutex_lock(&mtx),"pthread_mutex_lock failed with mtx, before setting masterExitReq");
        masterExitReq = 1;
        ec_zero(pthread_mutex_unlock(&mtx),"pthread_mutex_unlock failed with mtx, after setting masterExitReq");
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
    ec_zero(pthread_cond_broadcast(&queueNotEmpty),"pthread_cond_broadcast failed");  //to let every thread to finish its cleaning
    ec_zero(pthread_mutex_unlock(&mtx),"pthread_mutex_unlock failed with mtx, after checking flgSIGUSR1");

    for(i=0; i<nthread;i++)
    {
        ec_zero(pthread_join(tSlaves[i], NULL),"pthread_join failed");
    }
    for(i=0;i<sizeFileList;i++)
    {
        free(fileList[i]);
    }
    free(fileList);
    free(tSlaves);
    free(tSlavesArg);
}
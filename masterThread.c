//TODO: check for error when accessing the array
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "workerThread.h"

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

void directoryDigger(char* dir, char** fileList, int* fileListSize)     //recursive approach
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
                fileList[(* fileListSize) - 1] = calloc(strlen(dir) +1, sizeof(char));
                strcpy(fileList[(* fileListSize) - 1], dir); 
            }       
            else        //another type of error
            {
                
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
            if(errno == 0)
            {
                closedir(oDir);
            }
            
        }
    } 
    else 
    {
    //TODO: file doesn't exist
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
    pthread_cond_init(&queueNotFull, NULL);
    pthread_cond_init(&queueNotEmpty, NULL);

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
    sigfillset(&set);
    pthread_sgmask(SIG_SETMASK,&set,NULL);

    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handle_sighup;
    sigaction(SIGHUP,&sa,NULL);
    sa.sa_handler=handle_sigint;
    sigaction(SIGINT,&sa,NULL);
    sa.sa_handler=handle_sigquit;
    sigaction(SIGQUIT,&sa,NULL);
    sa.sa_handler=handle_sigterm;
    sigaction(SIGTERM,&sa,NULL);
    sa.sa_handler=handle_sigusr1;
    sigaction(SIGUSR1,&sa,NULL);

    sigprocmask(SIG_UNBLOCK, &mask,NULL)
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
                dirList[sizeDirList - 1] = calloc(strlen(argv[ac]) +1, sizeof(char));
                strcpy(dirList[sizeDirList - 1], argv[ac]);

            }
            else
            {
                sizeFileList ++;
                fileList = realloc(fileList, sizeFileList * sizeof(char*));
                fileList[sizeFileList - 1] = calloc(strlen(argv[ac]) +1, sizeof(char));
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
    if(pid == -1)   //padre, errore
    {
        //gestione errore
    }
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
    tSlavesArg = malloc(nthread * sizeof(workerArgs));
    for(i=0; i<nthread;i++)
    {
        tSlavesArg[i].queueHead = queueHead;
        tSlavesArg[i].queueSize = &queueSize;
        tSlavesArg[i].mtx = &mtx;
        tSlavesArg[i].queueNotEmpty = &queueNotEmpty;
        tSlavesArg[i].queueNotFull = &queueNotFull;
        tSlavesArg[i].exitReq = &masterExitReq;
        pthread_create(&(tSlaves[i]), NULL, &worker, &(tSlavesArg[i]));  
    }
    

    //START producing

    for(i=0;i<sizeFileList;i++)
    {
        pthread_mutex_lock(&mtx);
        while(queueSize>=qlen)  //full queue
        {
            pthread_cond_wait(&queueNotFull,&mtx);
        }
        if(flagEndFetching)     //setted
        {
            break;
        }
        if(queueSize == 0)
        {
            queueHead = malloc(1*sizeof(node));
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
        pthread_cond_signal(&queueNotEmpty);
        pthread_mutex_unlock(&mtx);
    }
    if(flagEndFetching)
    {
        pthread_mutex_lock(&mtx);
        masterExitReq = 1;
        pthread_mutex_unlock(&mtx);
    }
    //TODO:wait to end coda before joining childs, could do this by giving child a cond_signal to

    pthread_mutex_lock(&mtx);
    if(flagSIGUSR1)
    {
        masterExitReq = 2;
        kill(pid,SIGUSR2)
    }
    else{
        masterExitReq = 1;
    }
    pthread_cond_broadcast(&queueNotEmpty);  //to let every thread to finish its cleaning
    pthread_mutex_unlock(&mtx);

    for(i=0; i<nthread;i++)
    {
        pthread_join(tSlaves[i], NULL);
    }
    for(i=0;i<sizeFileList;i++)
    {
        free(fileList[i]);
    }
    free(fileList);
    free(tSlaves);
    free(tSlavesArg);
}
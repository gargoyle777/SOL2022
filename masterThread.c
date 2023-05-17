#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "workerThread.h"
#include "senderThread.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include "common.h"

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

static int insertElementQueue(char* target, int queueUpperLimit)
{
    //printf("master si occupd di %s\n",target);
    errno=0;
    ec_zero(pthread_mutex_lock(&(producermtx)),"pthread_mutex_lock failed with producermtx");
    while(pqSize>=queueUpperLimit)  //full queue
    {
        errno=0;
        ec_zero(pthread_cond_wait(&(pqFull),&producermtx),"pthread_cond_wait failed on pqFull");
    }
    if(flagEndFetching == 1)     //setted
    {
        return 0; 
    }

    if(pqSize == 0)
    {
        //printf("master inizia a mettere un elemento in testa\n");
        errno=0;
        queueHead = malloc(sizeof(pqElement));
        ec_null(queueHead,"malloc of queueHead failed");
        queueHead->next = NULL;
        queueHead->filename = malloc(strnlen(target,MAX_PATH_LENGTH)+1);
        memset(queueHead->filename,0,strnlen(target,MAX_PATH_LENGTH)+1);
        strncpy(queueHead->filename, target, strnlen(target,MAX_PATH_LENGTH)+1);  
        pqSize = 1;
        //printf("master ha messo un elemento in testa, %s\n",queueHead->filename);
    }
    else
    {
        pqElement *tmpPointer = queueHead;
        while(tmpPointer->next != NULL)
        {
            tmpPointer = tmpPointer->next;
        }
        errno=0;
        tmpPointer->next = malloc(sizeof(pqElement));
        ec_null(tmpPointer->next,"malloc of an element of the queue failed");
        tmpPointer = tmpPointer->next;
        tmpPointer->next = NULL;
        tmpPointer->filename = malloc( strnlen(target,MAX_PATH_LENGTH)+1);
        memset(tmpPointer->filename,0, strnlen(target,MAX_PATH_LENGTH)+1);
        strncpy(tmpPointer->filename, target,  strnlen(target,MAX_PATH_LENGTH)+1);  
        pqSize++;
    }
    errno=0;
    ec_zero(pthread_cond_signal(&pqEmpty),"pthread_cond_signal failed with pqEmpty");
    //printf("master ha segnalato su queue not empty\n");
    errno=0;
    ec_zero(pthread_mutex_unlock(&producermtx),"pthread_mutex_unlock failed with producermtx");
    //printf("master ha lasciato il lock\n");
    return 1;
}

static int startCollectorProcess()
{
    char *collectorPath="./collector";
    char *collectorArgs[]={collectorPath,NULL};
    int pid;
    pid= fork();
    errno=0;
    ec_meno1(pid,(strerror(errno)));
    if(pid==0)  //figlio
    {
        execv(collectorPath,collectorArgs);
    }
    return pid;
}

static void addFileToList(char*** fileList, char* target, int* sizeFileList )
{
    //printf("master vuole aggiungere %s\n",target);
    checked_realloc( (void**) fileList, (*sizeFileList) + 1, sizeof(char*));
    errno=0;
    (*fileList)[*sizeFileList] = malloc(strnlen(target,MAX_PATH_LENGTH)+1);
    ec_null((*fileList)[*sizeFileList],"malloc fallita, stringa di elemento di fileList non allocato");
    strncpy( (*fileList)[*sizeFileList], target, strnlen(target, MAX_PATH_LENGTH) +1 );
    //printf("master ha digerito: %s \n",(*fileList)[*sizeFileList]); //testing
    (*sizeFileList) ++;
}

static int checkFile(char* target)
{
    struct stat fileInfo;
    int result = 0;
    errno=0;
    int statRet= stat(target, &fileInfo);
    ec_meno1(statRet,strerror(errno));
    if (statRet == 0) 
    {
        if (S_ISREG(fileInfo.st_mode)) 
        {
            result= 1; // esiste, regular
        } 
        else 
        {
            //printf("%s non e' regular",target);
        }
    } 
    else 
    {
        //printf("%s non esiste",target);
    }
    return result;

}

static void checkAndAdd(char*** fileList, char* target, int* sizeFileList)
{
    if(checkFile(target))
    {
        addFileToList(fileList, target, sizeFileList);     
    }
    else
    {
        //nothing to do
    }
}

static void directoryDigger(char* path, char*** fileList, int* sizeFileList)
{
    DIR* directory;
    struct dirent* entry;
    errno = 0;
    char newPath[MAX_PATH_LENGTH];
    errno=0;
    ec_null(directory = opendir(path), strerror(errno));

    while ((entry=readdir(directory)) != NULL) 
    {
        if(entry->d_type == DT_DIR)
        {
            if(entry->d_name[0]=='.')   continue;
            memset(newPath,0,MAX_PATH_LENGTH);
            strncpy(newPath,path,strnlen(path,MAX_PATH_LENGTH+1));
            newPath[strnlen(path,MAX_PATH_LENGTH)] = '/';
            strncpy(&(newPath[strnlen(path,MAX_PATH_LENGTH) + 1]),entry->d_name,(strnlen(path,MAX_PATH_LENGTH)+1+strnlen(entry->d_name,MAX_PATH_LENGTH)+1));
            //printf("digging deeper: %s\n",newPath);
            directoryDigger(newPath, fileList, sizeFileList);
        }
        else
        {
            memset(newPath,0,MAX_PATH_LENGTH);
            strncpy(newPath,path,strnlen(path,MAX_PATH_LENGTH+1));
            newPath[strnlen(path,MAX_PATH_LENGTH)] = '/';   //newpath as a tmp value here
            strncpy(&(newPath[strnlen(path,MAX_PATH_LENGTH) + 1]),entry->d_name,(strnlen(path,MAX_PATH_LENGTH)+1+strnlen(entry->d_name,MAX_PATH_LENGTH)+1));
            //printf("digging deeper: %s\n",newPath);
            checkAndAdd(fileList, newPath, sizeFileList);
        }
    }
    errno=0;
    ec_meno1(closedir(directory),strerror(errno));
}

static void signalHandling()
{
    //for masking
    sigset_t set;
    struct sigaction sa;

    errno=0;
    ec_meno1(sigfillset(&set),(strerror(errno)));
    errno=0;
    ec_meno1(pthread_sigmask(SIG_SETMASK,&set,NULL),(strerror(errno))); 

    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handle_sighup;
    errno=0;
    ec_meno1(sigaction(SIGHUP,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigint;
    errno=0;
    ec_meno1(sigaction(SIGINT,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigquit;
    errno=0;
    ec_meno1(sigaction(SIGQUIT,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigterm;
    errno=0;
    ec_meno1(sigaction(SIGTERM,&sa,NULL),(strerror(errno)));
    sa.sa_handler=handle_sigusr1;
    errno=0;
    ec_meno1(sigaction(SIGUSR1,&sa,NULL),(strerror(errno)));
    errno=0;
    ec_meno1(sigemptyset(&set),(strerror(errno)));
    errno=0;
    ec_meno1(sigaddset(&set,SIGPIPE),"failed to sigaddset");
    errno=0;
    ec_meno1(pthread_sigmask(SIG_SETMASK,&set,NULL),(strerror(errno)));


}

int main(int argc, char* argv[])
{
    pthread_t senderThread;     // sender thread
    pthread_t *tSlaves;         //array of worker threads
    int i;                      //counter
    int opt;                    //holds the option returned from getopt
    char **fileList;            //lists of file names
    int sizeFileList = 0;       //dim of fileList
    int nthread = 4;            //number of worker threads, default is 4
    int qlen = 8;               //size of queue between master and workers, default is 8
    int dirFlag = 0;            //flag signaling -d option
    int delay = 0;              //delay value, default is 0
    char* inputtedDirectory;    //default is non present
    int pid;                    //pid of collector process
    char* tmpTarget;            //tmp value holding string
    pqElement* tmpqh;           //tmp value holding pqhelement
    sqElement* tmpsh;           //tmp value holding sqelement
    int senderSocket=0;         //sender socket, used to secure the closing of the socket
    int insertRetVal = 0;       //return value of insert function
    int collectorRetVal=0;      //return value from collector

    //printf("sto iniziando il main\n");//testing
    signalHandling();

	//printf("sto per parsare\n");//testing

    //new parser
    while(( opt = getopt(argc, argv, "n:q:t:d:") ) !=-1)
    {
        switch(opt) 
        {
            case 'n':   //number of thread
                nthread = atoi(optarg);
                break;
            case 'q':   //concurrent queue's length
                qlen = atoi(optarg);
                break;
            case 'd':
                dirFlag = 1;  
                //printf("directory individuata: %s\n",optarg);
                inputtedDirectory = malloc(strnlen(optarg,MAX_PATH_LENGTH)+1);
                strncpy(inputtedDirectory,optarg,strnlen(optarg,MAX_PATH_LENGTH)+1);
                break;
            case 't': //delay
                delay = atoi(optarg);
                break; 
            case '?': //invalid option
                //printf("invalid option\n");
                break;
        }
    }

    //file names
    while (optind < argc)
    {
        tmpTarget = argv[optind];
        checkAndAdd(&fileList,tmpTarget,&sizeFileList);
        optind++;          
    }

    //START directory exploration   
    if(dirFlag == 1) 
    {
        //printf("sto per iniziare a scavare\n");
        directoryDigger(inputtedDirectory,&fileList,&sizeFileList);
    } 
    //END of directory exploration

    //printf("numero file per master: %d\n",sizeFileList);

    //mtx initializations
    ec_zero(pthread_mutex_init(&producermtx,NULL),"pthread_mutex_init failed");

    ec_zero(pthread_mutex_init(&sendermtx,NULL),"pthread_mutex_init failed");

    ec_zero(pthread_mutex_init(&requestmtx,NULL),"pthread_mutex_init failed");

    ec_zero(pthread_cond_init(&pqFull, NULL),"pthread_cond_init failed");

    ec_zero(pthread_cond_init(&pqEmpty, NULL),"pthread_cond_init failed");

    ec_zero(pthread_cond_init(&sqEmpty,NULL),"pthread_cond_init failed");

    //START collector process
    pid = startCollectorProcess();
    //END of collector process

    //START of threading
    errno=0;
    tSlaves = malloc(nthread * sizeof(pthread_t));
    ec_null(tSlaves,"malloc fallita, tSalves non allocati");
    for(i=0; i<nthread;i++)
    {
        ec_zero(pthread_create(&(tSlaves[i]), NULL, &producerWorker, NULL), "ptread_create failure");  
    }

    ec_zero(pthread_create(&senderThread, NULL, &senderWorker, NULL), "pthread_create failure");
    //END of threading

    //START producing
	//printf("master inizia a produrre,%d elementi in lista\n",sizeFileList);
    for(i=0;i<sizeFileList;i++)
    {
        //printf("master si occupa di %s\n",fileList[i]);
        insertRetVal=insertElementQueue(fileList[i],qlen);
        if(insertRetVal == 0)
        {
            //time to stop
            i = sizeFileList;
            delay = 0;
        }
        usleep(delay);
    }
   //END producing
    ec_zero(pthread_mutex_lock(&requestmtx),"pthread_mutex_lock failed with producermtx, before checking flagSIGUSR1");

    if(flagSIGUSR1 == 1)
    {
        masterExitReq = 2;
        kill(pid,SIGUSR2);
    }
    else{
        masterExitReq = 1;
    }
    
    ec_zero(pthread_mutex_unlock(&(requestmtx)),"pthread_mutex_unlock failed with producermtx, after checking flgSIGUSR1");

    ec_zero(pthread_mutex_lock(&producermtx),"pthread_mutex_lock failed with producermtx, before checking flagSIGUSR1");
    ec_zero(pthread_cond_broadcast(&(pqEmpty)),"pthread_cond_broadcast failed");  //to let every thread to finish its cleaning
    ec_zero(pthread_mutex_unlock(&(producermtx)),"pthread_mutex_unlock failed with producermtx, after checking flgSIGUSR1");

    for(i=0; i<nthread;i++)
    {
    	//printf("master sta iniziando a fare il join del thread numero %d\n",i);
        ec_zero(pthread_join(tSlaves[i], NULL),"pthread_join failed");
    }

    ec_zero(pthread_mutex_lock(&sendermtx),"pthread_mutex_lock failed with sendermtx, before checking flagSIGUSR1");
    ec_zero(pthread_cond_signal(&sqEmpty),"sendermtx signal\n");
    ec_zero(pthread_mutex_unlock(&sendermtx),"pthread_mutex_lock failed with sendermtx, before checking flagSIGUSR1");
    //printf("master inizia il join di sender\n");
    senderSocket=pthread_join(senderThread, NULL);
    ec_zero(senderSocket,"pthread_join failed");
    

    //printf("master ha finito di fare i join\n");

    //START CLEANUP
    //cleaning own strings
    for(i=0;i<sizeFileList;i++)
    {
        free(fileList[i]);
    }
    free(fileList);
    free(tSlaves);

    //master check for unfreed lists
    while(queueHead != NULL)
    {
        tmpqh=queueHead;
        queueHead= queueHead->next;
        free(tmpqh->filename);
        free(tmpqh);
    }

    while(sqHead != NULL)
    {
        tmpsh=sqHead;
        sqHead= sqHead->next;
        free(tmpsh->filename);
        free(tmpsh);
    }

    //printf("master manda il segnale di fermarsi a collector\n");
    kill(pid,SIGUSR2);

    //free eventuale directory
    if(dirFlag == 1) free(inputtedDirectory);
    
    waitpid(pid,&collectorRetVal,0);
    //printf("master dice che collector returned with %d\n",WEXITSTATUS(checkk));
    errno=0;
    ec_zero(unlink(SOCKNAME),strerror(errno)); //clean the socket file 
    errno=0;
    ec_meno1(close(senderSocket),"failed to close socket from master");
    //printf("master ha aspettato il collector\n---master chiude---\n");
    return 0;
}
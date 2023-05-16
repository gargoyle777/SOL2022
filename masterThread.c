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
#include "senderThread.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include "common.h"
#include <fnmatch.h>
#include <fnmatch.h>

#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); exit(EXIT_FAILURE); }    
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

void checked_realloc(char ***ptr, int length, size_t size)
{
    errno=0;
    if(length==1) 
    {
        //printf("provo malloc \n");
        *ptr=malloc(length*size);
    }
    else 
    {
        //printf("provo realloc \n");
        *ptr=realloc(*ptr, length*size);
    }
    ec_null(*ptr,"checked_realloc fallita");
    //printf("riuscita\n");
}

static void insertElementQueue(char* target, int queueUpperLimit)
{
    //printf("master si occupd di %s\n",target);
    ec_zero(pthread_mutex_lock(&(mtx)),"pthread_mutex_lock failed with mtx");
    while(queueSize>=queueUpperLimit)  //full queue
    {
        ec_zero(pthread_cond_wait(&(queueFull),&mtx),"pthread_cond_wait failed on queueFull");
    }
    if(flagEndFetching)     //setted
    {
        return; //TODO: CHECK this branch
    }

    if(queueSize == 0)
    {
        //printf("master inizia a mettere un elemento in testa\n");
        queueHead = malloc(sizeof(qElem));
        ec_null(queueHead,"malloc of queueHead failed");
        queueHead->next = NULL;
        queueHead->filename = malloc(strnlen(target,MAX_PATH_LENGTH)+1);
        memset(queueHead->filename,0,strnlen(target,MAX_PATH_LENGTH)+1);
        strncpy(queueHead->filename, target, strnlen(target,MAX_PATH_LENGTH)+1);  
        queueSize = 1;
        //printf("master ha messo un elemento in testa, %s\n",queueHead->filename);
    }
    else
    {
        qElem *tmpPointer = queueHead;
        while(tmpPointer->next != NULL)
        {
            tmpPointer = tmpPointer->next;
        }
        tmpPointer->next = malloc(sizeof(qElem));
        ec_null(tmpPointer->next,"malloc of an element of the queue failed");
        tmpPointer = tmpPointer->next;
        tmpPointer->next = NULL;
        tmpPointer->filename = malloc( strnlen(target,MAX_PATH_LENGTH)+1);
        memset(tmpPointer->filename,0, strnlen(target,MAX_PATH_LENGTH)+1);
        strncpy(tmpPointer->filename, target,  strnlen(target,MAX_PATH_LENGTH)+1);  
        queueSize++;
    }

    ec_zero(pthread_cond_signal(&queueEmpty),"pthread_cond_signal failed with queueEmpty");
    //printf("master ha segnalato su queue not empty\n");
    ec_zero(pthread_mutex_unlock(&mtx),"pthread_mutex_unlock failed with mtx");
    //printf("master ha lasciato il lock\n");
}

static int startCollectorProcess()
{
    char *collectorPath="./collector";
    char *collectorArgs[]={collectorPath,NULL};
    int pid;
    pid= fork();
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
    checked_realloc(fileList, (*sizeFileList) + 1, sizeof(char*));
    (*fileList)[*sizeFileList] = malloc(strnlen(target,MAX_PATH_LENGTH)+1);
    ec_null((*fileList)[*sizeFileList],"malloc fallita, stringa di elemento di fileList non allocato");
    strncpy( (*fileList)[*sizeFileList], target, strnlen(target, MAX_PATH_LENGTH) +1 );
    //printf("master ha digerito: %s \n",(*fileList)[*sizeFileList]); //testing
    (*sizeFileList) ++;
}
/*
static int containsWildcard(char* target)
{
    size_t len = strnlen(target,UNIX_PATH_MAX);
    size_t i=0;
    for( i=0;i<len;i++)
    {
        if(target[i] == '*' || target[i]=='?') return 1;
    }
    return 0;
}
*/
static int checkFile(char* target)
{
    struct stat fileInfo;
    int result = 0;
    if (stat(target, &fileInfo) == 0) 
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

void directoryDigger(char* path, char*** fileList, int* sizeFileList)
{
    DIR* directory;
    struct dirent* entry;
    errno = 0;
    char newPath[MAX_PATH_LENGTH];
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
/*
static void addIfMatching(char*** fileList,char* tmpTarget,int* sizeFileList)
{
    DIR* directory = opendir(".");
    errno=0;
    ec_null(directory,strerror(errno));
    struct dirent* entry;

    while ((entry = readdir(directory)) != NULL) 
    {
        if (fnmatch(tmpTarget, entry->d_name, 0) == 0) 
        {
            checkAndAdd(fileList, entry->d_name, sizeFileList);
        }
    }

    closedir(directory);
}
*/
int main(int argc, char* argv[])
{
	//printf("sto iniziando il main\n");//testing

    //ec_null(getcwd(baseDir,256),"couldn't retrieve current working directory");
    //for masking
    sigset_t set;
    struct sigaction sa;

    queueSize = 0;
    pthread_mutex_init(&mtx,NULL);
    ec_zero(pthread_cond_init(&queueFull, NULL),"pthread_cond_init failed on condition queueFull");
    ec_zero(pthread_cond_init(&queueEmpty, NULL),"pthread_cond_init failed on condition queueEmpty");

    pthread_t senderThread;
    pthread_t *tSlaves;
    
    int i;  //counter
    int opt;
    char **fileList;
    int sizeFileList = 0;
    int nthread = 4;       //default is 4
    int qlen = 8;       //default is 8
    int dirFlag = 0;        //check for directory
    int delay = 0;        //default is 0
    char* inputtedDirectory; //default is non present
    int pid;
    char* tmpTarget;
    qElem* tmpqh;
    sqElement* tmpsh;
    int senderSocket=0;

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

	//printf("sto per parsare\n");//testing

    //new parse
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

    while (optind < argc) {
        tmpTarget = argv[optind];
        checkAndAdd(&fileList,tmpTarget,&sizeFileList);

        /*  WILDCARDS ARE HANDLED BY GLOB
        if((containsWildcard(tmpTarget)) == 0)
        { //no wildcard
            checkAndAdd(&fileList,tmpTarget,&sizeFileList);
        }
        else //wildcards are present
        {
            addIfMatching(&fileList,tmpTarget,&sizeFileList);

        }
        optind++;  
        */
    }

    //START directory exploration

    
    if(dirFlag == 1) 
    {
        //printf("sto per iniziare a scavare\n");
        directoryDigger(inputtedDirectory,&fileList,&sizeFileList);
    }
    
    //END of directory exploration

    //END parsing

    //START collector process
    pid = startCollectorProcess();
    //END of collector process

    //START of threading
    tSlaves = malloc(nthread * sizeof(pthread_t));
    ec_null(tSlaves,"malloc fallita, tSalves non allocati");
    for(i=0; i<nthread;i++)
    {
        ec_zero(pthread_create(&(tSlaves[i]), NULL, &producerWorker, NULL), "ptread_create failure");  
    }

    ec_zero(pthread_create(&senderThread, NULL, &senderWorker, NULL), "pthread_create failure");
    
    //START producing
	//printf("master inizia a produrre,%d elementi in lista\n",sizeFileList);
    for(i=0;i<sizeFileList;i++)
    {
        //printf("master si occupa di %s\n",fileList[i]);
        insertElementQueue(fileList[i],qlen);
        usleep(delay);
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
    ec_zero(pthread_cond_broadcast(&(queueEmpty)),"pthread_cond_broadcast failed");  //to let every thread to finish its cleaning
    ec_zero(pthread_mutex_unlock(&(mtx)),"pthread_mutex_unlock failed with mtx, after checking flgSIGUSR1");

    for(i=0; i<nthread;i++)
    {
    	//printf("master sta iniziando a fare il join del thread numero %d\n",i);
        ec_zero(pthread_join(tSlaves[i], NULL),"pthread_join failed");
    }

    //printf("master inizia il join di sender\n");
    senderSocket=pthread_join(senderThread, NULL);
    ec_zero(senderSocket,"pthread_join failed");

    //printf("master ha finito di fare i join\n");
    for(i=0;i<sizeFileList;i++)
    {
        free(fileList[i]);
    }
    free(fileList);
    free(tSlaves);

    //master check for unfreed stacks
    if(queueHead !=NULL)
    {
        while(queueHead != NULL)
        {
            tmpqh=queueHead;
            queueHead= queueHead->next;
            free(tmpqh->filename);
            free(tmpqh);
        }
    }

    if(sqHead != NULL)
    {
        while(sqHead != NULL)
        {
            tmpsh=sqHead;
            sqHead= sqHead->next;
            free(tmpsh->filename);
            free(tmpsh);
        }
    }

    //printf("master manda il segnale di fermarsi a collector\n");
    kill(pid,SIGUSR2);
    int checkk=0;

    //clean dei vecchi valori
    
    if(dirFlag == 1) free(inputtedDirectory);
    
    waitpid(pid,&checkk,0);
    //printf("master dice che collector returned with %d\n",WEXITSTATUS(checkk));
    errno=0;
    ec_zero(unlink(SOCKNAME),strerror(errno)); //clean the socket file 
    ec_meno1(close(senderSocket),strerror(errno));
    //printf("master ha aspettato il collector\n---master chiude---\n");
    return 0;
}

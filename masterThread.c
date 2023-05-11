//TODO: check for error when accessing the array

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "workerThread.h"
#include "senderThread.h"
#include <sys/wait.h>
#include "common.h"

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

void checked_realloc(char ***ptr, int length, size_t size)
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

static void insertElementQueue(char* target, int queueUpperLimit)
{
    printf("master si occupd di %s\n",target);
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
        printf("master inizia a mettere un elemento in testa\n");
        queueHead = malloc(sizeof(qElem));
        ec_null(queueHead,"malloc of queueHead failed");
        queueHead->next = NULL;
        queueHead->filename = malloc(strnlen(target,MAX_PATH_LENGTH)+1);
        memset(queueHead->filename,0,strnlen(target,MAX_PATH_LENGTH)+1);
        strncpy(queueHead->filename, target, strnlen(target,MAX_PATH_LENGTH)+1);  
        queueSize = 1;
        printf("master ha messo un elemento in testa, %s\n",queueHead->filename);
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
    printf("master ha segnalato su queue not empty\n");
    ec_zero(pthread_mutex_unlock(&mtx),"pthread_mutex_unlock failed with mtx");
    printf("master ha lasciato il lock\n");
}

//DOESNT SAVE THE DIGGING, JUST SAVING DIRECTORY BY THE NAME WITHOUT PATH
void directoryDigger(char* path, char*** fileList, int* fileListSize)     //recursive approach TODO: gestione errore troppo particolareggiata
{
    DIR* openedDir;
    struct dirent* freshDir;
    char tmpString[256];

    printf("scavo in una directory\n"); //testing

    errno = 0;
    ec_null(openedDir = opendir(path),path);

    while ((freshDir=readdir(openedDir)) != NULL) //TODO SUCCEDE BORDELLO CON IL PATH
    {
        memset(tmpString,0,256);
        memcpy(tmpString,path,strlen(path));
        memcpy(&(tmpString[strlen(path)]),freshDir->d_name,strlen(freshDir->d_name));
        printf("ecco il path <%s>\n", tmpString);

        if(freshDir->d_type == DT_DIR)  //it's a directory
        {
            tmpString[strlen(tmpString)] = '/';
            directoryDigger(tmpString,fileList,fileListSize);            
        }
        else if(freshDir->d_type == DT_REG)
        {
            (* fileListSize) ++;
            checked_realloc( fileList,(* fileListSize), sizeof(char*));
            *fileList[(* fileListSize) - 1] = malloc(strlen(tmpString));
            ec_null(*fileList[(* fileListSize) - 1],"malloc fallita, elemento di fileList non allocato");
            strcpy(*fileList[(* fileListSize) - 1], tmpString); 
            printf("ho collezionato %s\n",*fileList[(* fileListSize) - 1]); //testing
        }
        errno=0;
    }
    if(errno!=0)    ec_null(freshDir,"something went wrong with readdir()");
    ec_meno1(closedir(openedDir),"failure on close dir");
}

int main(int argc, char* argv[])
{
	printf("sto iniziando il main\n");//testing

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
    char **dirList;
    int sizeDirList = 0;
    int ac; //counts how many argument i have checked
    int nthread = 4;       //default is 4
    int qlen = 8;       //default is 8
    int dirFlag = 0;        //check for directory
    int delay = 0;        //default is 0
    char* inputtedDirectory; //default is non present

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

    /*new parse
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
            case 't':
                dirFlag = 1;  
                inputtedDirectory = malloc(strnlen(optarg,MAX_PATH_LENGTH)+1);
                strncpy(inputtedDirectory,optarg,MAX_PATH_LENGTH);
                break;
            case 'd': //delay
                delay = atoi(optarg);
                break; 
            case '?': //invalid option
        }
    }

    while (optind < argc) {
        printf("Argument: %s\n", argv[optind]);
        optind++;
    }
*/
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
                checked_realloc(&dirList,sizeDirList, sizeof(char*));
                dirList[sizeDirList - 1] = calloc(strlen(argv[ac]) +1, sizeof(char));
                ec_null(dirList[sizeDirList - 1] ,"calloc fallita, elemento di dirList non allocato");

                strcpy(dirList[sizeDirList - 1], argv[ac]);

            }
            else
            {
            	printf("file trovato nell'argomento %d\n",ac); //testing
                sizeFileList ++;

                checked_realloc(&fileList, sizeFileList, sizeof(char*));
                fileList[sizeFileList - 1] = malloc(strlen(argv[ac]) +1);
                ec_null(fileList[sizeFileList - 1] ,"malloc fallita, elemento di fileList non allocato");

                strcpy(fileList[sizeFileList-1],argv[ac]);
                printf("master ha digerito: %s \n",fileList[sizeFileList - 1]); //testing
            }
        }
    }

    //END parsing

    //START collector process
    char *collectorPath="./collector";
    char *collectorArgs[]={collectorPath,NULL};
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
    
    //END of directory exploration

    //START of threading
    tSlaves = malloc(nthread * sizeof(pthread_t));
    ec_null(tSlaves,"malloc fallita, tSalves non allocati");
    for(i=0; i<nthread;i++)
    {
        ec_zero(pthread_create(&(tSlaves[i]), NULL, &producerWorker, NULL), "ptread_create failure");  
    }

    ec_zero(pthread_create(&senderThread, NULL, &senderWorker, NULL), "pthread_create failure");
    
    //START producing
	printf("master inizia a produrre,%d elementi in lista\n",sizeFileList);
    for(i=0;i<sizeFileList;i++)
    {
        printf("master si occupa di %s\n",fileList[i]);
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
    	printf("master sta iniziando a fare il join del thread numero %d\n",i);
        ec_zero(pthread_join(tSlaves[i], NULL),"pthread_join failed");
    }

    printf("master inizia il join di sender\n");
    ec_zero(pthread_join(senderThread, NULL),"pthread_join failed");

    printf("master ha finito di fare i join\n");
    for(i=0;i<sizeFileList;i++)
    {
        free(fileList[i]);
    }
    free(fileList);
    free(tSlaves);

    printf("master manda il segnale di fermarsi a collector\n");
    kill(pid,SIGUSR2);
    int checkk=0;

    //clean dei vecchi valori
    for(i=0; i<sizeDirList; i++)
    {
        printf("faccio il free di %s\n",dirList[i]);
        free(dirList[i]);
    }
    
    if(sizeDirList>0){
        printf("faccio il free di dir list\n");
        free(dirList);
    }   
    
    if(dirFlag == 1) free (inputtedDirectory);
    
    waitpid(pid,&checkk,0);
    printf("master dice che collector returned with %d\n",WEXITSTATUS(checkk));
    printf("master ha aspettato il collector\n---master chiude---\n");
    return 0;
}

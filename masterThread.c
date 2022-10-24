//TODO: check for error when accessing the array
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

typedef struct queueEl{
    char* filename;
    struct queueEl * next;
} node;

typedef struct arguments 
{
    node* queueHead;
    int* queueSize;
    pthread_mutex_t * mtx;
    pthread_cond_t *queueNotFull;
    pthread_cond_t *queueNotEmpty;
    int exitReq;
}   workerArgs;

void directoryDigger(char* dir, char** fileList, int* fileListSize)     //recursive approach
{

    DIR* oDir;
    struct dirent* rDir;
    if (access(dir, F_OK) == 0) //file exist
    {
        errno = 0;
        oDir = openDir(dir);
        if( oDir == NULL)     //file exist but its not a directory
        {   
            if(errno == ENOTDIR)        //not a directory
            {
            (* fileListSize) ++;
                fileList = realloc((* fileListSize) * sizeof(char*));
                fileList[(* fileListSize) - 1] = malloc(strlen(dir) * sizeof(char));
                strcpy(fileList[(* fileListSize) - 1], dir); 
            }       
            else        //another type of error
            {
                
            }  
        }
        else        //file exist and its a directory or different type of error
        {
            errno = 0;
            while( (NULL!= (rDir = readdir(dir))) )
            {
                directoryDigger(rDir->d_name, fileList, fileListSize);
                //Check errno
            }
            if(errno == 0)
            {
                closedir(rDir);
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
    int qlen = 8;       //default is 8
    int dirFlag = 0;        //check for directory
    int delay = 0;        //default is 0

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
                dirList = realloc(sizeDirList * sizeof(char*));
                dirList[sizeDirList - 1] = malloc(strlen(argv[ac]) * sizeof(char));
                strcpy(dirList[sizeDirList - 1], argv[ac]);

            }
            else
            {
                sizeFileList ++;
                fileList = realloc(sizeFileList * sizeof(char*));
                fileList[sizeFileList - 1] = malloc(strlen(argv[ac]) * sizeof(char));
                strcpy(fileList[sizeFileList - 1], argv[ac]);
                
            }
        }
    }

    //END parsing

    //START directory exploration

    for(i=0; i<sizeDirList; i++)
    {
        directoryDigger(dirList[i], fileList, &sizeFileList);
    }
    
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
        tSlavesArg[i].exitReq = 0;
        pthread_create(&(tSlaves[i]), NULL, &worker, &(tSlavesArg[i]));  
    }
    

    //START producing

    for(i=0;i<sizeFileList;i++)
    {
        Pthread_mutex_lock(&mtx);
        while(queueSize>=qlen)
        {
            wait(queueNotFull,mtx);
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
        Pthread_cond_signal(&queueNotEmpty);
        Pthread_mutex_unlock(&mtx);
    }
}
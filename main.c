//TODO: check for error when accessing the array
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

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

    for(int c=0; c<sizeDirList; c++)
    {
        directoryDigger(dirList[c], fileList, &sizeFileList);
    }
    
    //END of directory exploration

    
}
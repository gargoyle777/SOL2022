#ifndef COMMON_H_
#define COMMON_H_
#include <pthread.h>
#include <stdio.h>

//defines
#define UNIX_PATH_MAX 108
#define MAX_PATH_LENGTH 256
#define SOCKNAME "./farm.sck"

//macro to check errors
#define ec_meno1(s,m) \
    if((s) == -1) { perror(m); exit(EXIT_FAILURE);}    

#define ec_null(s,m) \
    if((s) == NULL) { perror(m); exit(EXIT_FAILURE); }

#define ec_zero(s,m) \
    if((s) != 0) { perror(m); exit(EXIT_FAILURE); }

//structure for linked list between master and workers
typedef struct produceQueueElement
{
    char *filename;
    struct produceQueueElement *next;
} pqElement;

//structure for linked list between workers and sender
typedef struct sendQueueElement
{
    char *filename;
    long val;
    struct sendQueueElement *next;
} sqElement;

//extern variables shared between threads

//producers shared variables
extern pqElement *queueHead;
extern int pqSize;
extern pthread_mutex_t producermtx;
extern pthread_cond_t pqFull;
extern pthread_cond_t pqEmpty;

//used for inter thread communication
extern pthread_mutex_t requestmtx;
extern int masterExitReq;

//sender shared variables
extern sqElement *sqHead;
extern int sqSize;
extern pthread_mutex_t sendermtx;
extern pthread_cond_t sqEmpty;

//function prototypes
void senderlock_cleanup_handler(void* arg)
void producerlock_cleanup_handler(void* arg)
void requestlock_cleanup_handler(void* arg)
void workstruct_cleanup_handler(void* arg)
void file_cleanup_handler(void *arg)
void senderstruct_cleanup_handler(void* arg)
void socket_cleanup_handler(void* arg)
void checked_realloc(void ***ptr, int length, size_t size)

#endif
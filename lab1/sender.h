#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <mqueue.h>

typedef struct {
    int flag;      // 1 for message passing, 2 for shared memory
    union{
        mqd_t mq;
        char* shm_addr;
    }storage;
} mailbox_t;


typedef struct {
    long mtype;          // Message type, needed for message queues
    char mtext[1024];    // Message content (up to 1024 bytes)
} message_t;

void send(message_t message, mailbox_t* mailbox_ptr);
#include "receiver.h"
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

sem_t *Sender_SEM, *Receiver_SEM; // Semaphores

#define QUEUE_NAME "/lab1_posix_queue"

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    if (mailbox_ptr->flag == 1) {
        // Message Passing
        if (mq_receive(mailbox_ptr->storage.mq, message_ptr->mtext, sizeof(message_ptr->mtext), 0) == -1) {
            perror("mq_receive");
            exit(1);
        }
        printf("Receiving message: %s", message_ptr->mtext);
    } else if (mailbox_ptr->flag == 2) {
        sem_wait(Sender_SEM); // Wait for sender to send a message
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
        printf("Receiving message: %s", message_ptr->mtext);
        sem_post(Receiver_SEM); // Signal sender that message was received
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <method>\n", argv[0]);
        return 1;
    }

    int method = atoi(argv[1]);
    mailbox_t mailbox;
    mailbox.flag = method;

    // Initialize semaphores
    Sender_SEM = sem_open("/Sender_SEM", O_CREAT, 0644, 0);
    Receiver_SEM = sem_open("/Receiver_SEM", O_CREAT, 0644, 1);

    if (method == 1) {
        printf("Message Passing\n");
        mailbox.storage.mq = mq_open(QUEUE_NAME, O_RDONLY);
        if (mailbox.storage.mq == (mqd_t)-1) {
            perror("mq_open");
            exit(1);
        }
    } else if (method == 2) {
        printf("Shared Memory\n");
        int shmid = shmget(IPC_PRIVATE, 1024, IPC_CREAT | 0666);
        if (shmid == -1) {
            perror("shmget");
            exit(1);
        }
        mailbox.storage.shm_addr = (char*)shmat(shmid, NULL, 0);
        if (mailbox.storage.shm_addr == (char*)-1) {
            perror("shmat");
            exit(1);
        }
    }

    message_t message;
    clock_t start = clock();

    if (method == 2) {
        
    }

    do {
        receive(&message, &mailbox);
    } while (strcmp(message.mtext, "exit\n") != 0);

    printf("Sender exit!\n");

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Total time taken in receiving msg: %f s\n", time_taken);

    // Close and unlink semaphores
    sem_close(Sender_SEM);
    sem_close(Receiver_SEM);
    sem_unlink("/Sender_SEM");
    sem_unlink("/Receiver_SEM");

    return 0;
}

#define _POSIX_C_SOURCE 199309L

#include "receiver.h"
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

sem_t *Sender_SEM, *Receiver_SEM; // Semaphores

#define QUEUE_NAME "/lab1_posix_queue"

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    if (mailbox_ptr->flag == 1) {
        if (mq_receive(mailbox_ptr->storage.mq, message_ptr->mtext, sizeof(message_ptr->mtext), 0) == -1) {
            perror("mq_receive");
            exit(1);
        }
    } else if (mailbox_ptr->flag == 2) {
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
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
    Receiver_SEM = sem_open("/Receiver_SEM", O_CREAT, 0644, 0);

    if (method == 1) {
        mailbox.storage.mq = mq_open(QUEUE_NAME, O_RDONLY);
        if (mailbox.storage.mq == (mqd_t)-1) {
            perror("mq_open");
            exit(1);
        }
    } else if (method == 2) {
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
    struct timespec start, end;
    double time_taken = 0;

    do {
        sem_wait(Sender_SEM);

        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

        if (strcmp(message.mtext, "exit\n") == 0) {
            break;
        }

        printf("Receiving message: %s", message.mtext);

        // notify sender that receiver is ready
        sem_post(Receiver_SEM);
    } while (1);

    printf("\nSender exit!\n");
    printf("Total time taken in receiving msg: %f s\n", time_taken);

    // Close and unlink semaphores
    sem_close(Sender_SEM);
    sem_close(Receiver_SEM);
    sem_unlink("/Sender_SEM");
    sem_unlink("/Receiver_SEM");

    return 0;
}

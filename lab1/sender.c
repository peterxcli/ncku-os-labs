#define _POSIX_C_SOURCE 199309L

#include "sender.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define QUEUE_NAME "/lab1_posix_queue"
#define MAX_MSG_SIZE 1024
#define SHARED_MEMORY_NAME "/lab1_shared_memory"
#define SHARED_MEMORY_SIZE 1024

sem_t *Sender_SEM, *Receiver_SEM; // Semaphores

void send(message_t message, mailbox_t* mailbox_ptr){
    if (mailbox_ptr->flag == 1) {
        if (mq_send(mailbox_ptr->storage.mq, message.mtext, sizeof(message.mtext), 0) == -1) {
            perror("mq_send");
            exit(1);
        }
    } else if (mailbox_ptr->flag == 2) {
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <method> <input_file>\n", argv[0]);
        return 1;
    }

    int method = atoi(argv[1]);
    char* input_file = argv[2];
    mailbox_t mailbox;
    mailbox.flag = method;

    // Initialize semaphores (shared memory synchronization)
    Sender_SEM = sem_open("/Sender_SEM", O_CREAT, 0644, 0);  
    Receiver_SEM = sem_open("/Receiver_SEM", O_CREAT, 0644, 0); 

    if (method == 1) {
        printf("POSIX Message Passing\n");

        // Set up POSIX message queue attributes
        struct mq_attr attr;
        attr.mq_flags = 0;    // Non-blocking
        attr.mq_maxmsg = 10;           // Set queue capacity to 10 (max is 10)
        attr.mq_msgsize = MAX_MSG_SIZE; // Maximum message size
        attr.mq_curmsgs = 0;          // No current messages

        // Open the message queue with specified attributes
        mailbox.storage.mq = mq_open(QUEUE_NAME, O_CREAT | O_WRONLY, 0644, &attr);
        if (mailbox.storage.mq == (mqd_t)-1) {
            perror("mq_open");
            exit(1);
        }
    } else if (method == 2) {
        printf("Using POSIX Shared Memory\n");

        // Open shared memory
        int shm_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open");
            exit(1);
        }

        // Adjust the shared memory size
        if (ftruncate(shm_fd, SHARED_MEMORY_SIZE) == -1) {
            perror("ftruncate");
            exit(1);
        }

        // Map shared memory into address space
        mailbox.storage.shm_addr = mmap(0, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mailbox.storage.shm_addr == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }

        close(shm_fd); // No longer need the file descriptor
    }

    FILE* file = fopen(input_file, "r");
    if (!file) {
        perror("fopen");
        return 1;
    }

    message_t message;
    struct timespec start, end;
    double time_taken = 0;

    while (fgets(message.mtext, sizeof(message.mtext), file)) {
        sem_post(Sender_SEM);

        printf("Sending message: %s", message.mtext);

        clock_gettime(CLOCK_MONOTONIC, &start);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

        sem_wait(Receiver_SEM);
    }

    // Send an exit message
    {
        sem_post(Sender_SEM);

        strcpy(message.mtext, "exit\n");

        clock_gettime(CLOCK_MONOTONIC, &start);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

        printf("\nEnd of input file! exit!\n");
    }

    printf("Total time taken in sending msg: %f s\n", time_taken);

    fclose(file);

    if (method == 1) {
        // Close and unlink the POSIX message queue
        mq_close(mailbox.storage.mq);
        mq_unlink(QUEUE_NAME);
    } else if (method == 2) {
        munmap(mailbox.storage.shm_addr, SHARED_MEMORY_SIZE);
        shm_unlink(SHARED_MEMORY_NAME);
    }

    // Close and unlink semaphores
    sem_close(Sender_SEM);
    sem_close(Receiver_SEM);
    sem_unlink("/Sender_SEM");
    sem_unlink("/Receiver_SEM");

    return 0;
}

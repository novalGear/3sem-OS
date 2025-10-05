#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/wait.h>

const size_t MSG_SIZE = sizeof(size_t);

struct msg_buf {
    long mtype;
    size_t number;
};

void judge(int msgq_id, size_t runners_number, size_t judge_rcv_type) {
    size_t runners_ready = 0;
    struct msg_buf msg_temp;
    printf("Judge (pid = %d) created\n", (int)getpid());
    while (runners_ready < runners_number) {
        ssize_t rcv_res = msgrcv(msgq_id, (void*)&msg_temp, MSG_SIZE, 0, 0);
        if (rcv_res < 0) {
            perror("judge, msgrcv: ");
        } else {
            printf("Judge: Runner %lu ready\n", msg_temp.number);
            ++runners_ready;
        }
    }
    printf("Judge: All runners are ready, start the relay race\n");
    msg_temp.mtype = 1;
    int snd_res = msgsnd(msgq_id, (void*)&msg_temp, MSG_SIZE, 0);
    if (snd_res < 0) {
        perror("msgsnd: ");
    } else {
        ssize_t rcv_res = msgrcv(msgq_id, (void*)&msg_temp, MSG_SIZE, judge_rcv_type, 0);
        if (rcv_res < 0) {
            perror("judge, msgrcv: ");
        } else {
            printf("Judge: Last runner finished\n");
        }
    }
}

void runner(int msgq_id, size_t number) {
    printf("Runner %ld (pid = %d) created\n", number, (int)getpid());

    struct msg_buf msg = {.mtype = number, .number = number};
    int snd_res = msgsnd(msgq_id, (void*)&msg, MSG_SIZE, 0);
    if (snd_res < 0) {
        perror("runner, msgsnd: ");
    } else {
        printf("Runner %ld: ready\n", number);
        ssize_t rcv_res = msgrcv(msgq_id, (void*)&msg, MSG_SIZE, number, 0);
        if (rcv_res < 0) {
            perror("runner, msgrcv: ");
        } else {
            printf("Runner %ld: start!\n", number);
            msg.mtype = number + 1;
            snd_res = msgsnd(msgq_id, (void*)&msg, MSG_SIZE, 0);
            if (snd_res < 0) {
                perror("runner, msgsnd: ");
            } else {
                printf("Runner %ld: finish!\n", number);
            }
        }
    }
}

int main() {
    const size_t N = 10;

    int msgq_id = msgget(IPC_PRIVATE, IPC_CREAT | 0666);

    if (fork() == 0) {
        judge(msgq_id, N, N + 1);
        exit(0);
    }
    for (size_t runner_ind = 1; runner_ind < N + 1; runner_ind++) {
        if (fork() == 0) {
            runner(msgq_id, runner_ind);
            exit(0);
        }
    }

    for (size_t process_ind = 0; process_ind < N + 1; process_ind++) {
        wait(NULL);
    }
    msgctl(msgq_id, IPC_RMID, NULL);
}

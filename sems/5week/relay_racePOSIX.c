#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <mqueue.h>

#include <time.h>
#include <math.h>

const size_t MSG_SIZE = 8;

ssize_t mq_receive_with_set_prio(mqd_t msgq_id, char* msg_ptr, size_t msg_len, unsigned int set_prio) {
    bool received_flag = false;
    unsigned int rcv_prio = -1;
    ssize_t rcv_res = 0;
    while (!received_flag) {
        rcv_res = mq_receive(msgq_id, msg_ptr, msg_len, &rcv_prio);
        if (rcv_res < 0) {
            perror("mq_receive_with_set_prio: ");
            break;
        } else {
            if (rcv_prio == set_prio) {
                received_flag = true;
            }
            else {
                mq_send(msgq_id, msg_ptr, msg_len, rcv_prio);
            }
        }
    }
    return rcv_res;
}

void judge(const char* msgq_name, size_t runners_number, size_t judge_rcv_type) {
    mqd_t msgq_id = mq_open(msgq_name, O_RDWR);
    char msg_buffer[MSG_SIZE];
    size_t runners_ready = 0;
    printf("Judge (pid = %d) created\n", (int)getpid());

    while (runners_ready < runners_number) {
        unsigned int rcv_msg_prio = -1;
        ssize_t rcv_res = mq_receive(msgq_id, msg_buffer, MSG_SIZE, &rcv_msg_prio);
        if (rcv_res < 0) {
            perror("judge, mq_receive: ");
        } else {
            printf("Judge: Runner %u ready\n", rcv_msg_prio);
            ++runners_ready;
        }
    }
    printf("Judge: All runners are ready, start the relay race\n");

    int snd_res = mq_send(msgq_id, msg_buffer, MSG_SIZE, 1);
    if (snd_res < 0) {
        perror("mq_send: ");
    } else {
        ssize_t rcv_res = mq_receive_with_set_prio(msgq_id, msg_buffer, MSG_SIZE, judge_rcv_type);
        if (rcv_res < 0) {
            perror("judge, mq_receive: ");
        } else {
            printf("Judge: Last runner finished\n");
        }
    }
    mq_close(msgq_id);
}

void runner(const char* msgq_name, size_t number) {
    mqd_t msgq_id = mq_open(msgq_name, O_RDWR);
    printf("Runner %ld (pid = %d) created\n", number, (int)getpid());
    char msg_buffer[MSG_SIZE];
    int snd_res = mq_send(msgq_id, msg_buffer, MSG_SIZE, number);
    if (snd_res < 0) {
        perror("runner, mq_send: ");
    } else {
        printf("Runner %ld: ready\n", number);
        ssize_t rcv_res = mq_receive_with_set_prio(msgq_id, msg_buffer, MSG_SIZE, number);
        if (rcv_res < 0) {
            perror("runner, mq_receive: ");
        } else {
            printf("Runner %ld: start!\n", number);

            snd_res = mq_send(msgq_id, msg_buffer, MSG_SIZE, number + 1);
            if (snd_res < 0) {
                perror("runner, mq_send: ");
            } else {
                printf("Runner %ld: finish!\n", number);
            }
        }
    }
    mq_close(msgq_id);
}

double relay_race(size_t N) {
    const char msgq_name[] = "/queue";

    mq_unlink(msgq_name);

    struct mq_attr my_mq_attr = {
        .mq_flags = 0,          // Flags (ignored for mq_open())
        .mq_maxmsg = 10,        // Max. # of messages on queue
        .mq_msgsize = MSG_SIZE, // Max. message size (bytes)
        .mq_curmsgs = 0         // # of messages currently in queue
    };
    mqd_t msgq_id = mq_open(msgq_name, O_CREAT | O_RDWR, 0666, &my_mq_attr);
    if (msgq_id < 0) {
        perror("mq_open in main: ");
        // return 0;
    }
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    if (fork() == 0) {
        judge(msgq_name, N, N + 1);
        exit(0);
    }
    for (size_t runner_ind = 1; runner_ind < N + 1; runner_ind++) {
        if (fork() == 0) {
            runner(msgq_name, runner_ind);
            exit(0);
        }
    }

    for (size_t process_ind = 0; process_ind < N + 1; process_ind++) {
        wait(NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    mq_unlink(msgq_name);

    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                                  (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    return elapsed_time;
}

int main() {
    const int K = 10;  // количество запусков
    const size_t runners_number = 5;
    double times[K];

    for (int i = 0; i < K; i++) {
        times[i] = relay_race(runners_number);
    }

    double sum = 0.0;
    for (int i = 0; i < K; i++) {
        sum += times[i];
    }
    double mean = sum / K;

    double variance = 0.0;
    for (int i = 0; i < K; i++) {
        variance += (times[i] - mean) * (times[i] - mean);
    }
    variance /= K;

    double std_dev = sqrt(variance);

    printf("Average relay race time: %.6f seconds\n", mean);
    printf("Standard deviation: %.6f seconds\n", std_dev);

    return 0;
}

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdbool.h>

int main() {
    int child_process_num = 5;
    bool make_fork = true;
    printf("[%i]: parent process here\n", (int) getpid());
    for (int p_ind=0; p_ind<child_process_num; p_ind++) {
        if (make_fork) {
            pid_t pid = fork();
            make_fork = (pid == 0);
            if (pid != 0) {
                pid_t my_pid  = getpid();
                printf("[%d]: created process [%i]\n", (int) my_pid, (int) pid);
            } else {
                pid_t my_pid  = getpid();
                pid_t my_ppid = getppid();
                printf("[%d]: I am child of process [%i]\n", (int) my_pid, (int) my_ppid);
            }
        } else {
            continue;
        }
    }
    int status = 0;
    wait(&status);
    printf("[%i]: All child processes are terminated\n", (int) getpid());
    return 0;
}

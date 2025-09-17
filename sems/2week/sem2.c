#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

int main() {
    int child_process_num = 5;
    for (int p_ind=0; p_ind<child_process_num; p_ind++) {
        pid_t pid = fork();
        if (pid == 0) {
            pid_t my_pid  = getpid();
            pid_t my_ppid = getppid();
            printf("[%d]: I am child of process [%i]\n", (int) my_pid, (int) my_ppid);
            return 0;
        } else {
            printf("Parent process [%i] created child process [%i]\n", (int) getpid(), (int) pid);
        }
    }
    int status = 0;
    for (int p_ind=0; p_ind<child_process_num; p_ind++) {
        wait(&status);
    }
    printf("All child processes are terminated\n");
    return 0;
}

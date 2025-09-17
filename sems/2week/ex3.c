#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char* argv[]) {

    pid_t parent_pid = getpid();
    int num_count = argc - 1;

    for (int p_ind=0; p_ind<num_count; p_ind++) {
        int curr_num = atoi(argv[p_ind + 1]);
        pid_t pid = fork();
        if (pid == 0) {
            usleep( (useconds_t) curr_num * 1000);
            printf("%d ", curr_num);
            return 0;
        }
    }
    for (int p_ind=0; p_ind<num_count; p_ind++) {
        int status = 0;
        wait(&status);
    }
    printf("\n");
    return 0;
}

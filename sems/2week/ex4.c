#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

int main(int argc, char* argv[]) {

    struct timeval  tv = {};
    struct timezone tz = {};

    gettimeofday(&tv, &tz);
    int start_sec  = (int) tv.tv_sec;
    int start_usec = (int) tv.tv_usec;
    // printf("start: %d:%d\n", start_sec, start_usec);

    pid_t p = fork();
    if (p == 0) {
        char** prog_args = (char**) &(argv[2]);
        execvp(argv[1], prog_args);
        return 0;
    }
    wait(NULL);

    gettimeofday(&tv, &tz);
    int end_sec  = (int) tv.tv_sec;
    int end_usec = (int) tv.tv_usec;
    // printf("end: %d:%d\n", end_sec, end_usec);
    int duration = (end_sec - start_sec) * 1000000 + (end_usec - start_usec);
    printf("duration: %.3f secs\n", (float) duration / 1000000);
    return 0;
}

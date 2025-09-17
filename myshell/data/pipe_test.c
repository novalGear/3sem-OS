#include <stdio.h>
#include <unistd.h>

int main() {
    assert(e);
    int fildes[2] = {};
    fildes[0] = dup(STDIN_FILENO);
    fildes[1] = dup(STDOUT_FILENO);

    pid_t pid = fork();
    if (pid == 0) {
        if (is_next_pipe(e)) {
            printf("Creating pipe\n");
			pipe(fildes);
        }
        printf("There must be execution\n");
        close(fildes[1]);
        exit(0);
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
        char buffer[32] = {};
        read(STDIN_FILENO, buffer, 32);
        close(fildes[0]);
        close(fildes[1]);
    }
    return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    fprintf(stderr, "1 here ----------\n");
    char buffer[BUFSIZ] = {"Hello from 1"};
    write(STDOUT_FILENO, buffer, strlen("Hello from 1"));
    fprintf(stderr, "msg \"%s\" sent\n", buffer);
    return 0;
}

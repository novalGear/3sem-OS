#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    char buffer[BUFSIZ] = {};
    fprintf(stderr, "2 here ----------\n");
    read(STDIN_FILENO, buffer, BUFSIZ);
    fprintf(stderr, "String read: \"%s\"\n", buffer);
    strcpy(buffer, "Hello from 2");
    write(STDOUT_FILENO, buffer, strlen("Hello from 2"));
    fprintf(stderr, "msg \"%s\" sent\n", buffer);
    return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    char buffer[32] = {};
    fprintf(stderr, "start reading from stdin (test2)\n");
    read(STDIN_FILENO, buffer, 32);
    fprintf(stderr, "String read: \"%s\"\n", buffer);
    strcpy(buffer, "Hello from test2");
    write(STDOUT_FILENO, buffer, 32);
    return 0;
}

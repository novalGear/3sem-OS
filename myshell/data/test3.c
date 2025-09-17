#include <stdio.h>
#include <unistd.h>

int main() {
    char buffer[32] = {};
    fprintf(stderr, "start reading from stdin (test3)\n");
    read(STDIN_FILENO, buffer, 32);
    fprintf(stderr, "String read: \"%s\"\n", buffer);
    return 0;
}

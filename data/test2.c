#include <stdio.h>
#include <unistd.h>

int main() {
    char buffer[32] = {};
    printf("start reading from stdin\n");
    read(STDIN_FILENO, buffer, 32);
    printf("String read: \"%s\"\n", buffer);

    return 0;
}

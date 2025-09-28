#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    char buffer[BUFSIZ] = {};
    fprintf(stderr, "3 here ----------\n");
    read(STDIN_FILENO, buffer, BUFSIZ);
    fprintf(stderr, "String read: \"%s\"\n", buffer);
    return 0;
}

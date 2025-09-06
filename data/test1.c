#include <stdio.h>
#include <unistd.h>

int main() {
    char buffer[32] = {"Hello there?\n"};
    write(STDOUT_FILENO, buffer, 32);
    fprintf(stderr, "msg \"%s\" sent (test3)\n", buffer);
    return 0;
}

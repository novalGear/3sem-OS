#include <stdio.h>
#include <unistd.h>

int main() {
    char buffer[32] = {"Hello there?\n"};
    write(STDOUT_FILENO, buffer, 32);
    return 0;
}

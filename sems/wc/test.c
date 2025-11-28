#include <stdio.h>
#include <unistd.h>

int main() {

    fprintf(stdout, "1\n 2 3 4 5 6 7\n");
    fprintf(stderr, "a b cde\n");
    fflush(stderr);
    fflush(stdout);
    return 0;
}

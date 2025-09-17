#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

ssize_t safewrite(int fd, const void* buffer, size_t size) {
    assert(buffer);

    ssize_t written_total = 0;
    while ((size_t)written_total < size) {
        ssize_t written_on_write = write(fd, buffer + written_total, size - written_total);
        if (written_on_write < 0 && errno != EINTR) {
            perror("Error in safe write");
            return written_on_write;
        }
        written_total += written_on_write;
    }
    return written_total;
}

void stream(int fd_IN, int fd_OUT) {

    char buffer[BUFSIZ];
    ssize_t bytes_read    = -1;
    ssize_t bytes_written = 0;

    while (bytes_read != 0) {
        bytes_read = read(fd_IN, buffer, BUFSIZ);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("Error on read: ");
                break;
            }
        }
        bytes_written = safewrite(fd_OUT, buffer, bytes_read);
        if (bytes_written < 0 && errno != EINTR) {
            perror("Error in write: ");
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        stream(STDIN_FILENO, STDOUT_FILENO);
    } else {
        for (int file_ind=1; file_ind < argc; file_ind++) {
            int fd = 0;
            fd = open(argv[file_ind], O_RDONLY);
            if (fd < 0) {
                perror("Error while opening file in O_RDONLY mode: ");
                continue;
            }
            stream(fd, STDOUT_FILENO);
            int close_res = close(fd);
            if (close_res < 0) {
                perror("Error while closing file: ");
                continue;
            }
        }
    }

    return 0;
}

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/epoll.h>


#define DBG_PRINT(...)  printf("%s:%d ", __func__, __LINE__); \
                        printf(__VA_ARGS__);

#define HARD_CHECK(cond, msg) if ( (int)(cond) < 0) { perror(msg); exit(EXIT_FAILURE); }

#define BUFFER_SIZE (4)

typedef struct {
    size_t bytes;
    size_t words;
    size_t lines;
} wc_ctx_t;


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

ssize_t saferead(int fd, void* buffer, size_t size) {
    assert(buffer);
    ssize_t read_total = 0;
    while ((size_t)read_total < size) {
        ssize_t read_on_read = read(fd, (char*)buffer + read_total, size - read_total);
        if (read_on_read < 0) {
            if (errno == EINTR) continue;
            perror("Error in safe read");
            return read_on_read;
        }
        if (read_on_read == 0) {
            break;
        }
        read_total += read_on_read;
    }
    return read_total;
}

int count(int fd_from, wc_ctx_t* info)
{
    DBG_PRINT("\n");
    bool in_word = false;
    char buffer[BUFFER_SIZE];
    // while (true)
    // {
    DBG_PRINT("\n");
    // не знаем, сколько нужно прочитать, читаем кусками по BUFFER_SIZE
    ssize_t read_bytes = read(fd_from, buffer, BUFFER_SIZE);
    // HARD_CHECK(read_bytes, "Reading error");
    if (read_bytes == 0) {
        DBG_PRINT("file end\n");
        return 0;
    }
    if (read_bytes < 0) {
        perror("read");
        return -1;
    }

    DBG_PRINT("read success\n");
    info->bytes += read_bytes;
    for (ssize_t i = 0; i < read_bytes; ++i)
    {
        if (buffer[i] == '\n')
        {
            info->lines++;
        }
        if (!isspace(buffer[i]))
        {
            if (!in_word)
            {
                info->words++;
                in_word = true;
            }
        } else
        {
            in_word = false;
        }
    }
    // safewrite(fd_to, buffer, read_bytes);
    // }

    DBG_PRINT("info->bytes: %ld\n", info->bytes);
    return read_bytes;
}

void dump_wc_info(wc_ctx_t* info, const char* msg) {
    assert(msg);
    assert(info);

    printf("%s\n", msg);
    printf("bytes: %ld\n", info->bytes);
    printf("words: %ld\n", info->words);
    printf("lines: %ld\n", info->lines);
}

int main(int argc, char* const* argv) {

    if (argc < 2) {
        perror("too few arguments");
        return EXIT_FAILURE;
    }
    int STDOUT_pipefds[2];
    int STDERR_pipefds[2];

    if (pipe(STDOUT_pipefds) != 0)
    {
        perror("STDOUT pipe");
        return EXIT_FAILURE;
    }
    if (pipe(STDERR_pipefds) != 0)
    {
        perror("STDERR pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(STDOUT_pipefds[0]);                  // Closing read ds for child
        dup2(STDOUT_pipefds[1], STDOUT_FILENO);    // Dupping write fd to stdout (1)
        close(STDOUT_pipefds[1]);                  // Closing write fd

        close(STDERR_pipefds[0]);
        dup2(STDERR_pipefds[1], STDERR_FILENO);
        close(STDERR_pipefds[1]);

        execvp(argv[1], argv + 1);

        return 0;
    }
    close(STDOUT_pipefds[1]);
    close(STDERR_pipefds[1]);  // Closing write fd for parent

    // собираем данные
    int fd_out = STDOUT_pipefds[0];
    int fd_err = STDERR_pipefds[0];

    // подготовка epoll

    int epfd = epoll_create1(0);
    HARD_CHECK(epfd, "epoll create");

    struct epoll_event ev;
    ev.events = EPOLLIN;

    ev.data.fd = fd_out;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd_out, &ev) == -1) {
        perror("epoll_ctl: STDOUT");
        close(epfd);
        exit(EXIT_FAILURE);
    }

    ev.data.fd = fd_err;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd_err, &ev) == -1) {
        perror("epoll_ctl: STDERR");
        close(epfd);
        exit(EXIT_FAILURE);
    }
    // подготовка завершена

    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    wc_ctx_t stdout_info = {};
    wc_ctx_t stderr_info = {};

    int active_fds = 2;
    while (active_fds > 0) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            wc_ctx_t* info = (fd == fd_out) ? &stdout_info : &stderr_info;
            int result = count(fd, info);
            if (result <= 0) {
                close(fd);
                --active_fds;
            }
        }
    }

    dump_wc_info(&stdout_info, "STDOUT:");
    dump_wc_info(&stderr_info, "STDERR:");

    pid_t closed_pid;
    while ((closed_pid = wait(NULL)) != -1) {}

    return EXIT_SUCCESS;
}

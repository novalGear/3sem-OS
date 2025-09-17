#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include "mycp.h"

void clean_input_buffer() {
    while (getchar() != '\n')
        continue;
}

bool check_flags(struct flags_states* flags_values, int argc, char *const argv[])
{
    assert(flags_values != NULL);
    assert(argv != NULL);

    int opt = 0;
    const char optstring[] = "vif";
    struct option longoptions[] =
    {
        {"verbose",     0, 0, 'v'},
        {"interactive", 0, 0, 'i'},
        {"force",       0, 0, 'f'}
    };
    int optidx = 0;

    while ((opt = getopt_long(argc, argv, optstring, longoptions, &optidx)) != -1)
    {
        switch (opt)
        {
            case 'v':
                flags_values->verbose = true;
                flags_values->verbose_count += 1;
                break;
            case 'i':
                flags_values->interactive = true;
                flags_values->interactive_count += 1;
                break;
            case 'f':
                flags_values->force = true;
                flags_values->force_count += 1;
                break;

            default:
                fprintf(stderr, "option read error\n");
                return false;
        }
    }
    if ( flags_values->interactive && flags_values->force) {
        fprintf(stderr, "ERROR: --interactive and --force are exclusive, but both met in flags\n");
        return false;
    }
    return true;
}

void print_choose_option() {
    printf("Do you want to force copy? [y/n]\n");
}

bool get_interactive_permission() {
    char input = 0;
    print_choose_option();
    while (true)
    {
        scanf("%c", &input);
        switch (input)
        {
            case 'y':   return true;
            case 'n':   return false;
            default:
                printf("Error: Only [y] and [n] are read as answeres\n");
                clean_input_buffer();
                print_choose_option();
                break;
        }
    }
}

int combine_open_flags(struct flags_states* flags) {
    int open_flags = O_WRONLY | O_CREAT;
    if (flags->force) {
        open_flags |= O_TRUNC;
    } else {
        open_flags |= O_EXCL;
    }
    return open_flags;
}

int is_dir(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

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

int pstream(const char* IN_path, const char* OUT_path, int open_flags) {
    assert(IN_path);
    assert(OUT_path);

    int fd_IN  = open(IN_path, O_RDONLY);
    if (fd_IN < 0) {
        printf("file name: [%s]\n", IN_path);
        perror("Error while opening file in O_RDONLY mode: %s\n");
        return -1;
    }
    int fd_OUT = open(OUT_path, open_flags, 0666);
    if (fd_OUT < 0) {
        printf("file name: [%s]\n", OUT_path);
        perror("Error while opening file: ");
        return -1;
    }

    stream(fd_IN, fd_OUT);

    int close_res = close(fd_IN);
    if (close_res < 0) {
        perror("Error while closing file: ");
        // continue;
    }
    close_res = close(fd_OUT);
    if (close_res < 0) {
        perror("Error while closing file: ");
        // continue;
    }
    return 0;
}

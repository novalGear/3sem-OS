#pragma once

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>

struct flags_states
{
    bool verbose;
    bool interactive;
    bool force;

    size_t verbose_count;
    size_t interactive_count;
    size_t force_count;
};

void clean_input_buffer();

bool check_flags(struct flags_states *flags_values, int argc, char *const argv[]);
void print_choose_option();
bool get_interactive_permission();

int combine_open_flags(struct flags_states* flags);

int is_dir(const char *path);

ssize_t safewrite(int fd, const void* buffer, size_t size);
void stream(int fd_IN, int fd_OUT);
int pstream(const char* IN_path, const char* OUT_path, int open_flags);

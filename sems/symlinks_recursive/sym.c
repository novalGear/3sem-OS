#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#ifdef DEBUG

#define DBG_PRINT(...)  printf("%s:%d ", __func__, __LINE__); \
                        printf(__VA_ARGS__);
#else
#define DBG_PRINT(...)
#endif

// const int PATH_MAX = 64;
const int DEPTH_MAX = 100;

int print_info(const char* full_path, const char* file_name) {
    assert(file_name);
    assert(full_path);

    struct stat file_stat;
    int stat_result = lstat(full_path, &file_stat);

    if (stat_result == -1) {
        perror("lstat in print_info");
        return -1;
    }

    printf("%s", file_name);

    if (S_ISLNK(file_stat.st_mode)) {
        char link_target[1024];
        ssize_t len = readlink(full_path, link_target, sizeof(link_target) - 1);

        if (len != -1) {
            link_target[len] = '\0';
            printf(" -> %s", link_target);
        } else {
            perror("readlink: ");
        }
    }

    printf("\n");
    return 0;
}

int find_symlinks_recursive(const char* dir_path, int depth) {
    assert(dir_path);

    DIR* d = opendir(dir_path);
    if (d == NULL) {
        return 0;
    }

    struct dirent* e;
    struct stat st;

    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, e->d_name) >= (int)sizeof(full_path)) {
            fprintf(stderr, "Path too long: %s/%s\n", dir_path, e->d_name);
            continue;
        }

        if (lstat(full_path, &st) == -1) {
            continue;
        }

        if (S_ISLNK(st.st_mode)) {
            print_info(full_path, e->d_name);
        }
        else if (S_ISDIR(st.st_mode)) {
            if (depth < DEPTH_MAX) {
                find_symlinks_recursive(full_path, depth + 1);
            }
        }
    }

    closedir(d);
    return 0;
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        printf("dir name expected as first argument\n");
        return -1;
    }

    struct stat st;
    if (stat(argv[1], &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", argv[1]);
        return -1;
    }

    find_symlinks_recursive(argv[1], 0);
    return 0;
}

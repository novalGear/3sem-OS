#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>

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

struct flags_states {
    bool all;
    bool directory;
    bool long_opt;
    bool inode;
    bool numeric;
    bool recursive;
};

int check_flags(struct flags_states* flags_values, int argc, char *const argv[])
{
    assert(flags_values != NULL);
    assert(argv != NULL);

    int opt = 0;
    const char optstring[] = "adlinR";
    struct option longoptions[] =
    {
        {"all",         0, 0, 'a'},
        {"directory",   0, 0, 'd'},
        {"long",        0, 0, 'l'},
        {"inode",       0, 0, 'i'},
        {"numeric",     0, 0, 'n'},
        {"recursive",   0, 0, 'R'}
    };
    int optidx = 0;
    int n_flags = 0;
    while ((opt = getopt_long(argc, argv, optstring, longoptions, &optidx)) != -1)
    {
        switch (opt)
        {
            case 'a':
                flags_values->all = true;
                ++n_flags;
                break;
            case 'd':
                flags_values->directory = true;
                ++n_flags;
                break;
            case 'l':
                flags_values->long_opt = true;
                ++n_flags;
                break;
            case 'i':
                flags_values->inode = true;
                ++n_flags;
                break;
            case 'n':
                flags_values->numeric = true;
                ++n_flags;
                break;
            case 'R':
                flags_values->recursive = true;
                ++n_flags;
                break;

            default:
                fprintf(stderr, "option read error\n");
                return false;
        }
    }
    return n_flags;
}

void format_mode(mode_t mode, char* str) {
    str[0] = '\0';

    if (S_ISREG(mode)) str[0] = '-';
    else if (S_ISDIR(mode)) str[0] = 'd';
    else if (S_ISLNK(mode)) str[0] = 'l';
    else if (S_ISCHR(mode)) str[0] = 'c';
    else if (S_ISBLK(mode)) str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else str[0] = '?';

    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';

    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';

    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

int print_info(const char* full_path, const char* file_name, struct flags_states* fs) {
    assert(file_name);
    assert(full_path);

    struct stat file_stat;
    int stat_result = lstat(full_path, &file_stat);

    if (fs->long_opt) {
        if (stat_result == -1) {
            perror("lstat in print_info");
            return -1;
        }
        char mode_str[11];
        format_mode(file_stat.st_mode, mode_str);
        printf("%s ", mode_str);

        printf("%3lu ", (unsigned long)file_stat.st_nlink);

        struct passwd *pwd = getpwuid(file_stat.st_uid);
        struct group  *grp = getgrgid(file_stat.st_gid);
        printf("%-8s %-8s ", pwd ? pwd->pw_name : "unknown", grp ? grp->gr_name : "unknown");

        if (S_ISBLK(file_stat.st_mode) || S_ISCHR(file_stat.st_mode)) {
                printf("%3d, %3d ", major(file_stat.st_rdev), minor(file_stat.st_rdev));
        } else {
            printf("%8lld ", (long long)file_stat.st_size);
        }

        time_t now = time(NULL);
        struct tm *tm_mtime = localtime(&file_stat.st_mtime);

        char time_str[100];
        if (now - file_stat.st_mtime > 6 * 30 * 24 * 60 * 60) {
            strftime(time_str, sizeof(time_str), "%b %d  %Y", tm_mtime);
        } else {
            strftime(time_str, sizeof(time_str), "%b %d %H:%M", tm_mtime);
        }
        printf("%s ", time_str);
    }
    if (fs->inode) {
        printf("%ld ", (unsigned long)file_stat.st_ino);
    }
    printf("%s\n", file_name);
    return 0;
}

int print_files_in_dir(const char* dir_path, struct flags_states* fs) {
    assert(dir_path);

    DIR* d = opendir(dir_path);
    if (d == NULL) {
        perror("Could not open current directory");
        return -1;
    }
    struct dirent* e;

    while ( (e = readdir(d)) != NULL) {
        // пропускаем текущую и родительскую директорию
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            if (!fs->all || strlen(e->d_name) <= 2) {
                continue;
            }
        }
        char sub_path[PATH_MAX];
        // конструируем путь к файлу/папке
        // и проверка на размер пути
        if (snprintf(sub_path, sizeof(sub_path), "%s/%s", dir_path, e->d_name) >= (int)sizeof(sub_path)) {
            fprintf(stderr, "Error: Path too long: %s/%s\n", dir_path, e->d_name);
            continue;
        }
        // рекурсивный обход
        if (e->d_type == DT_DIR) {
            print_info(sub_path, e->d_name, fs);
            if (fs -> recursive) {
                printf("\n%s:\n", sub_path);
                print_files_in_dir(sub_path, fs);
            }
        }else {
            // только сами директории без содержимого с этим флагом
            if (fs->directory) {
                continue;
            }
            print_info(sub_path, e->d_name, fs);
        }
    }
    closedir(d);
    return 0;
}

int main(int argc, char* argv[]) {
    struct flags_states fs;
    int n_flags = check_flags(&fs, argc, argv);

    if (argc == 1 + n_flags) {
        print_files_in_dir(".", &fs);
    } else {
        for (int arg_ind=1; arg_ind < argc; arg_ind++) {
            struct stat st;
            stat(argv[arg_ind], &st);

            if (S_ISDIR(st.st_mode)) {
                print_files_in_dir(argv[arg_ind], &fs);
            } else if (S_ISREG(st.st_mode)) {
                print_info(argv[arg_ind], argv[arg_ind], &fs);
            }
        }
    }
    return 0;
}

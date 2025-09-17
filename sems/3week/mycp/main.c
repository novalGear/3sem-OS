
#include "mycp.h"

int main(int argc, char* argv[]) {
    struct flags_states flags = {};
    if (! check_flags(&flags, argc, argv)) {
        return -1;
    }
    if (flags.interactive && get_interactive_permission()) {
        flags.force = true;
    }
    size_t arguments_counter = 1 + flags.verbose_count + flags.interactive_count + flags.force_count;

    int open_flags = combine_open_flags(&flags);
    if (is_dir(argv[argc-1])) {
        char dest_dir[BUFSIZ / 2];
        strncpy(dest_dir, argv[argc-1], strlen(argv[argc-1]));

        for (int file_ind = arguments_counter; file_ind < argc - 1; file_ind++) {
            char dest_file_path[BUFSIZ] = {};
            snprintf(dest_file_path, BUFSIZ, "./%s/%s", dest_dir, argv[file_ind]);

            const char* IN_path  = argv[file_ind];
            const char* OUT_path = dest_file_path;
            int pstream_res = pstream(IN_path, OUT_path, open_flags);
            if (flags.verbose && pstream_res == 0) {
                printf("\'%s\' -> \'%s\'\n", IN_path, OUT_path);
            }
        }
    } else {
        size_t curr_arg_ind = arguments_counter;
        const char* IN_path  = argv[curr_arg_ind];
        const char* OUT_path = argv[curr_arg_ind + 1];
        int pstream_res = pstream(IN_path, OUT_path, open_flags);
        if (flags.verbose && pstream_res == 0) {
            printf("\'%s\' -> \'%s\'\n", IN_path, OUT_path);
        }
    }
    return 0;
}

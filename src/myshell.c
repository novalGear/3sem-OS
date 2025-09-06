#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "parser.h"

#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)

void
cmd_struct_print(const struct command* cmd) {
	assert(cmd);
	DPRINTF("cmd name: \"%s\"\n", cmd->exe);
	for (uint32_t arg_ind=0; arg_ind<cmd->arg_count; arg_ind++) {
		DPRINTF("%u: \"%s\"\n", arg_ind, cmd->args[arg_ind]);
	}
}

bool is_next_pipe(const struct expr* e) {
    assert(e);  // May be unnecessary
    return (e != NULL) && (e->next != NULL) && (e->next->type == EXPR_TYPE_PIPE);
}

void
execute_cmd(const struct expr* e) {
    assert(e);
	static int input_fd = STDIN_FILENO;
	int fildes[2] = {};
	bool not_last_cmd = is_next_pipe(e);
	if (not_last_cmd) {
		DPRINTF("Not last command -> pipe\n");
		pipe(fildes);
	}
    pid_t pid = fork();
    if (pid == 0) {
		if (input_fd != STDIN_FILENO) {
			dup2(input_fd, STDIN_FILENO);
			close(input_fd);
		}
        if (not_last_cmd) {
			dup2(fildes[1], STDOUT_FILENO);
			close(fildes[0]);
			close(fildes[1]);
        }
        DPRINTF("There goes execution\n");
		const uint32_t cmd_args_count = e->cmd.arg_count;
		char* args[cmd_args_count + 2] = {};
		char temp_buf[5] = {"grep"};
		if (e->cmd.arg_count > 1) {
			args[0] = temp_buf;
			memcpy(args + 1, e->cmd.args, sizeof(char*) * e->cmd.arg_count);

		}
		for (uint32_t arg_ind=0; arg_ind<e->cmd.arg_count + 1; arg_ind++) {
			DPRINTF("%u: \"%s\"\n", arg_ind, args[arg_ind]);
		}
		// DPRINTF("cmd: 	\"%s\"\n", e->cmd.exe);
		cmd_struct_print(&(e->cmd));
		execvp(e->cmd.exe, args);
		DPRINTF("exec finished\n");
        exit(0);
    } else {
		DPRINTF("parent here\n");
		if (input_fd != STDIN_FILENO) {
			close(input_fd);
		}
		if (not_last_cmd) {
			close(fildes[1]);
			input_fd = fildes[0];
		}
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

void
process_expr(const struct expr* e) {
    assert(e);
    if (e->type == EXPR_TYPE_COMMAND) {
			printf("\tCommand: %s", e->cmd.exe);
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
				printf(" %s", e->cmd.args[i]);
			printf("\n");
            execute_cmd(e);
		} else if (e->type == EXPR_TYPE_PIPE) {
			printf("\tPIPE\n");
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
		} else {
			assert(false);
	}
}

void execute_command_line(const struct command_line* line) {
    assert(line);
	printf("================================\n");
	printf("Command line:\n");
	printf("Is background: %d\n", (int)line->is_background);
	printf("Output: ");
	if (line->out_type == OUTPUT_TYPE_STDOUT) {
		printf("stdout\n");
	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
		printf("new file - \"%s\"\n", line->out_file);
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
		printf("append file - \"%s\"\n", line->out_file);
	} else {
		assert(false);
	}
	printf("Expressions:\n");
	const struct expr *e = line->head;
	while (e != NULL) {
		process_expr(e);
		e = e->next;
	}
}

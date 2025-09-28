#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "parser.h"

#ifdef DEBUG
	#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
   #define DPRINTF(...)
#endif

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
			DPRINTF("INPUT FD != STDIN\n");
			dup2(input_fd, STDIN_FILENO);
			close(input_fd);
		}
        if (not_last_cmd) {
			DPRINTF("Not last command\n");
			dup2(fildes[1], STDOUT_FILENO);
			close(fildes[0]);
			close(fildes[1]);
        }
        DPRINTF("There goes execution\n");
		cmd_struct_print(&(e->cmd));
		execvp(e->cmd.exe, e->cmd.args);
		perror("execvp failed");
        exit(1);
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
			DPRINTF("\tCommand: %s", e->cmd.exe);
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
				DPRINTF(" %s", e->cmd.args[i]);
			DPRINTF("\n");
            execute_cmd(e);
		} else if (e->type == EXPR_TYPE_PIPE) {
			DPRINTF("\tPIPE\n");
		} else {
			assert(false);
	}
}

void execute_command_line(const struct command_line* line) {
    assert(line);
	DPRINTF("================================\n");
	DPRINTF("Command line:\n");
	DPRINTF("Expressions:\n");
	const struct expr *e = line->head;
	while (e != NULL) {
		process_expr(e);
		e = e->next;
	}
}

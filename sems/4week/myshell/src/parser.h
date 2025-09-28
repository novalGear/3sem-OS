#pragma once

#include <stdbool.h>
#include <stdint.h>

struct parser;

enum parser_error {
	PARSER_ERR_NONE,
	PARSER_ERR_PIPE_WITH_NO_LEFT_ARG,
	PARSER_ERR_PIPE_WITH_LEFT_ARG_NOT_A_COMMAND,
	PARSER_ERR_TOO_LATE_ARGUMENTS,
	PARSER_ERR_ENDS_NOT_WITH_A_COMMAND,
};

struct command {
	char *exe;
	char** args;
	uint32_t arg_count;
	uint32_t arg_capacity;
};

enum expr_type {
	EXPR_TYPE_COMMAND,
	EXPR_TYPE_PIPE,
};

struct expr {
	enum expr_type type;
	/** Valid if the type is COMMAND. */
	struct command cmd;
	struct expr *next;
};

struct command_line {
	struct expr *head;
	struct expr *tail;
};

void
command_line_delete(struct command_line *line);

struct parser *
parser_new(void);

void
parser_feed(struct parser *p, const char *str, uint32_t len);

enum parser_error
parser_pop_next(struct parser *p, struct command_line **out);

void
parser_delete(struct parser *p);

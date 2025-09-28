#include "parser.h"

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
	#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
   #define DPRINTF(...)
#endif

struct parser {
	char *buffer;
	uint32_t size;
	uint32_t capacity;
};

enum token_type {
	TOKEN_TYPE_NONE,
	TOKEN_TYPE_STR,
	TOKEN_TYPE_NEW_LINE,
	TOKEN_TYPE_PIPE,
};

struct token {
	enum token_type type;
	char *data;
	uint32_t size;
	uint32_t capacity;
};

void token_dump(const struct token* t) {
	assert(t);
	DPRINTF("Token type: %d\n", t->type);
	if (t->data != NULL) {
		DPRINTF("data: \"%s\"\n", t->data);
	}
}

static char *
token_strdup(const struct token *t)
{
	assert(t->type == TOKEN_TYPE_STR);
	assert(t->size > 0);
	char *res = malloc(t->size + 1);
	memcpy(res, t->data, t->size);
	res[t->size] = 0;
	return res;
}

static void
token_append(struct token *t, char c)
{
	if (t->size == t->capacity) {
		t->capacity = (t->capacity + 1) * 2;
		t->data = realloc(t->data, sizeof(*t->data) * t->capacity);
	} else {
		assert(t->size < t->capacity);
	}
	t->data[t->size++] = c;
}

static void
token_reset(struct token *t)
{
	t->size = 0;
	t->type = TOKEN_TYPE_NONE;
}

static void
command_append_arg(struct command *cmd, char *arg)
{
	if (cmd->arg_count == cmd->arg_capacity) {
		cmd->arg_capacity = (cmd->arg_capacity + 1) * 2;
		cmd->args = realloc(cmd->args, sizeof(*cmd->args) * cmd->arg_capacity);
	} else {
		assert(cmd->arg_count < cmd->arg_capacity);
	}
	cmd->args[cmd->arg_count++] = arg;
}

void
command_line_delete(struct command_line *line)
{
	DPRINTF("command line delete\n");
	while (line->head != NULL) {
		struct expr *e = line->head;
		if (e->type == EXPR_TYPE_COMMAND) {
			struct command *cmd = &e->cmd;
			// free(cmd->exe);
			for (uint32_t i = 0; i < cmd->arg_count; ++i)
				free(cmd->args[i]);
			free(cmd->args);
		}
		line->head = e->next;
		free(e);
	}
	free(line);
}

static void
command_line_append(struct command_line *line, struct expr *e)
{
	if (line->head == NULL)
		line->head = e;
	else
		line->tail->next = e;
	line->tail = e;
}

struct parser *
parser_new(void)
{
	return calloc(1, sizeof(struct parser));
}

void
parser_feed(struct parser *p, const char *str, uint32_t len)
{
	uint32_t cap = p->capacity - p->size;
	if (cap < len) {
		uint32_t new_capacity = (p->capacity + 1) * 2;
		if (new_capacity - p->size < len)
			new_capacity = p->size + len;
		p->buffer = realloc(p->buffer, sizeof(*p->buffer) * new_capacity);
		p->capacity = new_capacity;
	}
	memcpy(p->buffer + p->size, str, len);
	p->size += len;
	assert(p->size <= p->capacity);
}

static void
parser_consume(struct parser *p, uint32_t size)
{
	assert(p->size >= size);
	if (size == p->size) {
		p->size = 0;
		return;
	}
	memmove(p->buffer, p->buffer + size, p->size - size);
	p->size -= size;
}

const char*
skip2first_nonspace(const char* c) {
	assert(c);
	DPRINTF("skipping until nonspace character:\n");
	while (*c == ' ') {
		DPRINTF("%c", *c);
		++c;
	}
	DPRINTF("\ndone!\n");
	return c;
}

static uint32_t
parse_token(const char *pos, const char *end, struct token *out)
{
	DPRINTF("Parse token\n");
	token_reset(out);
	const char *begin = pos;
	DPRINTF("%c[%d]", *pos, *pos);
	pos = skip2first_nonspace(pos);
	pos = pos > end ? end : pos;
	out->type = TOKEN_TYPE_STR;
	DPRINTF("%c[%d]", *pos, *pos);

	if (*pos == '\r') {
		out->type = TOKEN_TYPE_NEW_LINE;
		return pos + 2 - begin;
	} else if (*pos == '\n') {
		out->type = TOKEN_TYPE_NEW_LINE;
		return pos + 1 - begin;
	} else if (*pos == '|') {
		out->type = TOKEN_TYPE_PIPE;
		return pos + 1 - begin;
	}
	while (pos < end) {
		char c = *pos;
		DPRINTF("%c[%d]", *pos, *pos);
		bool token_end = (c == ' ') || (c == '\n') || (c == '\r');
		if (token_end) {
			DPRINTF("token parsed:\n");
			token_dump(out);
			return pos - begin;
		}
		token_append(out, c);
		++pos;
	}
	return 0;
}

enum parser_error
parser_pop_next(struct parser *p, struct command_line **out)
{
	struct command_line *line = calloc(1, sizeof(*line));
	char *pos = p->buffer;
	const char *begin = pos;
	char *end = pos + p->size;
	struct token token = {0};
	enum parser_error res = PARSER_ERR_NONE;

	while (pos < end) {
		uint32_t used = parse_token(pos, end, &token);
		if (used == 0)
			goto return_no_line;
		pos += used;
		struct expr *e;
		switch(token.type) {
		case TOKEN_TYPE_STR:
			if (line->tail != NULL && line->tail->type == EXPR_TYPE_COMMAND) {
				command_append_arg(&line->tail->cmd, token_strdup(&token));
				continue;
			}
			e = calloc(1, sizeof(*e));
			e->type = EXPR_TYPE_COMMAND;
			e->cmd.exe = token_strdup(&token);
			DPRINTF("cmd name: [%s]\n", e->cmd.exe);
			command_append_arg(&e->cmd, e->cmd.exe);
			command_line_append(line, e);
			continue;

		case TOKEN_TYPE_NEW_LINE:
			/* Skip new lines. */
			if (line->tail == NULL)
				continue;
			goto close_and_return;
		case TOKEN_TYPE_PIPE:
			if (line->tail == NULL) {
				res = PARSER_ERR_PIPE_WITH_NO_LEFT_ARG;
				goto return_error;
			}
			if (line->tail->type != EXPR_TYPE_COMMAND) {
				res = PARSER_ERR_PIPE_WITH_LEFT_ARG_NOT_A_COMMAND;
				goto return_error;
			}
			e = calloc(1, sizeof(*e));
			e->type = EXPR_TYPE_PIPE;
			command_line_append(line, e);
			continue;
		default:
			assert(false);
		}
	}

close_and_return:
	if (token.type == TOKEN_TYPE_NEW_LINE) {
		DPRINTF("last token - NEW LINE\n");
		assert(line->tail != NULL);
		parser_consume(p, pos - begin);
		if (line->tail->type != EXPR_TYPE_COMMAND) {
			res = PARSER_ERR_ENDS_NOT_WITH_A_COMMAND;
			goto return_no_line;
		}
		res = PARSER_ERR_NONE;
		*out = line;
		goto return_final;
	}
	res = PARSER_ERR_TOO_LATE_ARGUMENTS;
	goto return_error;

return_error:
	DPRINTF("return error\n");
	/*
	 * Try to skip the whole current line. It can't be executed but can't
	 * just crash here because of that.
	 */
	while (pos < end) {
		uint32_t used = parse_token(pos, end, &token);
		if (used == 0)
			break;
		pos += used;
		if (token.type == TOKEN_TYPE_NEW_LINE) {
			parser_consume(p, pos - begin);
			goto return_no_line;
		}
	}
	res = PARSER_ERR_NONE;
	goto return_no_line;

return_no_line:
	DPRINTF("return no line\n");
	command_line_delete(line);
	*out = NULL;

return_final:
	DPRINTF("return final\n");
	free(token.data);
	return res;
}

void
parser_delete(struct parser *p)
{
	free(p->buffer);
	free(p);
}

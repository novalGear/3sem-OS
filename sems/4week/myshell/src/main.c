#include <unistd.h>
#include <stdio.h>

#include "myshell.h"
#include "parser.h"

#ifdef DEBUG
	#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
   #define DPRINTF(...)
#endif

int main() {
    const size_t buf_size = 1024;
	char buf[buf_size] = {};
	int rc = 0;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			DPRINTF("parser pop next\n");
			enum parser_error err = parser_pop_next(p, &line);
			DPRINTF("cmd line ready, err = %d\n", err);

			if (err == PARSER_ERR_NONE && line == NULL) {
				DPRINTF("empty cmd line\n");
				break;
			}
			if (err != PARSER_ERR_NONE) {
				DPRINTF("Error: %d\n", (int)err);
				continue;
			}
			DPRINTF("Exec cmd line...\n\n");
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}

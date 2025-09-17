GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

SOURCES_DIR = src
SOURCES = $(wildcard $(SOURCES_DIR)/*.c)
HEADERS = $(wildcard $(SOURCES_DIR)/*.h)
APP_DIR = app

all:
	gcc $(GCC_FLAGS) $(SOURCES) -o $(APP_DIR)/mybash -I src

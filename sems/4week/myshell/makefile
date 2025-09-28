GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

SOURCES_DIR = src
SOURCES = $(wildcard $(SOURCES_DIR)/*.c)
APP_DIR = app
DATA_DIR = data
EXE = mybash
DEBUG_EXE = mybashd

all:
	make compile
	make compile_test
	make test

setup:
	mkdir $(APP_DIR)

run:
	./$(APP_DIR)/$(EXE)

compile_debug:
	gcc $(GCC_FLAGS) $(SOURCES) -o $(APP_DIR)/$(DEBUG_EXE) -DDEBUG -I src

compile:
	gcc $(GCC_FLAGS) $(SOURCES) -o $(APP_DIR)/$(EXE) -I src

test:
	./$(APP_DIR)/$(EXE) < ./data/test.txt

compile_test:
	gcc $(GCC_FLAGS) $(DATA_DIR)/1.c -o $(DATA_DIR)/1
	gcc $(GCC_FLAGS) $(DATA_DIR)/2.c -o $(DATA_DIR)/2
	gcc $(GCC_FLAGS) $(DATA_DIR)/3.c -o $(DATA_DIR)/3

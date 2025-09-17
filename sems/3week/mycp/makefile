GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant
SOURCES = main.c mycp.c
DATA = $(wildcard ./*.txt)
TESTDIR = testdir

all:
	make clean
	gcc $(GCC_FLAGS) $(SOURCES) -o mycp
	./mycp -v main.c newfile.txt
	./mycp -v main.c newfile.txt
	./mycp -v -f main.c newfile.txt
	./mycp -v -i main.c newfile.txt
	./mycp -v main.c testdir
	./mycp -v mycp.h mycp.c testdir

clean:
	-rm $(DATA)
	-rm -rf $(TESTDIR)
	mkdir $(TESTDIR)

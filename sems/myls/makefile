
all:
	gcc main.c -Wall -Wextra -o myls
	make testing

testing:
	./myls -l myls testdir
	./myls -R -l -i
	./myls -R -d -l
	./myls -a -R

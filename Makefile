cshell: cshell.c
	gcc -g -Wall -o cshell cshell.c

cshell_prod: cshell.c
	gcc -Wall -Werror -o cshell cshell.c
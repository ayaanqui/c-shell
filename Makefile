spawnshell: spawnshell.c
	gcc -g -Wall -o spawnshell spawnshell.c

spawnshell_prod: spawnshell.c
	gcc -Wall -Werror -o spawnshell spawnshell.c
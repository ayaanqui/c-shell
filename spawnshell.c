/* $begin shellmain */
#include <fcntl.h>
#include <spawn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXARGS 128
#define MAXLINE 8192 /* Max text line length */

extern char **environ; /* Defined by libc */

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

int main()
{
    char cmdline[MAXLINE]; /* Command line */

    while (1)
    {
        char *result;
        /* Read */
        printf("CS361 > ");
        result = fgets(cmdline, MAXLINE, stdin);
        if (result == NULL && ferror(stdin))
        {
            fprintf(stderr, "fatal fgets error\n");
            exit(EXIT_FAILURE);
        }

        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid = 0;       /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return; /* Ignore empty lines */

    if (!builtin_command(argv))
    {
        // TODO: run the command argv using posix_spawnp.
        printf("not yet implemented :(\n");
        /* Parent waits for foreground job to terminate */
        if (!bg)
        {
            int status;
            if (waitpid(pid, &status, 0) < 0)
                unix_error("waitfg: waitpid error");
        }
        else
            printf("%d %s", pid, cmdline);
    }
    else
        waitpid(pid, 0, 0);
}

/**
 * Makes copy of src into dst given a start and end position, 
 * if the user does not want to specify an end the user can simply send a very large number for end
 * and the function will stop copying once we reach the end of src.
 * Also, I believe this function is only doing a shallow copy and not a deep copy. I figured that this wouldn't matter
 * since I will not be changing any of these arrays, and they could be seen as immutable.
 * 
 * This function was written completely by me for this hw as I thought this was the best way to approach
 * the problem.
 */
void make_copy(char **dst, char **src, int start, int end)
{
    int dst_start = 0;
    for (int i = start; i < end && src[i] != NULL; ++i)
    {
        dst[dst_start] = src[i];
        ++dst_start;
    }
}

int call_processes(char **argv, posix_spawn_file_actions_t *actions, int *pid)
{
    if (0 != posix_spawnp(pid, argv[0], actions, NULL, argv, environ))
    {
        perror("spawn failed");
        return 0;
    }
    return 1;
}

int create_pipe(char **lhs, char **rhs)
{
    int child_status;
    posix_spawn_file_actions_t actions1, actions2;
    int pipe_fds[2];
    int pid1, pid2;

    posix_spawn_file_actions_init(&actions1);
    posix_spawn_file_actions_init(&actions2);

    pipe(pipe_fds);

    posix_spawn_file_actions_adddup2(&actions1, pipe_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions1, pipe_fds[0]);

    posix_spawn_file_actions_adddup2(&actions2, pipe_fds[0], STDIN_FILENO);
    posix_spawn_file_actions_addclose(&actions2, pipe_fds[1]);

    call_processes(lhs, &actions1, &pid1);
    call_processes(rhs, &actions2, &pid2);

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    waitpid(pid1, &child_status, 0);
    return 1;
}

int parse_operators(char **argv)
{
    int size = 0;
    while (argv[size] != NULL)
        ++size;

    for (int i = 0; argv[i] != NULL; ++i)
    {
        if (argv[i][0] == '|')
        {
            char **lhs = (char **)malloc((i - 1) * sizeof(char));
            char **rhs = (char **)malloc((size - i - 2) * sizeof(char));
            make_copy(lhs, argv, 0, i);
            make_copy(rhs, argv, i + 1, 10);

            return create_pipe(lhs, rhs);
        }
        else if (argv[i][0] == '<')
        {
            printf("<\n");
            return 1;
        }
        else if (argv[i][0] == '>')
        {
            printf(">\n");
            return 1;
        }
        else if (argv[i][0] == ';')
        {
            char **lhs = (char **)malloc((i - 1) * sizeof(char));
            char **rhs = (char **)malloc((size - i - 2) * sizeof(char));
            make_copy(lhs, argv, 0, i);
            make_copy(rhs, argv, i + 1, 10);

            posix_spawn_file_actions_t actions1, actions2;
            int pid1, pid2, child_status;

            // Command 1 (lhs)
            posix_spawn_file_actions_init(&actions1);
            call_processes(lhs, &actions1, &pid1);
            waitpid(pid1, &child_status, 0);

            // Command 2 (rhs)
            posix_spawn_file_actions_init(&actions2);
            call_processes(rhs, &actions2, &pid2);
            waitpid(pid2, &child_status, 0);

            return 1;
        }
    }

    posix_spawn_file_actions_t actions;
    int pid, child_status;
    posix_spawn_file_actions_init(&actions);
    if (!call_processes(argv, &actions, &pid))
        return 0;
    waitpid(pid, &child_status, 0);
    return 1;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "exit")) /* exit command */
        exit(0);
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;
    else
        return parse_operators(argv);
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

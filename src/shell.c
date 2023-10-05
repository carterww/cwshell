#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../include/job.h"
#include "../include/io.h"

#define INPUT_LENGTH 1024
#define MAX_TOKENS 100
#define MAX_COMMAND_COUNT 100

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

/* This is a list of current commands that are running
 * in the shell. Each index contains a pointer to a process
 * struct. The process is a head of a linked list of processes
 * that were executed in the same command.
 * NOTE: I will need to change this, it is too simple.
 */
process processes[MAX_COMMAND_COUNT];
int process_count = 0;

/* Initializes the shell by ensuring the shell is the
 * foreground process group of terminal. If so, it puts shell
 * in its own process group and grabs control of the terminal.
 */
void init_shell() {
    /* Get the shell's terminal file descriptor */
    shell_terminal = STDIN_FILENO;
    /* Check if the file descriptor is associated with a terminal */
    shell_is_interactive = isatty (shell_terminal);
    if (shell_is_interactive) {
        /* Loop until shell is in the foreground.
         * This checks if the current process group leader's
         * id is the same as the terminal's controling process
         * group id.
         */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) {
            /* Send SIGTTIN to the process group leader.
             * this usually means that this proces wants to be
             * in the foreground.
             */
            kill(shell_pgid, SIGTTIN); 
        }
        /* Ignore interactive and job-control signals. */
        //signal(SIGINT, SIG_IGN); /* Later change this to show history */
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        /* Put shell in its own process group. */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0)
        {
            perror("Shell could not be put into its own process group.");
            exit(1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr(shell_terminal, &shell_tmodes);
        printf("Shell initialized.\n");
    }
}

void launch_process(process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground) {
    pid_t pid;

    /* If shell isn't in foreground, then we can't
     * set the process as foreground.
     */
    if (shell_is_interactive) {
        pid = getpid();
        if (pgid == 0) pgid = pid;
        if (foreground) {
            /* Set this process as the foreground process group for the terminal. */
            tcsetpgrp(shell_terminal, pgid);
        }

        /* Children inherit the signal handlers of parent which
         * was ignore, so we need to set them back to default
         */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
    }

    /* Set the process stdin, stdout, and stderr to the specified file descriptors.
     * if necessary.
     */
    if (infile != STDIN_FILENO) {
        /* Make infile the standard input file descriptor.
         * old STDIN_FILENO is closed.
         */
        dup2(infile, STDIN_FILENO);
        close(infile);
    }
    if (outfile != STDOUT_FILENO) {
        /* Make outfile the standard output file descriptor.
         * old STDOUT_FILENO is closed.
         */
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
    }
    if (errfile != STDERR_FILENO) {
        /* Make errfile the standard error file descriptor.
         * old STDERR_FILENO is closed.
         */
        dup2(errfile, STDERR_FILENO);
        close(errfile);
    }
    /* After managing file descriptors, signals, and process groups,
     * execute the command.
     */
    execvp(p->argv[0], p->argv); /* execvp will search PATH for the command. */
    perror("execvp");
    exit(1);
}

void execute_command(process *head) {
    int i = 0;
    int foreground = 1;
    while (head->argv[i] != NULL && i < 100) {
        i++;
        printf("%s\n", head->argv[i - 1]);

    }
    if (strcmp(head->argv[i - 1], "&") == 0) {
        head->argv[i - 1] = NULL;
        foreground = 0;
    }
    processes[process_count++] = *head;

    pid_t pid;
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid > 0) {
        /* Parent */
        head->pid = pid;
        printf("%d\n", foreground);
        if (foreground) {
            /* Wait for the child to finish */
            waitpid(pid, NULL, 0);
            /* Put the shell back in the foreground */
            tcsetpgrp(shell_terminal, shell_pgid);
        }
    } else {
        /* Child */
        launch_process(head, 0, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, foreground);
    }
    return;

}

void handle_command(int argc, char *argv[]) {
    /* Main leaves argv[argc] empty so we can place null there.
     * to indicate the end of args for exec
     */
    argv[argc] = NULL;
    char *token;
    char *tokens[MAX_TOKENS];
    int i = 0;
    int j = 0;
    /* This will be the current process we are working with */
    process *curr_process = malloc(sizeof(process));
    process *prev_process = NULL;
    process *head = curr_process;

    memset(tokens, 0, MAX_TOKENS * sizeof(char *));

    while ((token = argv[i++]) != NULL) {
        if (strcmp("&", token) == 0) {
            init_process(curr_process, prev_process); /* Initialize the values */
            char **p_argv_perm = malloc(sizeof(char *) * (j + 2));
            p_argv_perm[j + 1] = NULL;
            p_argv_perm[j] = token;
            for (int k = 0; k < j; k++) {
                p_argv_perm[k] = tokens[k];
            }
            curr_process->argv = p_argv_perm;
            memset(tokens, 0, MAX_TOKENS * sizeof(char *));
            prev_process = curr_process;
            j = 0;
        } else {
            tokens[j] = token;
            j++;
        }
    }

    /* Initialize the last process */
    if (j > 0) {
        init_process(curr_process, prev_process); /* Initialize the values */
        char **p_argv_perm = malloc(sizeof(char *) * (j + 1));
        p_argv_perm[j] = NULL;
        for (int k = 0; k < j; k++) {
            p_argv_perm[k] = tokens[k];
        }
        curr_process->argv = p_argv_perm;
    }

    execute_command(head);
}

int main(int argc, char **argv, char **envp) {
    printf("%d", getpid());
    char input_line[INPUT_LENGTH];
    char *tokens[MAX_TOKENS];
    int token_count;

    init_shell();

    while (1) {
        /* Zero out memory to be safe */
        memset(input_line, 0, INPUT_LENGTH);
        memset(tokens, 0, MAX_TOKENS * sizeof(char *));
        /* Print the prompt string */
        printf("%s", get_prompt_string(NULL));
        fflush(NULL); /* Flush since we didn't print a newline */

        read(STDIN_FILENO, input_line, INPUT_LENGTH);

        tokens[0] = strtok(input_line, " \t\n");
        if (tokens[0] == NULL) {
            /* No input */
            continue;
        }
        token_count = 1;
        while (token_count < MAX_TOKENS - 1 &&
                (tokens[token_count] = strtok(NULL, " \t\n")) != NULL)
            token_count++;

        /* Execute the command */
        handle_command(token_count, tokens);
    }

    return 0;
}

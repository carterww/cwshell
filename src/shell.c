#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stddef.h>
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

char background_process_buffer[INPUT_LENGTH];

static process *head = NULL;

void sigchld_hanlder(int n) {
    if (head == NULL)
        return;
    process *p = head, *tmp = NULL;
    while (p) {
        if (p->foreground) {
            p = p->next;
            continue;
        }
        pid_t wait_result = waitpid(p->pid, &(p->status), WNOHANG);
        if (wait_result == 0) {
            p = p->next;
            continue;
        } else if (wait_result == -1) {
            perror("waitpid");
            exit(1);
        } else {
            tmp = p;
            char msg[64];
            if (WIFEXITED(p->status)) {
                sprintf(msg, "%s exited with status %d\n", p->argv[0], WEXITSTATUS(p->status));
            } else {
                sprintf(msg, "%s exited abnormally\n", p->argv[0]);
            }
            /* If we don't have the terminal then don't print to it
             * simply add it to the buffer.
             */
            size_t left = INPUT_LENGTH - strlen(background_process_buffer) - 1;
            strncat(background_process_buffer, msg, left);
            p = p->next;
            /* Free completed process */
            remove_process(&head, tmp->pid);
            free_process(tmp);
            free(tmp);
            tmp = NULL;
        }
    }
}

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
        /* So we get notified when a child process exits */
        signal(SIGCHLD, sigchld_hanlder);

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
    }
}

void execute_process(process *p) {
    pid_t pid;

    /* If shell isn't in foreground, then we can't
     * set the process as foreground.
     */
    if (shell_is_interactive) {
        pid = getpid();
        if (p->foreground) {
            /* Set this process as the foreground process group for the terminal. */
            tcsetpgrp(shell_terminal, pid);
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

    execvp(p->argv[0], p->argv); /* execvp will search PATH for the command. */
    perror("execvp");
    exit(1);
}

void launch_processes(process *local_head) {
    if (local_head == NULL)
        return;
    process *p = local_head, *tmp = NULL;
    while (p) {
        pid_t pid;
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid > 0) {
            /* Parent */
            p->pid = pid;
            /* Put the child in its own process group */
            setpgid(pid, pid);
            if (p->foreground) {
                /* Wait for the child to finish */
                waitpid(pid, &(p->status), 0);
                p->completed = 1;
                /* Put the shell back in the foreground */
                tcsetpgrp(shell_terminal, shell_pgid);
                /* Set these in case process adjusted them */
                tcsetattr(shell_terminal, TCSANOW, &shell_tmodes);
            } else {
                printf("%s [%d]\n", p->argv[0], pid);
                /* Add the process to the list of processes */
                if (head != NULL) {
                    add_process(&head, p);
                } else {
                    head = p;
                }
            }
        } else {
            /* Child */
            execute_process(p);
        }
        if (p->completed) {
            tmp = p;
            WIFEXITED(p->status) ? 
                printf("%s exited with status %d\n", p->argv[0], WEXITSTATUS(p->status)) : 
                printf("%s exited abnormally\n", p->argv[0]);
        }
        p = p->next;
        /* Free completed process */
        if (tmp != NULL) {
            free_process(tmp);
            free(tmp);
            tmp = NULL;
        }
    }
}

void handle_input(char **tokens, int token_count) {
    process *local_head = NULL;
    process *p = malloc(sizeof(process));
    init_process(p);
    p->argv = malloc(sizeof(char *) * (token_count + 1));

    char ended_on_symbol = 0;
    int i = 0, j = 0;
    do {
        /* strtok does not alloc memory, so we need to do it ourselves
         * to make sure the memory is still valid after the next main
         * loop iteration.
         */
        if (strcmp("&", tokens[i]) == 0) {
            *p->argv = realloc(*p->argv, sizeof(char *) * (j + 1));
            p->argv[j] = NULL;
            p->foreground = 0;
            ended_on_symbol |= 2;
        } else if (strcmp(";", tokens[i]) == 0) {
            /* Move all this to function since same as above */
            *p->argv = realloc(*p->argv, sizeof(char *) * (j + 1));
            p->argv[j] = NULL;
            ended_on_symbol |= 2;
        }

        if (ended_on_symbol & 2) {
            ended_on_symbol = 0;
            if (local_head != NULL) {
                add_process(&local_head, p);
            } else {
                local_head = p;
            }       
            if (i + 1 < token_count) {
                p = malloc(sizeof(process));
                init_process(p);
                p->argv = malloc(sizeof(char *) * (token_count + 1));
                j = 0;
                i++;
                continue;
            } else {
                ended_on_symbol = 1;
                break;
            }
        }

        p->argv[j] = malloc((strlen(tokens[i]) + 1));
        strcpy(p->argv[j], tokens[i]);
        i++;
        j++;
    } while (i < token_count);
    if (!ended_on_symbol) {
        p->argv = realloc(p->argv, sizeof(char *) * (j + 1));
        p->argv[j] = NULL;
    }
    if (local_head == NULL)
        local_head = p;

    launch_processes(local_head);
}

int main(int argc, char **argv, char **envp) {
    printf("%d\n", getpid());
    char input_line[INPUT_LENGTH];
    char *tokens[MAX_TOKENS];
    int token_count;

    init_shell();
    memset(background_process_buffer, 0, INPUT_LENGTH);

    while (1) {
        /* Zero out memory to be safe */
        memset(input_line, 0, INPUT_LENGTH);
        memset(tokens, 0, MAX_TOKENS * sizeof(char *));
        if (background_process_buffer[0] != '\0') {
            printf("%s", background_process_buffer);
            memset(background_process_buffer, 0, INPUT_LENGTH);
        }
        /* Print the prompt string */
        printf("%s", get_prompt_string(NULL));
        fflush(NULL); /* Flush since we didn't print a newline */

        /* Sig child may interrupt read */
        do {
            read(STDIN_FILENO, input_line, INPUT_LENGTH);
        } while (errno == EINTR);

        tokens[0] = strtok(input_line, " \t\n");
        if (tokens[0] == NULL) {
            /* No input */
            continue;
        }
        token_count = 1;
        while (token_count < MAX_TOKENS &&
                (tokens[token_count] = strtok(NULL, " \t\n")) != NULL)
            token_count++;

        handle_input(tokens, token_count);
    }

    return 0;
}

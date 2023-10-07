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
#include "../include/history.h"

/* Constants for input */
#define INPUT_LENGTH 1024
#define MAX_TOKENS 100
#define MAX_COMMAND_COUNT 10

static pid_t shell_pgid;
static struct termios shell_tmodes;
static int shell_terminal; /* File descriptor for the terminal */
static int shell_is_interactive;

/* Head of process linked list */
static process *head = NULL;

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
        signal(SIGINT, print_history); /* Later change this to show history */
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        //signal(SIGCHLD, SIG_IGN);

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

/* Executes the process by replacing the current process
 * with the process to be executed. Child calls this function.
 */
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

/* Launches the processes in the list of processes, adds background
 * processes to the global linked list
 */
void launch_processes(process **processes, int process_count) {
    if (process_count == 0)
        return;
    process *p = processes[0], *prev = NULL;
    int i = 0;
    while (i < process_count && p) {
        pid_t pid;
        /* Create a new process */
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

                /* Retrive the exit status of the process */
                WIFEXITED(p->status) ? 
                    printf("%s exited with status %d\n", p->argv[0], WEXITSTATUS(p->status)) : 
                    printf("%s exited abnormally\n", p->argv[0]);
                free_process(p);
                free(p);
                return;
            } else {
                /* Print pid of background process */
                printf("%s [%d]\n", p->argv[0], pid);
                /* Add the process to the list of processes */
                prev = p;
                p = processes[++i];
                add_process(&head, prev);
            }
        } else {
            /* Child */
            execute_process(p);
        }
    }
}

/* Parses command into processes and launches them */
void handle_input(char **tokens, int token_count) {
    int process_count = 0;
    process *processes[MAX_COMMAND_COUNT];

    /* Initialize first process */
    process *p = malloc(sizeof(process));
    init_process(p);
    p->argv = malloc(sizeof(char *) * (token_count + 1));

    /* If we end parse on symbol, we need to add last
     * process to the list of processes. We use this
     * variable to keep track of that and to handle
     * checking for next process in the loop.
     */
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

            /* Second bit indicates that we need to check
             * for next process in the loop.
             */
            ended_on_symbol |= 2;
        }   

        if (ended_on_symbol & 2) {
            ended_on_symbol = 0;
            processes[process_count++] = p;
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

    /* If we didn't end on a symbol, we need to add the last process */
    if (!ended_on_symbol) {
        p->argv = realloc(p->argv, sizeof(char *) * (j + 1));
        p->argv[j] = NULL;
        processes[process_count++] = p;
    }

    launch_processes(processes, process_count);
}

/* Mark the process as completed or stopped and return
 * 1 if completed, 2 if stopped, 0 if neither, -1 if abnormal
 * termination.
 */
int mark_process(process *p) {
    pid_t wait_result = waitpid(p->pid, &(p->status), WNOHANG);
    if (wait_result == 0) {
        return 0;
    } else if (wait_result == -1) {
        perror("waitpid");
        exit(1);
    } else {
        if (WIFEXITED(p->status)) {
            p->completed = 1;
            return 1;
        } else if (WIFSTOPPED(p->status)) {
            p->stopped = 1;
            return 2;
        } else if (WIFCONTINUED(p->status)) {
            p->stopped = 0;
            return 0;
        } else {
            p->completed = 1;
            return -1;
        }
    }
}

/* Check the status of background processes and print
 * their status if they have completed. This strategy
 * used over SIGCHLD handling because it avoids cases
 * where we would remove a process from the list of
 * while we are iterating over it.
 */
int check_background_processes() {
    int should_print = 0; /* Used to determine if we should print prompt again */
    if (head == NULL)
        return should_print;
    process *p = head, *tmp = NULL;
    while (p) {
        int process_state = mark_process(p);
        if (process_state != 0 && should_print == 0) {
            printf("\n");
            should_print = 1;
        }
        if (process_state == 0) {
            p = p->next;
            continue;
        } else if (process_state < 0) {
            printf("%s [%d] exited abnormally\n", p->argv[0], p->pid);
        } else if (process_state == 1) {
            printf("%s [%d] exited with status %d\n", p->argv[0], p->pid, WEXITSTATUS(p->status));
        } else if (process_state == 2) {
            printf("%s [%d] suspended. Send SIGCONT to continue job\n", p->argv[0], p->pid);
        }
        if (p->completed) {
            tmp = p;
            remove_process(&head, p->pid);
        }
        p = p->next;
        if (tmp != NULL) {
            free_process(tmp);
            free(tmp);
            tmp = NULL;
        }
    }
    return should_print; 
}

/* Main loop of the shell */
int main(int argc, char **argv, char **envp) {
    char input_line[INPUT_LENGTH];
    char *tokens[MAX_TOKENS];
    int token_count;
    struct timeval timeout;
    fd_set readfds;

    timeout.tv_sec = 0;
    timeout.tv_usec = 50000; /* 50 ms */
    init_shell();

    /* Main shell loop */
    while (1) {
        /* Zero out memory to be safe */
        memset(input_line, 0, INPUT_LENGTH);
        memset(tokens, 0, MAX_TOKENS * sizeof(char *));
        printf("%s", get_prompt_string(NULL));
        fflush(NULL); /* Flush since we didn't print a newline */
        int n;

        /* Wait for input */
        while (1) {
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            /* Probably could have used aio_read() here, but
             * I am more familiar with select().
             * This allows us to check for background processes
             * every 50 ms while waiting for input. May not be most
             * efficient, but it is simple and avoids cases where
             * signal handling messes with process linked list state
             * while modifying/reading it.
             */
            n = select(1, &readfds, NULL, NULL, &timeout);
            if (n == -1) {
                if (errno == EINTR) { /* Interrupted by signal */
                    errno = 0;
                    continue;
                }
                perror("select");
                exit(1);
            } else if (n == 0) {
                if (check_background_processes() != 0)
                    printf("%s", get_prompt_string(NULL));
                fflush(NULL);
                continue;
            }
            do {
                errno = 0;
                if (read(STDIN_FILENO, input_line, INPUT_LENGTH) >= 0) {
                    goto input_found;
                } 
            } while (errno == EINTR); /* Interrupted by signal, restart read */
        }
input_found:
        add_to_history(input_line); /* If this is r x, it will be overwritten */
        tokens[0] = strtok(input_line, " \t\n");
        if (tokens[0] == NULL) {
            /* No input */
            continue;
        }
        if (strcmp(tokens[0], "exit") == 0) {
            /* Exit the shell */
            exit(0);
        } else if (strcmp(tokens[0], "r") == 0) {
            /* Fetch a command from the history */
            char *cmd = fetch_command(tokens[0], tokens[1]);
            if (cmd == NULL) {
                printf("Error retrieving command.\n");
                continue;
            }
            memset(input_line, 0, INPUT_LENGTH);
            strcpy(input_line, cmd);
            goto input_found; /* Execute fetched command */
        }
        token_count = 1;
        while (token_count < MAX_TOKENS &&
                (tokens[token_count] = strtok(NULL, " \t\n")) != NULL) {
            if (tokens[token_count][0] == '\"') {
                /* Handle quoted strings */
                int start_len = strlen(tokens[token_count]);
                char *tmp = strtok(NULL, "\"");
                if (tmp == NULL) {
                    /* One word in quotes */
                    if (tokens[token_count][start_len - 1] == '\"') {
                        tokens[token_count]++;
                        tokens[token_count][start_len - 1] = '\0';
                        printf("token: %s\n", tokens[token_count]);
                        token_count++;
                        continue;
                    }
                    /* No closing quote */
                    printf("Error parsing input.\n");
                    break;
                }
                /* Multiple words in quotes */
                tokens[token_count]++;
                tokens[token_count][start_len - 1] = ' ';
            }
            token_count++;
        }

        /* Parse and execute the command */
        handle_input(tokens, token_count);
    }

    return 0;
}

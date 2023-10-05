#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

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
        signal(SIGINT, SIG_IGN); /* Later change this to show history */
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
    }
}

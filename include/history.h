#ifndef _HISTORY_H
#define _HISTORY_H

#include <stddef.h>
#include <sys/types.h>

/* Struct to hold information relating
 * to a command in the history.
 */
typedef struct {
    pid_t *pid_head;
    char *command;
    size_t command_index;
} history_info;

#define MAX_HISTORY 10

/* Add a command to the history. */
void add_to_history(char *command);

/* Print the history. */
void print_history(int status);

/* Get the command at the given index. */
char *fetch_command(char *token1, char *token2);

/* Free the history. */
void free_history(history_info *hinfo);

#endif /* _HISTORY_H */

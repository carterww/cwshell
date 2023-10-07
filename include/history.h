#ifndef _HISTORY_H
#define _HISTORY_H

#include <signal.h>
#include <stddef.h>

/* Struct to hold information relating
 * to a command in the history.
 */
typedef struct {
    pid_t *pid_head;
    char *command;
    size_t command_index;
} history_info;

#define MAX_HISTORY 10

/* Index of the next command to be inserted
 * into the history array. Needs to be modded
 * by MAX_HISTORY to keep it in the range.
 */
static size_t next_command_index = 0;

/* Array of pointers to history_info structs.
 * This is a circular array, so we need to
 * keep track of the next index to insert
 * a command into.
 */
static history_info *history[MAX_HISTORY];

/* Add a command to the history. */
void add_to_history(char *command);

/* Print the history. */
void print_history(int status);

/* Get the command at the given index. */
char *fetch_command(char *token1, char *token2);

/* Free the history. */
void free_history(history_info *hinfo);

#endif /* _HISTORY_H */

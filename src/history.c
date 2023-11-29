#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io.h"
#include "history.h"

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


/* Used to make printing history look nice */
static int get_digit_count(size_t num) {
    int count = 0;
    while (num != 0) {
        num /= 10;
        count++;
    }
    return count;
}

static int get_command_index(char *tokens[], int token_count);

/* Add a command to the history */
void add_to_history(char *command) {
    size_t insert_index = next_command_index % MAX_HISTORY;
    /* Since revolving array, free the old command being pushed out */
    if (history[insert_index] != NULL) {
        free_history(history[insert_index]);
        history[insert_index] = NULL;
    }
    history[insert_index] = malloc(sizeof(history_info));
    history[insert_index]->command = malloc(sizeof(char) * (strlen(command) + 1));
    strcpy(history[insert_index]->command, command);
    history[insert_index]->command_index = next_command_index;
    next_command_index++;
}

/* Print the history */
void print_history(int status) {
    printf("\nCommand History:\n");
    int i;
    if (next_command_index < MAX_HISTORY) {
        i = 0;
    } else {
        i = next_command_index - MAX_HISTORY;
    }
    for (int cnt = 0; cnt < MAX_HISTORY; cnt++, i++) {
        i = i % MAX_HISTORY;
        if (history[i] != NULL && history[i]->command != NULL) {
            printf("[%lu]", history[i]->command_index + 1);
            int digit_count = get_digit_count(history[i]->command_index + 1);
            /* Print spaces to make the history look nice */
            for (int j = 0; j < 3 - digit_count; j++) {
                printf(" ");
            }
            printf(" %s", history[i]->command);
        }
    }
    /* reprint prompt */
    printf("%s", get_prompt_string(NULL));
}

/* Fetch a command from the history */
char *fetch_command(char *tokens[], int token_count) {
    int command_index = -1;
    if (token_count < 2) {
        /* grab most recent command, -2 here because we already entered
         * r x into history
         */
        command_index = next_command_index - 2;
    } else {
        command_index = get_command_index(tokens, token_count);
        if (command_index == -1) {
            /* no match found */
            fprintf(stderr, "No match found for ");
            for (int i = 1; i < token_count; i++) {
                fprintf(stderr, "%s ", tokens[i]);
            }
            fprintf(stderr, "\n");
            next_command_index--; /* overwrite r x in history */
            free_history(history[next_command_index % MAX_HISTORY]);
            history[next_command_index % MAX_HISTORY] = NULL;
            return NULL;
        }
    }
    command_index = command_index % MAX_HISTORY;
    next_command_index--; /* overwrite r x in history */
    return history[command_index]->command;
}

/* Free allocated memory for and in the history_info struct */
void free_history(history_info *hinfo) {
    if (hinfo->command != NULL) {
        free(hinfo->command);
    }
    free(hinfo);
}

static int get_command_index(char *tokens[], int token_count) {
    int command_index = -1;
    int total_token_len = 0;
    fprintf(stderr, "token_count: %d\n", token_count);
    for (int i = 1; i < token_count; i++) {
        fprintf(stderr, "token: %s\n", tokens[i]);
        total_token_len += strlen(tokens[i]) + 1;
    }
    int start = next_command_index % MAX_HISTORY;
    for (int i = 0; i < MAX_HISTORY; i++) {
        int index = (start + i) % MAX_HISTORY;
        if (history[index] == NULL) continue;
        if (history[index]->command == NULL) continue;

        int command_len = strlen(history[index]->command);
        if (command_len < total_token_len) continue;
        /* Compare strings */
        int cascade_len = 0;
        for (int j = 1; j < token_count; j++) {
            int token_len = strlen(tokens[j]);
            for (int k = 0; k < token_len; k++) {
                if (tokens[j][k] == '\0')
                    goto getout;
                if (history[index]->command[k + cascade_len] != tokens[j][k])
                    goto getout;
                if (k == strlen(tokens[j]) - 1 && history[index]->command[k + cascade_len] == tokens[j][k]) {
                    /* found a match */
                    if (j == token_count - 1) command_index = history[index]->command_index;
                    break;
                }
            }
            cascade_len += token_len + 1;
        }
getout:
        if (command_index != -1) break;
    }
    fprintf(stderr, "command_index: %d\n", command_index);
    return command_index;
}

#include "../include/job.h"
#include <stdio.h>
#include <stdlib.h>

/* Init process to default state */
process *init_process(process *p) {
    p->argv = NULL;
    p->next = NULL;
    p->completed = 0;
    p->stopped = 0;
    p->status = 0;
    p->foreground = 1;
    return p;
}

/* Add a process to the linked list of processes. */
process *add_process(process **head, process *p) {
    if (*head == NULL) {
        *head = p;
        return p;
    }
    process *q;
    for (q = *head; q->next; q = q->next)
        ;
    q->next = p;
    return p;
}

/* Remove the process from the list of processes. */
process *remove_process(process **head, pid_t pid) {
    process *p, *prev = NULL;
    for (p = *head; p; p = p->next) {
        if (p->pid == pid) {
            if (prev) {
                prev->next = p->next;
            }
            else
                *head = p->next;
            return p;
        }
        prev = p;
    }
    return NULL;
}

/* Find the process with the indicated pid. */
process *find_process(process *head, pid_t pid) {
    process *p;
    for (p = head; p; p = p->next)
        if (p->pid == pid)
            return p;
    return NULL;
}

/* Free the memory associated with a process.
 * But not the pointer to the process itself
 */
void free_process(process *p) {
    if (p->argv == NULL)
        return;
    for (int i = 0; p->argv[i] != NULL; i++)
        free(p->argv[i]);
    free(p->argv);
}

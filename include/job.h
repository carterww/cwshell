#ifndef _JOB_H
#define _JOB_H

#include <unistd.h>

/* A process is a single process. */
typedef struct process {
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
  char foreground;            /* Is process in foreground */
} process;

/* Init process to default state */
process *init_process(process *p);

/* Add a process to the linked list of processes. */
process *add_process(process **head, process *p);

/* Remove the process from the list of processes. */
process *remove_process(process **head, pid_t pid);

/* Find the process with the indicated pid. */
process *find_process(process *head, pid_t pid);

/* Free the memory associated with a process.
 * But not the pointer to the process itself
 */
void free_process(process *p);

#endif /* _JOB_H */

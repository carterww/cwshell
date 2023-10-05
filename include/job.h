#ifndef _JOB_H
#define _JOB_H

#include <unistd.h>
#include <termios.h>

/* A process is a single process. */
typedef struct process {
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
} process;


/* A job is a pipeline of processes.  */
typedef struct job {
  struct job *next;           /* next active job */
  char *command;              /* command line, used for messages */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  char notified;              /* true if user told about stopped job */
  struct termios tmodes;      /* saved terminal modes */
  int stdin, stdout, stderr;  /* standard i/o channels */
} job;

/* Find the active job with the process group id
 * matching the given pgid.
 * @param head: The head of the job linked list.
 * @param pgid: The process group id to match.
 * @return: Pointer to job with matching pgid, or NULL if no match.
 */
job *job_find(job *head, pid_t pgid);

/* Checks if a job is stopped.
 * @param j: The job to check.
 * @return: 1 if all processes in the job are stopped or completed
 * 0 otherwise.
 */
int job_is_stopped(job *j);

/* Checks if a job is completed.
 * @param j: The job to check.
 * @return: 1 if all processes in the job are completed
 * 0 otherwise.
 */
int job_is_completed(job *j);

#endif /* _JOB_H */

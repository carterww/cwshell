#include "../include/job.h"

job *job_find(job *head, pid_t pgid) {
    for (job *j = head; j; j = j->next)
        if (j->pgid == pgid)
            return j;
    return NULL;    
}

int job_is_stopped(job *j) {
    for (process *p = j->first_process; p; p = p->next)
        /* If there is a single process that is still running, then the job is not done */
        if (!p->completed && !p->stopped)
            return 0;
    return 1;
}
int job_is_completed(job *j) {
    for (process *p = j->first_process; p; p = p->next)
        /* If there is a single process that has not completed, then the job is not done */
        if (!p->completed)
            return 0;
    return 1;
}

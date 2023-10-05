#ifndef _IO_H
#define _IO_H

#define PROMPT_STRING "shell> "

/* Don't want anything in here for now, but
 * could add some stuff later.
 */
typedef struct {
    
} prompt_info;

/* Could make this more complex, so using a function
 * to get the prompt string.
 */
char *get_prompt_string(prompt_info *pinfo);

#endif /* _IO_H */

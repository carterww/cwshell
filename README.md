# CS 425 Shell
This program is a simple shell that can run background and foreground processes.
## Capabilities
The shell can run multiple background processes and one foreground process. The shell expects a command that can made up of multiple background processes and an optional foreground process. Here are some examples:
1. "ls -l &"
    - Runs ls -l in the background.
2. "ls -l"
    - Runs ls -l in the foreground.
3. "ls -l & ps & cat README.md"
    - Spawns two background processes, ls -l and ps, and runs cat README.md in the foreground.
4. "ls -l & ps & cat README.md &"
    - Runs all the processes in the background.
There are a couple of reserved commands:
1. "exit"
    - Exits the shell.
2. "r x" or "r"
    - "r" executes the last command in the history.
    - "r x" executes the x command in the history. This will be discusses further in the history section.

## History
The augmentation requires the shell to have a command history feature. This feature hijacks SIGINT by changing its signal handler to print the command history of the current shell instance. The history hold the last 10 commands executed and acts as a FIFO queue when it is full. Sending SIGINT will output something like this:
[12]  ps
[13]  ls
[14]  kill -9 10130
... And so on
Running "r" will execute the last command (not printed in example) and "r 14" would run "kill -9 10130." The "r" or "r x" command will not be entered into the history, rather, its command will be.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include "LineParser.h"
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

int debug = 0; // 0 - no debug, 1 - debug mode on
void printDebug(int pid, cmdLine *pCmdLine)
{
    fprintf(stderr, "PID: %d\n", pid);
    fprintf(stderr, "Executing file name: %s\n", pCmdLine->arguments[0]);

    if (pCmdLine->blocking)
    {
        fprintf(stderr, "Foreground/Background: Foreground\n");
    }
    else
    {
        fprintf(stderr, "Foreground/Background: Background\n");
    }
}
void execute(cmdLine *pCmdLine)
{

    if (strcmp(pCmdLine->arguments[0], "cd") == 0)
    {
        if (chdir(pCmdLine->arguments[1]) == -1)
        {
            perror("cd failed");
            return;
        }
        printf("Changed directory to %s\n", pCmdLine->arguments[1]);
        return;
    }
    else if (strcmp(pCmdLine->arguments[0], "stop") == 0)
    {
        if (pCmdLine->argCount < 2)
        {
            fprintf(stderr, " missing process id\n");
            return;
        }
        int target_pid = atoi(pCmdLine->arguments[1]);

        if (kill(target_pid, SIGTSTP) == -1)
        {
            perror("stop failed");
        }
        printf("Stopped process with PID %d\n", target_pid);
        return;
    }
    else if (strcmp(pCmdLine->arguments[0], "wakeup") == 0)
    {
        if (pCmdLine->argCount < 2)
        {
            fprintf(stderr, "stop: missing process id\n");
            return;
        }
        int target_pid = atoi(pCmdLine->arguments[1]);
        if (kill(target_pid, SIGCONT) == -1)
        {
            perror("wakeup failed");
        }
        printf("Woke up process with PID %d\n", target_pid);
        return;
    }
    else if (strcmp(pCmdLine->arguments[0], "ice") == 0)
    {
        if (pCmdLine->argCount < 2)
        {
            fprintf(stderr, "missing process id\n");
            return;
        }
        int target_pid = atoi(pCmdLine->arguments[1]);
        if (kill(target_pid, SIGINT) == -1)
        {
            perror("ice failed");
        }
        printf("Iced process with PID %d\n", target_pid);
        return;
    }
    else if (strcmp(pCmdLine->arguments[0], "nuke") == 0)
    {
        if (pCmdLine->argCount < 2)
        {
            fprintf(stderr, " missing process id\n");
            return;
        }
        int target_pid = atoi(pCmdLine->arguments[1]);
        if (kill(-target_pid, SIGKILL) == -1)
        {
            perror("nuke failed");
        }
        printf("Nuked process with PID %d and all its children\n", target_pid);
        return;
    }

    int pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        return;
    }
    if (pid == 0)
    {
        // input redirection
        if (pCmdLine->inputRedirect != NULL)
        {
            int fd_in = open(pCmdLine->inputRedirect, O_RDONLY);
            if (fd_in == -1)
            {
                perror("failed to open input file");
                _exit(1);
            }
            if (dup2(fd_in, STDIN_FILENO) == -1)
            {
                perror("dup2 input failed");
                _exit(1);
            }
            close(fd_in);
        }

        // output redirction
        if (pCmdLine->outputRedirect != NULL)
        {

            int fd_out = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1)
            {
                perror("failed to open output file");
                _exit(1);
            }
            if (dup2(fd_out, STDOUT_FILENO) == -1)
            {
                perror("dup2 output failed");
                _exit(1);
            }
            close(fd_out);
        }

        // running the executa
        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments))
        { // search for the excute in the system's PATH
            perror("execv error");
            freeCmdLines(pCmdLine);
            _exit(1);
        }
    }
    else
    { // parent process
        printDebug(pid, pCmdLine);
        if (pCmdLine->blocking)
        {
            waitpid(pid, NULL, 0);
        }
    }
}

int main(int argc, char **argv)
{
    while (1)
    {
        char path[PATH_MAX];
        char input[2048];
        if (getcwd(path, PATH_MAX) == NULL)
        {
            perror("getcwd error");
            return 1;
        }
        printf("%s> ", path);
        if (fgets(input, 2048, stdin) == NULL)
        {
            perror("fgets error");
            return 1;
        }
        if (strncmp(input, "quit", 4) == 0)
        {
            break;
        }
        if (strstr(input, "-d") != NULL)
        { // need to turn on debug mode
            debug = 1;
        }
        cmdLine *cmd = parseCmdLines(input);
        if (cmd == NULL)
        {
            continue;
        }
        execute(cmd); // execute the command
        freeCmdLines(cmd);
    }
    return 0;
}
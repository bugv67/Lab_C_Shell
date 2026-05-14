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
    if (pCmdLine->next != NULL)
    {
        // handeling errors
        if (pCmdLine->outputRedirect != NULL)
        {
            fprintf(stderr, "Error: left side of pipe cannot redirect output\n");
            return;
        }
        if (pCmdLine->next->inputRedirect != NULL)
        {
            fprintf(stderr, "Error: right side of pipe cannot redirect input\n");
            return;
        }
        handlePipes(pCmdLine);
        return;
    }

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

        // running the executa !!! a regular command with no pipes !!
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

void handlePipes(cmdLine *pCmdLine)
{
    // wanted lines are chained in arg
    // need do ass support in difreent inputs or outputs
    int pipefd[2];          // opening line
    if (pipe(pipefd) == -1) // stage 1- creating a pipe
    {
        perror("pipe failed");
        return 1;
    }
    int child1, child2;

    child1 = fork(); // stage 2- forking the first child process
    if (child1 == -1)
    {
        perror("fork failed");
        return;
    }
    // child=0 , parent>0
    if (child1 == 0)
    {
        // check for redirecting in outpur or input
        //  input redirection
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
        } // no output redirect because going to the pipe directly

        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        // child 1
        close(STDOUT_FILENO); // stage 3.1 -closing stdout
        close(pipefd[0]);     // closing read end

        dup(pipefd[1]);   // stage 3.2 - duplicating the write end of the pipe to stdout
        close(pipefd[1]); // stage 3.3- closing write end after dup

        fprintf(stderr, "(child1>going to execute cmd: %s %s)\n", pCmdLine->arguments[0], pCmdLine->arguments);
        execvp(pCmdLine->arguments[0], pCmdLine->arguments); // stage 3.4 - execut  cmd1 and disappiring from the process
        exit(1);
    }

    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(pipefd[1]); // stage 4

    fprintf(stderr, "(parent_process>forking...)\n");
    child2 = fork(); // stage 5- forking the second child process
    if (child2 == -1)
    {
        perror("fork failed");
        return;
    }
    if (child2 == 0)
    { //  no input redirect - getting from the pipe

        // output redirction
        if (pCmdLine->next->outputRedirect != NULL)
        {

            int fd_out = open(pCmdLine->next->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        // child 2
        close(STDIN_FILENO); // stage 6.1-closing stdin
        close(pipefd[1]);    // closing write end

        dup(pipefd[0]);   // stage 6.2- duplicating the read end of the pipe to stdin
        close(pipefd[0]); // stage 6.3- closing read end after dup

        fprintf(stderr, "(child2>going to execute cmd: %s %s)\n", pCmdLine->next->arguments[0], pCmdLine->next->arguments);
        execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments); // stage 6.4- execut cmd2
        exit(1);
    }

    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    close(pipefd[0]); // stage 7

    // stage 8
    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(child1, NULL, 0); // stage 8
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting...)\n");
}

int main(int argc, char **argv)
{
    // need to update in order to hnadle pipes - in excute
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
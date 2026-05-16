#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include "LineParser.h"
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <bits/waitflags.h>

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
typedef struct process
{
    cmdLine *cmd;         // the parsed command line
    pid_t pid;            // the process id that is running the command
    int status;           // status of the process: RUNNING 1 /SUSPENDED 0 /TERMINATED -2
    struct process *next; // next process in chain
} process;

process *process_list = NULL; // rember to free!
int debug = 0;                // 0 - no debug, 1 - debug mode on

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
    process *new_node = (process *)malloc(sizeof(process));
    if (new_node == NULL)
    {
        perror("malloc failed");
        return;
    }

    new_node->cmd = cmd;
    new_node->pid = pid;
    new_node->status = RUNNING;

    new_node->next = *process_list;
    *process_list = new_node;
}

void printProcessList(process **process_list)
{
    updateProcessList(process_list);

    printf("PID\t\tSTATUS\t\tCommand\n");

    process *curr = *process_list;
    process *prev = NULL; // keeping track of the previous node in order to remove the terminated processes from the list

    while (curr != NULL)
    {
        // getting the statu
        char *status_str = "Unknown";
        if (curr->status == TERMINATED)
            status_str = "Terminated";
        else if (curr->status == RUNNING)
            status_str = "Running";
        else if (curr->status == SUSPENDED)
            status_str = "Suspended";

        // printing info about the process
        printf("%d\t\t%s\t\t", curr->pid, status_str);
        for (int i = 0; i < curr->cmd->argCount; i++)
        {
            printf("%s ", curr->cmd->arguments[i]);
        }
        printf("\n");

        // need to remove the terminated processes from the list
        process *next_node = curr->next;

        if (curr->status == TERMINATED)
        {
            if (prev == NULL) // last process in the list
            {
                *process_list = next_node;
            }
            else
            {
                prev->next = next_node;
            }

            freeCmdLines(curr->cmd);
            free(curr);
        }
        else
        {
            prev = curr;
        }
        curr = next_node;
    }
}

void updateProcessStatus(process *process_list, int pid, int status)
{
    process *curr = process_list;
    while (curr != NULL)
    {
        if (curr->pid == pid) // found the process
        {
            curr->status = status;
            return;
        }
        curr = curr->next;
    }
}

void updateProcessList(process **process_list)
{
    process *curr = *process_list;
    while (curr != NULL)
    {
        int status;
        // בודקים מה מצב הילד מבלי להיתקע. משלבים את הדגלים עם פעולת OR |
        int res = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if (res > 0)
        {                                                 // התהליך שינה סטטוס!
            if (WIFEXITED(status) || WIFSIGNALED(status)) // child terminated normally or by signal
            {
                updateProcessStatus(*process_list, curr->pid, TERMINATED);
            }
            else if (WIFSTOPPED(status)) // child stopped
            {
                updateProcessStatus(*process_list, curr->pid, SUSPENDED);
            }
            else if (WIFCONTINUED(status)) // child waked up
            {
                updateProcessStatus(*process_list, curr->pid, RUNNING);
            }
        }
        else if (res == -1) // error / bot found
        {
            updateProcessStatus(*process_list, curr->pid, TERMINATED);
        }
        curr = curr->next;
    }
}

void freeProcessList(process *process_list)
{
    process *curr = process_list;
    while (curr != NULL)
    {
        process *temp = curr;
        curr = curr->next;

        if (temp->cmd != NULL) // fee the cmdine
        {
            freeCmdLines(temp->cmd);
        }
        free(temp); // free the node
    }
}

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
        updateProcessStatus(process_list, target_pid, SUSPENDED);
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
        updateProcessStatus(process_list, target_pid, RUNNING);
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
        updateProcessStatus(process_list, target_pid, TERMINATED);
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
        updateProcessStatus(process_list, target_pid, TERMINATED);
        return;
    }
    else if (strcmp(pCmdLine->arguments[0], "procs") == 0) // C1
    {
        printProcessList(&process_list);
        return;
    }

    int pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        return;
    }
    if (pid == 0)
    { // child processs
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
        addProcess(&process_list, pCmdLine, pid); // adding the process to the list

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
    addProcess(&process_list, pCmdLine, child1);
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

        fprintf(stderr, "(child1>going to execute cmd: %s)\n", pCmdLine->arguments[0], pCmdLine->arguments);
        execvp(pCmdLine->arguments[0], pCmdLine->arguments); // stage 3.4 - execut  cmd1 and disappiring from the process
        exit(1);
    }

    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(pipefd[1]); // stage 4

    fprintf(stderr, "(parent_process>forking...)\n");
    child2 = fork(); // stage 5- forking the second child process
    ddProcess(&process_list, pCmdLine, child2);
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

        fprintf(stderr, "(child2>going to execute cmd: %s)\n", pCmdLine->next->arguments[0], pCmdLine->next->arguments);
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
        // freeCmdLines(cmd);   // the line is in list
        freeProcessList(process_list);
    }
    return 0;
}
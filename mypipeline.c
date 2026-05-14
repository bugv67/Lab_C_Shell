#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
    // cimulate the ps -xl | grep 5 command int terminal
    //  "printing" all the process that are running and the other is filtiring only the ones with 5

    // wanted lines
    char *cmd1[] = {"ps", "-xl", NULL};
    char *cmd2[] = {"grep", "5", NULL};

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
        return 1;
    }
    // child=0 , parent>0
    if (child1 == 0)
    {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        // child 1
        close(STDOUT_FILENO); // stage 3.1 -closing stdout
        close(pipefd[0]);     // closing read end

        dup(pipefd[1]);   // stage 3.2 - duplicating the write end of the pipe to stdout
        close(pipefd[1]); // stage 3.3- closing write end after dup

        fprintf(stderr, "(child1>going to execute cmd: %s %s)\n", cmd1[0], cmd1[1]);
        execvp(cmd1[0], cmd1); // stage 3.4 - execut  cmd1 and disappiring from the process
        exit(1);
    }

    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(pipefd[1]); // stage 4

    fprintf(stderr, "(parent_process>forking...)\n");
    child2 = fork(); // stage 5- forking the second child process
    if (child2 == -1)
    {
        perror("fork failed");
        return 1;
    }
    if (child2 == 0)
    {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        // child 2
        close(STDIN_FILENO); // stage 6.1-closing stdin
        close(pipefd[1]);    // closing write end

        dup(pipefd[0]);   // stage 6.2- duplicating the read end of the pipe to stdin
        close(pipefd[0]); // stage 6.3- closing read end after dup

        fprintf(stderr, "(child2>going to execute cmd: %s %s)\n", cmd2[0], cmd2[1]);
        execvp(cmd2[0], cmd2); // stage 6.4- execut cmd2
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
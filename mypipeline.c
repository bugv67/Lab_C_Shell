#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // wanted lines
    char *cmd1[] = {"ps", "-xl", NULL};
    char *cmd2[] = {"grep", "5", NULL};

    int pipefd[2]; // opening line
    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        return 1;
    }
    int child1, child2;

    child1 = fork();
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
        close(STDOUT_FILENO); // closing stdout
        close(pipefd[0]);     // closing read end

        dup(pipefd[1]);
        close(pipefd[1]); // closing write end after dup

        fprintf(stderr, "(child1>going to execute cmd: %s %s)\n", cmd1[0], cmd1[1]);
        execvp(cmd1[0], cmd1); // execut  cmd1 and disappiring from the process
        exit(1);               // חומת מגן: הילד לא ממשיך מפה בשום מצב
    }
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(pipefd[1]);

    fprintf(stderr, "(parent_process>forking...)\n");
    child2 = fork();
    if (child2 == -1)
    {
        perror("fork failed");
        return 1;
    }
    if (child2 == 0)
    {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        // child 2
        close(STDIN_FILENO); // closing stdin
        close(pipefd[1]);    // closing write end

        dup(pipefd[0]);
        close(pipefd[0]); // closing read end after dup

        fprintf(stderr, "(child2>going to execute cmd: %s %s)\n", cmd2[0], cmd2[1]);
        execvp(cmd2[0], cmd2); // execut cmd2
        exit(1);
    }

    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    close(pipefd[0]);

    // stage 8
    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting...)\n");
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        return 1;
    }

    int pipefd[2];
    char buffer[1024];

    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        return 1;
    }

    int pid = fork();

    if (pid == -1)
    {
        perror("fork failed");
        return 1;
    }

    // 1- write to pipe 0- read from pipe
    if (pid > 0)
    {
        // parent

        close(pipefd[0]);

        printf("Parent: Sending msg to child...\n");
        write(pipefd[1], argv[1], strlen(argv[1]));

        close(pipefd[1]);

        wait(NULL);
    }
    else
    {
        // child
        close(pipefd[1]);

        // reding to buffer
        int bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            printf("Child received msg: %s\n", buffer);
        }

        close(pipefd[0]);
        _exit(0);
    }

    return 0;
}
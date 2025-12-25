#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <positive integer>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);

    if (n <= 0)
    {
        fprintf(stderr, "Error: Please enter a positive integer.\n");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("Fork failed");
        return 1;
    }
    else if (pid == 0)
    {
        // Child process
        printf("Collatz sequence: ");

        while (n != 1)
        {
            printf("%d, ", n);

            if (n % 2 == 0)
                n = n / 2;
            else
                n = 3 * n + 1;
        }

        printf("1\n");
    }
    else
    {
        // Parent process
        wait(NULL);
        printf("Child process completed.\n");
    }

    return 0;
}

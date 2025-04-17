#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_GUESSES 100

void play_guesser(int read_fd, int write_fd, int max_number) {
    srand(time(NULL) ^ (getpid() << 16));
    int guess, response;
    int attempts = 0;

    while (1) {
        guess = rand() % max_number + 1;
        printf("Guesser: trying %d\n", guess);
        write(write_fd, &guess, sizeof(guess));
        attempts++;

        read(read_fd, &response, sizeof(response));
        if (response == 1) {
            printf("Guessed correctly in %d attempts!\n", attempts);
            break;
        }
    }
}

void play_thinker(int read_fd, int write_fd, int max_number) {
    srand(time(NULL) ^ (getpid() << 16));
    int secret = rand() % max_number + 1;
    printf("Thinker: my number is %d\n", secret);

    int guess, response;
    while (1) {
        read(read_fd, &guess, sizeof(guess));
        if (guess == secret) {
            response = 1;
            write(write_fd, &response, sizeof(response));
            break;
        } else {
            response = 0;
            write(write_fd, &response, sizeof(response));
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <max_number>\n", argv[0]);
        return 1;
    }
    int max_number = atoi(argv[1]);
    if (max_number <= 0) {
        fprintf(stderr, "Max number must be positive\n");
        return 1;
    }

    int parent_to_child[2], child_to_parent[2];
    if (pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    for (int round = 0; round < 10; round++) {
        if (pid == 0) {
            // Child process
            if (round % 2 == 0) {
                close(parent_to_child[1]);
                close(child_to_parent[0]);
                play_guesser(parent_to_child[0], child_to_parent[1], max_number);
            } else {
                close(child_to_parent[1]);
                close(parent_to_child[0]);
                play_thinker(child_to_parent[0], parent_to_child[1], max_number);
            }
        } else {
            // Parent process
            if (round % 2 == 0) {
                close(child_to_parent[1]);
                close(parent_to_child[0]);
                play_thinker(child_to_parent[0], parent_to_child[1], max_number);
            } else {
                close(parent_to_child[1]);
                close(child_to_parent[0]);
                play_guesser(parent_to_child[0], child_to_parent[1], max_number);
            }
        }
    }

    if (pid != 0) {
        close(parent_to_child[0]);
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        close(child_to_parent[1]);
        kill(pid, SIGTERM);
        wait(NULL);
    }
    return 0;
}

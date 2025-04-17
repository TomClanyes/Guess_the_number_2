#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_GUESSES 100

volatile sig_atomic_t guessed_number = 0;
volatile sig_atomic_t current_guess = 0;
volatile sig_atomic_t attempts = 0;
volatile sig_atomic_t game_over = 0;
volatile sig_atomic_t is_guesser = 0;
volatile sig_atomic_t ready = 0;

void handle_signal(int sig, siginfo_t *info, void *context) {
    if (sig == SIGUSR1) {
        game_over = 1;
        printf("Guessed correctly! Number was %d. Attempts: %d\n", current_guess, attempts);
    } else if (sig == SIGUSR2) {
        printf("Guess %d is wrong.\n", current_guess);
    } else if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
        current_guess = sig - SIGRTMIN + 1;
        attempts++;
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_signal;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    for (int i = SIGRTMIN; i <= SIGRTMAX; i++) {
        sigaction(i, &sa, NULL);
    }
}

void play_guesser(int max_number, pid_t other_pid) {
    srand(time(NULL) ^ (getpid() << 16));
    while (!game_over) {
        int guess = rand() % max_number + 1;
        printf("Guessing %d...\n", guess);
        kill(other_pid, SIGRTMIN + guess - 1);
        pause(); // Wait for response (SIGUSR1 or SIGUSR2)
    }
}

void play_thinker(int max_number, pid_t other_pid) {
    srand(time(NULL) ^ (getpid() << 16));
    guessed_number = rand() % max_number + 1;
    printf("Thinker: I'm thinking of %d (pid=%d)\n", guessed_number, getpid());
    kill(other_pid, SIGUSR1); // Notify guesser to start
    while (!game_over) {
        pause(); // Wait for guess (SIGRTMIN + guess)
        if (current_guess == guessed_number) {
            kill(other_pid, SIGUSR1); // Correct
        } else {
            kill(other_pid, SIGUSR2); // Wrong
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

    setup_signal_handlers();

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    for (int round = 0; round < 10; round++) {
        game_over = 0;
        attempts = 0;
        current_guess = 0;
        if (pid == 0) {
            // Child process
            if (round % 2 == 0) {
                is_guesser = 1;
                play_guesser(max_number, getppid());
            } else {
                is_guesser = 0;
                play_thinker(max_number, getppid());
            }
        } else {
            // Parent process
            if (round % 2 == 0) {
                is_guesser = 0;
                play_thinker(max_number, pid);
            } else {
                is_guesser = 1;
                play_guesser(max_number, pid);
            }
        }
    }

    if (pid != 0) {
        kill(pid, SIGTERM); // Terminate child
        wait(NULL);
    }
    return 0;
}

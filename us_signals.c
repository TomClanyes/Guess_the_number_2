#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>

#define MAX_ATTEMPTS 100
#define GAME_CYCLES 10
#define TIMEOUT_SEC 5

typedef struct {
    int number;
    int guess;
    int attempts;
    bool is_correct;
    bool waiting_for_response;
} GameData;

volatile sig_atomic_t game_over = 0;
volatile GameData game;

void flush_output() {
    fflush(stdout);
}

void alarm_handler(int sig) {
    (void)sig;
    game_over = 1;
    printf("\nTimeout reached. Ending game.\n");
    flush_output();
}

void thinker_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
        game.guess = info->si_value.sival_int;
        game.attempts++;
        
        if (game.guess == game.number) {
            game.is_correct = true;
            printf("Thinker: Correct! %d in %d attempts\n", game.guess, game.attempts);
        } else {
            game.is_correct = false;
            printf("Thinker: Wrong guess %d (attempt %d)\n", game.guess, game.attempts);
        }
        flush_output();
        
        //send response
        union sigval value;
        value.sival_int = game.is_correct;
        sigqueue(info->si_pid, SIGUSR1, value);
    }
}

void guesser_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig == SIGUSR1) {
        game.is_correct = info->si_value.sival_int;
        game.waiting_for_response = false;
        
        if (game.is_correct) {
            printf("Guesser: Correct! Number was %d (attempts: %d)\n", 
                  game.guess, game.attempts);
        } else {
            printf("Guesser: Wrong! Attempt %d: %d\n", game.attempts, game.guess);
        }
        flush_output();
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <N>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "N must be positive\n");
        return 1;
    }

    //initialize game data
    game.number = 0;
    game.guess = 0;
    game.attempts = 0;
    game.is_correct = false;
    game.waiting_for_response = false;

    //set alarm handler
    signal(SIGALRM, alarm_handler);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    srand(time(NULL) ^ (getpid() << 16));

    if (pid == 0) {
        //child process - guesser
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = guesser_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, NULL);

        printf("Child process started as guesser (pid: %d)\n", getpid());
        flush_output();

        int cycles = 0;
        while (cycles < GAME_CYCLES && !game_over) {
            alarm(TIMEOUT_SEC);

            //generate guess
            game.guess = rand() % N + 1;
            game.attempts++;
            printf("Guesser: Attempt %d - guessing %d\n", game.attempts, game.guess);
            flush_output();

            //send guess to parent
            union sigval value;
            value.sival_int = game.guess;
            game.waiting_for_response = true;
            if (sigqueue(getppid(), SIGRTMIN, value) == -1) {
                perror("sigqueue");
                break;
            }

            //wait for response
            while (game.waiting_for_response && !game_over) {
                pause();
            }
            
            if (game.is_correct) {
                game.attempts = 0;
                cycles++;
                //switch roles after correct guess
                printf("\n--- Switching roles ---\n");
            }
            
            alarm(0);
        }
        
        printf("Child process exiting\n");
        flush_output();
    } 
    else {
        //parent process - thinker
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = thinker_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGRTMIN, &sa, NULL);

        printf("Parent process started as thinker (pid: %d)\n", getpid());
        flush_output();

        int cycles = 0;
        while (cycles < GAME_CYCLES && !game_over) {
            alarm(TIMEOUT_SEC);

            //generate number to guess
            game.number = rand() % N + 1;
            game.attempts = 0;
            game.is_correct = false;
            printf("Thinker: New number is %d\n", game.number);
            flush_output();

            //wait for correct guess
            while (!game_over && !game.is_correct) {
                pause();
            }
            
            if (game.is_correct) {
                cycles++;
            }
            
            alarm(0);
        }

        //END
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, 0);
        printf("Game finished after %d rounds\n", cycles);
        flush_output();
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#define FIFO_NAME1 "/tmp/guess_number_fifo1"
#define FIFO_NAME2 "/tmp/guess_number_fifo2"
#define MAX_ATTEMPTS 100
#define GAME_CYCLES 10
#define BUFFER_SIZE 128

volatile sig_atomic_t game_over = 0;

void sigint_handler(int sig) {
    (void)sig;
    game_over = 1;
}

void cleanup() {
    unlink(FIFO_NAME1);
    unlink(FIFO_NAME2);
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
    
    signal(SIGINT, sigint_handler);
    atexit(cleanup);
    
    //creating named channels
    mkfifo(FIFO_NAME1, 0666);
    mkfifo(FIFO_NAME2, 0666);
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    
    srand(time(NULL) ^ (getpid() << 16));
    
    int fd_read, fd_write;
    if (pid == 0) {
        //child process(start with guesser)
        fd_read = open(FIFO_NAME1, O_RDONLY);
        fd_write = open(FIFO_NAME2, O_WRONLY);
        printf("Child process started as guesser (pid: %d)\n", getpid());
    } else {
        //parent process
        fd_write = open(FIFO_NAME1, O_WRONLY);
        fd_read = open(FIFO_NAME2, O_RDONLY);
        printf("Parent process started as thinker (pid: %d)\n", getpid());
    }
    
    int cycles = 0;
    int is_guesser = (pid == 0); // 1 - guessr, 0 - thinker

    while (cycles < GAME_CYCLES && !game_over) {
        if (!is_guesser) {
            //mod thinker
            int number = rand() % N + 1;
            printf("Thinker (pid %d): I'm thinking of %d\n", getpid(), number);
            
            //send number for guesser
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "THINK %d", number);
            write(fd_write, buffer, strlen(buffer)+1);
            
            //accept and check
            int attempts = 0;
            char response[BUFFER_SIZE];
            while (!game_over) {
                read(fd_read, response, BUFFER_SIZE);
                
                if (strncmp(response, "GUESS", 5) == 0) {
                    int guess;
                    sscanf(response, "GUESS %d", &guess);
                    attempts++;
                    
                    if (guess == number) {
                        printf("Thinker: Correct guess %d in %d attempts!\n", guess, attempts);
                        snprintf(buffer, BUFFER_SIZE, "RESULT CORRECT %d", attempts);
                        write(fd_write, buffer, strlen(buffer)+1);
                        break;
                    } else {
                        printf("Thinker: Wrong guess %d (attempt %d)\n", guess, attempts);
                        snprintf(buffer, BUFFER_SIZE, "RESULT WRONG");
                        write(fd_write, buffer, strlen(buffer)+1);
                    }
                }
            }
            is_guesser = 1; //change rols
        } else {
            char message[BUFFER_SIZE];
            read(fd_read, message, BUFFER_SIZE);
            
            if (strncmp(message, "THINK", 5) == 0) {
                int number;
                sscanf(message, "THINK %d", &number);
                int attempts = 0;
                
                while (!game_over) {
                    attempts++;
                    int guess = rand() % N + 1;
                    printf("Guesser (pid %d): Attempt %d - guessing %d\n", 
                          getpid(), attempts, guess);
                    
                    //send guess
                    char buffer[BUFFER_SIZE];
                    snprintf(buffer, BUFFER_SIZE, "GUESS %d", guess);
                    write(fd_write, buffer, strlen(buffer)+1);
                    
                    //get a response
                    read(fd_read, message, BUFFER_SIZE);
                    if (strncmp(message, "RESULT CORRECT", 14) == 0) {
                        int used_attempts;
                        sscanf(message, "RESULT CORRECT %d", &used_attempts);
                        printf("Guesser: Correct! Number was %d (attempts: %d)\n",
                              number, used_attempts);
                        break;
                    } else if (strncmp(message, "RESULT WRONG", 12) == 0) {
                        continue;
                    }
                }
                is_guesser = 0; //change rols
            }
        }
        //retry for win
        cycles++;
        printf("--- Completed round %d ---\n", cycles);
    }
    
    //End
    close(fd_read);
    close(fd_write);
    
    if (pid != 0) {
        int status;
        waitpid(pid, &status, 0);
        printf("Game finished after %d rounds\n", cycles);
    }
    
    return 0;
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

volatile sig_atomic_t guessed = 0;
int secret_number = 0;
int max_number = 100;
int round_number = 0;
pid_t child_pid;

void handle_guess(int sig, siginfo_t *info, void *context) {
    int guess = info->si_value.sival_int;
    if (guess == secret_number) {
        kill(info->si_pid, SIGUSR1); // Угадал
        guessed = 1;
    } else {
        kill(info->si_pid, SIGUSR2); // Не угадал
    }
}

void setup_guess_handler() {
    struct sigaction sa;
    sa.sa_sigaction = handle_guess;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);
}

void play_parent(int rounds, int max) {
    srand(time(NULL) ^ getpid());
    setup_guess_handler();

    for (int i = 1; i <= rounds; i++) {
        round_number = i;
        secret_number = 1 + rand() % max;
        guessed = 0;

        printf("[Родитель] Загадал число для раунда %d\n", i);
        kill(child_pid, SIGUSR1); // Старт раунда

        while (!guessed) {
            pause();
        }

        // Ждём подтверждение от ребёнка
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGUSR2);
        siginfo_t si;
        sigwaitinfo(&mask, &si);
    }

    printf("[Родитель] Игра завершена.\n");
    kill(child_pid, SIGTERM);
    wait(NULL);
}

void child_round(int max, sigset_t mask) {
    int *numbers = malloc(sizeof(int) * max);
    for (int i = 0; i < max; i++) {
        numbers[i] = i + 1;
    }

    int size = max;
    int attempts = 0;

    while (size > 0) {
        int index = rand() % size;
        int guess = numbers[index];
        attempts++;

        printf("[Дочерний] Пробую: %d\n", guess);

        union sigval val;
        val.sival_int = guess;
        sigqueue(getppid(), SIGRTMIN, val);

        siginfo_t si;
        sigwaitinfo(&mask, &si);
        if (si.si_signo == SIGUSR1) {
            printf("[Дочерний] Угадал число %d за %d попыток\n", guess, attempts);
            kill(getppid(), SIGUSR2); // Подтверждение
            break;
        } else {
            numbers[index] = numbers[size - 1];
            size--;
        }
    }

    free(numbers);
}

void play_child(int max) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);

    srand(time(NULL) ^ getpid());

    while (1) {
        siginfo_t si;
        sigwaitinfo(&mask, &si); // Ожидаем SIGUSR1 (старт раунда)
        child_round(max, mask);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2 || (max_number = atoi(argv[1])) <= 0) {
        fprintf(stderr, "Usage: %s <max_number>\n", argv[0]);
        return 1;
    }

    sigset_t block_all;
    sigemptyset(&block_all);
    sigaddset(&block_all, SIGUSR1);
    sigaddset(&block_all, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block_all, NULL);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        play_child(max_number);
    } else {
        child_pid = pid;
        play_parent(10, max_number);
    }

    return 0;
}

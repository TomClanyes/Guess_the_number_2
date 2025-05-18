#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

#define ROUNDS 10
#define MAX_N 100

pid_t other_pid;
int target_number = 0;
int attempts = 0;
int received_response = 0;
int is_child = 0;
int game_over = 0;
int current_round = 0;
int N = MAX_N;

// Перемешивание массива (Фишер-Йетс)
void shuffle(int *array, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

// Обработка сигнала "угадал"
void handle_sigusr1(int sig) {
    printf("[Игрок %d] Угадал число за %d попыток!\n", is_child, attempts);
    received_response = 1;
    game_over = 1;
}

// Обработка сигнала "не угадал"
void handle_sigusr2(int sig) {
    received_response = 1;
}

// Обработка сигнала от другого процесса (угадывание)
void handle_guess(int sig, siginfo_t *info, void *context) {
    int guessed = info->si_value.sival_int;
    attempts++;
    printf("[Родитель] Получил попытку: %d (цель: %d)\n", guessed, target_number);

    if (guessed == target_number) {
        kill(other_pid, SIGUSR1);
    } else {
        kill(other_pid, SIGUSR2);
    }
}

// Ожидание ответа от родителя
void wait_for_response() {
    while (!received_response) {
        pause();
    }
    received_response = 0;
}

// Родитель загадывает число
void parent_round() {
    target_number = (rand() % N) + 1;
    attempts = 0;
    game_over = 0;

    printf("\n[Родитель] Загадал число от 1 до %d\n", N);

    while (!game_over) {
        pause();
    }
}

// Дочерний процесс угадывает в случайном порядке без повторений
void child_round() {
    int *numbers = malloc(sizeof(int) * N);
    for (int i = 0; i < N; ++i) {
        numbers[i] = i + 1;
    }
    shuffle(numbers, N);

    attempts = 0;
    game_over = 0;

    for (int i = 0; i < N && !game_over; ++i) {
        int guess = numbers[i];
        printf("[Дочерний] Пробую: %d\n", guess);

        // Используем sigqueue для отправки числа
        union sigval value;
        value.sival_int = guess;
        sigqueue(other_pid, SIGRTMIN, value);

        wait_for_response();
    }

    free(numbers);
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        N = atoi(argv[1]);
        if (N < 1 || N > MAX_N) {
            fprintf(stderr, "Число должно быть от 1 до %d\n", MAX_N);
            exit(EXIT_FAILURE);
        }
    }

    srand(time(NULL));

    // Установка обработчиков сигналов SIGRTMIN
    struct sigaction sa_guess = {0};
    sa_guess.sa_sigaction = handle_guess;
    sa_guess.sa_flags = SA_SIGINFO;

    // Устанавливаем обработчики для всех сигналов от SIGRTMIN
    sigaction(SIGRTMIN, &sa_guess, NULL);

    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        is_child = 1;
        other_pid = getppid();
    } else {
        is_child = 0;
        other_pid = pid;
    }

    while (1) {
        for (current_round = 1; current_round <= ROUNDS; ++current_round) {
            printf("\n===== Раунд %d =====\n", current_round);

            if (is_child) {
                child_round();
            } else {
                parent_round();
            }

            // Меняем роли
            is_child = !is_child;
            pid_t temp = other_pid;
            other_pid = getpid();
            if (is_child) {
                other_pid = temp;
            }

            sleep(1);  // для читаемости вывода
        }

        if (!is_child) {
            kill(other_pid, SIGTERM);
            wait(NULL);
        }
    }

    return 0;
}


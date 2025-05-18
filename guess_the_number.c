#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define FIFO_REQ "guess_fifo"
#define FIFO_RES "result_fifo"
#define ROUNDS 10
#define MAX_N 100

// Очистка FIFO при выходе
void cleanup() {
    unlink(FIFO_REQ);
    unlink(FIFO_RES);
}

// Алгоритм Фишера-Йетса — перемешивание массива
void shuffle(int *array, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

// Родитель загадывает, читает попытки из FIFO и отвечает
void parent_round(int N, int fd_req, int fd_res) {
    int number = rand() % N + 1;
    int guess = 0;
    int attempts = 0;

    printf("\n[Родитель] Загадал число от 1 до %d\n", N);

    while (1) {
        read(fd_req, &guess, sizeof(int));
        attempts++;

        if (guess == number) {
            int response = 1;
            write(fd_res, &response, sizeof(int));
            printf("[Родитель] Угадано! Попыток: %d\n", attempts);
            break;
        } else {
            int response = 0;
            write(fd_res, &response, sizeof(int));
        }
    }
}

// Дочерний процесс угадывает случайными уникальными числами
void child_round(int N, int fd_req, int fd_res) {
    int *numbers = malloc(sizeof(int) * N);
    for (int i = 0; i < N; i++) {
        numbers[i] = i + 1;
    }

    shuffle(numbers, N);

    int response = 0;
    int attempts = 0;

    for (int i = 0; i < N; i++) {
        int guess = numbers[i];
        printf("[Дочерний] Пробую: %d\n", guess);
        write(fd_req, &guess, sizeof(int));
        read(fd_res, &response, sizeof(int));
        attempts++;

        if (response == 1) {
            printf("[Дочерний] Угадал число %d за %d попыток!\n", guess, attempts);
            break;
        }
    }

    free(numbers);
}

int main(int argc, char *argv[]) {
    int N = MAX_N;
    if (argc == 2) {
        N = atoi(argv[1]);
        if (N < 1 || N > MAX_N) {
            fprintf(stderr, "Число должно быть от 1 до %d\n", MAX_N);
            exit(EXIT_FAILURE);
        }
    }

    srand(time(NULL));

    // Создание FIFO
    atexit(cleanup);
    mkfifo(FIFO_REQ, 0666);
    mkfifo(FIFO_RES, 0666);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Дочерний процесс: угадывает
        int fd_req = open(FIFO_REQ, O_WRONLY);
        int fd_res = open(FIFO_RES, O_RDONLY);

        for (int i = 0; i < ROUNDS; i++) {
            child_round(N, fd_req, fd_res);
            sleep(1); // Пауза для читаемости вывода (опционально)
        }

        close(fd_req);
        close(fd_res);
        exit(0);
    } else {
        // Родительский процесс: загадывает
        int fd_req = open(FIFO_REQ, O_RDONLY);
        int fd_res = open(FIFO_RES, O_WRONLY);

        for (int i = 0; i < ROUNDS; i++) {
            parent_round(N, fd_req, fd_res);
        }

        close(fd_req);
        close(fd_res);
        wait(NULL); // Ждём завершения дочернего процесса
    }

    return 0;
}

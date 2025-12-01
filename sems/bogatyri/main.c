#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#define N (128)
#define BUFFER_SIZE (1024)

struct monitor_t {
    pthread_mutex_t mtx;
    pthread_cond_t  cond;       //

    size_t      last_assigned;
    size_t      thread_count;

    char        current_char;
    size_t      str_next_ind;
    char        string_buffer[BUFFER_SIZE];
    size_t      string_length;
};

void monitor_init(struct monitor_t* monitor) {
    assert(monitor);

    pthread_mutex_init(&(monitor->mtx), NULL);
    pthread_cond_init(&(monitor->cond), NULL);
    monitor->last_assigned = -1;
    monitor->thread_count = 0;

    monitor->current_char = 0;
    monitor->str_next_ind = 0;
    monitor->string_length = 0;
    monitor->string_buffer[0] = '\0';
}

void monitor_destroy(struct monitor_t* monitor) {
    assert(monitor);
    pthread_cond_destroy(&(monitor->cond));
    pthread_mutex_destroy(&(monitor->mtx));
}


void molvit(char symbol) {
    printf("%c", symbol);
}

int read_next_char(struct monitor_t* monitor) {
    assert(monitor);
    // +1 потому что '\0' используется как терминальный символ
    if (monitor->str_next_ind < monitor->string_length + 1) {
        monitor->current_char = (monitor->string_buffer)[monitor->str_next_ind];
        (monitor->str_next_ind)++;
        return monitor->current_char;
    } else {
        printf("\nIndex out of range for string: %ld\n", monitor->str_next_ind);
        monitor->current_char = '\0';
        return -1;
    }
}

// Богатырь получает могучий номер
size_t acquire_thread_number(struct monitor_t* monitor) {
    pthread_mutex_lock(&(monitor->mtx));
    size_t my_number = ++(monitor->last_assigned);
    monitor->thread_count++;

    // Сигнализируем, если все потоки созданы
    if (monitor->thread_count == N) {
        // Разбудить братьев по оружию
        pthread_cond_broadcast(&(monitor->cond));
        read_next_char(monitor);
    }
    pthread_mutex_unlock(&(monitor->mtx));
    return my_number;
}

void* bogatyr(void* arg) {
    struct monitor_t* monitor = (struct monitor_t*)arg;

    // Получаем номер потока
    int my_number = acquire_thread_number(monitor);

    // Ждем, пока все потоки получат номера
    pthread_mutex_lock(&(monitor->mtx));

    // ожидание в цикле - POSIX не гарантирует отсутствие ложных пробуждений
    while (monitor->thread_count < N) {
        pthread_cond_wait(&(monitor->cond), &(monitor->mtx));
    }
    pthread_mutex_unlock(&(monitor->mtx));

    while (1) {
        pthread_mutex_lock(&(monitor->mtx));

        // условие завершения - терминальный символ строки
        if (monitor->current_char == '\0') {
            pthread_cond_broadcast(&(monitor->cond));   // объявить, что песня закончилась
            pthread_mutex_unlock(&(monitor->mtx));
            break;
        }
        // проверяем наша ли очередь
        if (monitor->current_char != (char)my_number) {
            // не наша очередь -> спать до следующей буквы
            pthread_cond_wait(&(monitor->cond), &(monitor->mtx));
            pthread_mutex_unlock(&(monitor->mtx));

        } else {
            molvit((char)my_number);
            read_next_char(monitor);

            pthread_cond_broadcast(&(monitor->cond));   // всем объявить о новой букве
            pthread_mutex_unlock(&(monitor->mtx));
        }
    }

    return NULL;
}

int main() {
    struct monitor_t monitor;
    monitor_init(&monitor);

    printf("Enter string: ");
    if (fgets(monitor.string_buffer, BUFFER_SIZE, stdin) == NULL) {
        fprintf(stderr, "Error reading input\n");
        monitor_destroy(&monitor);
        return 1;
    }

    size_t len = strlen(monitor.string_buffer);
    monitor.string_length = len;

    printf("Processing string: %s (length: %zu)\n",
           monitor.string_buffer, monitor.string_length);
    printf("Creating %d threads...\n", N);

    pthread_t threads[N];
    for (size_t i = 0; i < N; i++) {
        if (pthread_create(&threads[i], NULL, bogatyr, &monitor) != 0) {
            perror("Failed to create thread");

            // В случае ошибки будим ожидающие потоки
            pthread_mutex_lock(&(monitor.mtx));
            monitor.thread_count = N;  // Помечаем как "все потоки созданы"
            pthread_cond_broadcast(&(monitor.cond));
            pthread_mutex_unlock(&(monitor.mtx));

            // Ждем уже созданные потоки
            for (size_t j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }

            monitor_destroy(&monitor);
            return 1;
        }
    }

    // Ждем завершения всех потоков
    for (size_t i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nAll characters processed.\n");
    monitor_destroy(&monitor);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>

// M должно быть кратно N
#define M (16)
#define N (4)


struct monitor_t {
    pthread_mutex_t mtx;
    int* array;
};

struct thread_arg_t {
    size_t index;
    struct monitor_t* monitor;
};

void monitor_init(struct monitor_t* monitor) {
    assert(monitor);

    pthread_mutex_init(&(monitor->mtx), NULL);
    // pthread_cond_init(&(monitor->cond), NULL);
}

void monitor_destroy(struct monitor_t* monitor) {
    assert(monitor);
    // pthread_cond_destroy(&(monitor->cond));
    pthread_mutex_destroy(&(monitor->mtx));
}

int comparator(const void* a, const void* b) {
    return ( (*(int*)a) - (*(int*)b) );
}

void print_int_array(int* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");
}

void print_size_t_array(size_t* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%ld ", array[i]);
    }
    printf("\n");
}

void sort_subarray(void* arg) {
    assert(arg);
    struct thread_arg_t thread_arg = *(struct thread_arg_t*)arg;

    size_t i = thread_arg.index;
    struct monitor_t* monitor = thread_arg.monitor;

    size_t size     = (M / N);
    size_t from_ind = i * (M / N);

    int* src = monitor->array + from_ind;
    int local_array[size] = {};
    pthread_mutex_lock(&(monitor->mtx));            // делаем lock и копируем часть массива, которую сортируем
    memcpy(local_array, src, size * sizeof(int));

    printf("before sorting\n");
    print_int_array(local_array, size);

    pthread_mutex_unlock(&(monitor->mtx));          // освобождаем доступ к монитору, переходим к сортировке


    qsort(local_array, size, sizeof(int), &comparator);


    pthread_mutex_lock(&(monitor->mtx));            // lock монитора, записываем отсортированную часть
    printf("after sorting\n");
    print_int_array(local_array, size);
    printf("=========================================\n");
    memcpy(src, local_array, size * sizeof(int));
    pthread_mutex_unlock(&(monitor->mtx));
}

size_t find_min_in_current_slice(int* array, size_t indices[]) {
    assert(array);

    size_t target_ind = 0;
    int min_val = INT_MAX;
    bool found = false;

    for (size_t i = 0; i < N; i++) {
        if (indices[i] < (M / N)) {
            int temp = array[(M / N) * i + indices[i]];
            if (temp < min_val) {
                min_val = temp;
                target_ind = i;
                found = true;
            }
        }
    }

    if (!found) return N;  // Все фрагменты закончились
    return target_ind;
}

int* merge(int* array) {
    assert(array);

    size_t indices[N] = {0};
    int* new_array = (int*)calloc(M, sizeof(int));

    for (size_t i = 0; i < M; i++) {
        size_t min_ind = find_min_in_current_slice(array, indices);
        if (min_ind >= N) break;  // Все фрагменты обработаны

        new_array[i] = array[(M / N) * min_ind + indices[min_ind]];
        indices[min_ind]++;
    }

    return new_array;
}

bool check_sorting(int* array, const size_t size) {
    int array_cp[size] = {};
    for (size_t i = 0; i < size; i++) {
        array_cp[i] = array[i];
    }

    qsort(array_cp, size, sizeof(int), comparator);

    size_t mismatch_count = 0;
    for (size_t i = 0; i < size; i++) {
        if (array_cp[i] != array[i]) {
            printf("Mismatch: array_cp[%ld] = %d != %d = array[%ld]\n", i, array_cp[i], array[i], i);
            ++mismatch_count;
        }
    }

    return mismatch_count > 0;
}

int main() {

    // возможно придется динамически выделять ???
    int array[M] = {8, 9, 3, 2, 0, 1, 4, 5, 7, 6, 10, 15, 13, 12, 11, 14};
    pthread_t threads[N];
    struct monitor_t monitor;

    monitor.array = array;
    monitor_init(&monitor);

    struct thread_arg_t thread_data[N] = {};
    for (size_t i = 0; i < N; i++) {
        thread_data[i].index = i;
        thread_data[i].monitor = &monitor;
    }
    // сначала потоки частично сортируют массив
    for (size_t i = 0; i < N; i++) {

        if (pthread_create(&threads[i], NULL, sort_subarray, &(thread_data[i])) != 0) {
            perror("Failed to create thread");

            for (size_t j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }

            monitor_destroy(&monitor);
            return 1;
        }
    }

    // ждем пока потоки завершат работу
    for (size_t i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    // финальная стадия сортировки - слияние
    int* new_array = merge(monitor.array);

    // проверка сортировки
    if (check_sorting(new_array, M)) {
        printf("Sorting failed\n");
        return false;
    } else {
        printf("Sorting succesful\n");
        return true;
    }

    if (new_array != NULL) {
        free(new_array);
    }
    monitor_destroy(&monitor);
    return 0;
}

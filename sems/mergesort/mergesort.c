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
#define N (5)

#define DEBUG

#ifdef DEBUG
#define DBG_PRINT(...) printf(__VA_ARGS__);
#else
#define DBG_PRINT(...)
#endif

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
}

void monitor_destroy(struct monitor_t* monitor) {
    assert(monitor);
    pthread_mutex_destroy(&(monitor->mtx));
}

int comparator(const void* a, const void* b) {
    return ( (*(int*)a) - (*(int*)b) );
}

void print_int_array(int* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        DBG_PRINT("%d ", array[i])
    }
    DBG_PRINT("\n")
}

void print_size_t_array(size_t* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        DBG_PRINT("%ld ", array[i])
    }
    DBG_PRINT("\n")
}

size_t min_size_t(size_t a, size_t b) {
    size_t res = (a < b) ? a : b;
    return res;
}

size_t max_size_t(size_t a, size_t b) {
    size_t res = (a < b) ? b : a;
    return res;
}

void sort_subarray(void* arg) {
    assert(arg);
    struct thread_arg_t thread_arg = *(struct thread_arg_t*)arg;

    size_t i = thread_arg.index;
    struct monitor_t* monitor = thread_arg.monitor;

    size_t size      = (M / N);
    size_t start_ind = i * (M / N);
    size_t end_ind   = min_size_t(start_ind + size, M);
    size = end_ind - start_ind;

    // последнему может достаться больше !!!

    int* local_array = monitor->array + start_ind;

    DBG_PRINT("before sorting\n")
    print_int_array(local_array, size);

    qsort(local_array, size, sizeof(int), &comparator);

    DBG_PRINT("after sorting\n")
    print_int_array(local_array, size);
    DBG_PRINT("=========================================\n")
}

size_t find_min_in_current_slice(int* array, size_t indices[], size_t ends[]) {
    assert(array);

    size_t target_ind = 0;
    int min_val = INT_MAX;
    bool found = false;

    DBG_PRINT("find min in: ");
    for (size_t i = 0; i < N; i++) {
        DBG_PRINT("%d(%ld) ", array[indices[i]], indices[i]);
    }
    DBG_PRINT("\n");

    for (size_t i = 0; i < N; i++) {
        // проверка что еще не вышли из отсортированного фрагмента
        if (indices[i] < ends[i]) {
            int temp = array[indices[i]];
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

    size_t indices[N] = {0}; // массив индексов на элементы в отсортированных фрагментах, по одному на фрагмент
    for (size_t i = 0; i < N; i++) {
        indices[i] = i * (M / N);
    }
    size_t ends[N] = {0};
    for (size_t i = 0; i < N; i++) {
        ends[i] = (i+1) * (M / N);
    }
    ends[N-1] = max_size_t(ends[N-1], M);

    DBG_PRINT("ends indices array:\n");
    print_size_t_array(ends, N);

    int* new_array = (int*)malloc(M * sizeof(int));

    for (size_t i = 0; i < M; i++) {
        // выбираем минимальный элемент из текущих
        size_t min_ind = find_min_in_current_slice(array, indices, ends);
        if (min_ind >= N) break;  // Все фрагменты обработаны

        DBG_PRINT("found min: %d(%ld)\n", array[indices[min_ind]], indices[min_ind]);
        new_array[i] = array[indices[min_ind]];
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

    int array[M] = {8, 9, 3, 2, 0, 1, 4, 5, 7, 6, 10, 15, 13, 12, 11, 14};
    pthread_t threads[N];
    struct monitor_t monitor;

    monitor.array = array;
    monitor_init(&monitor);

    // подготовим данные для потоков
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

    DBG_PRINT("before merge:\n");
    print_int_array(monitor.array, M);
    // финальная стадия сортировки - слияние
    int* new_array = merge(monitor.array);

    DBG_PRINT("after merge:\n");
    // проверка сортировки
    if (check_sorting(new_array, M)) {
        print_int_array(new_array, M);
        printf("Sorting failed\n");
        return false;
    } else {
        print_int_array(new_array, M);
        printf("Sorting succesful\n");
        return true;
    }

    if (new_array != NULL) {
        free(new_array);
    }
    monitor_destroy(&monitor);
    return 0;
}

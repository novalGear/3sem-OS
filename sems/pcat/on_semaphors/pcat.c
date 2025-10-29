#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#ifdef DEBUG
#define DBG_PRINT(...)                                      \
    printf("pid %d:%s:%d: ", getpid(), __func__, __LINE__); \
    printf(__VA_ARGS__);

#else
#define DBG_PRINT(...)
#endif

typedef struct {
    sem_t full;
    sem_t empty;
    sem_t mutex;
} semaphores_t;

typedef struct {
    int start;
    int end;
    unsigned long size;
    unsigned long capacity;
    char* storage;
} buffer_t;

buffer_t* shared_buffer = NULL;
semaphores_t* shared_sems = NULL;
char* shared_storage = NULL;
int shm_fd_buffer = -1;
int shm_fd_storage = -1;
int shm_fd_sems = -1;

ssize_t safewrite(int fd, const void* buffer, size_t size) {
    assert(buffer);
    ssize_t written_total = 0;
    while ((size_t)written_total < size) {
        ssize_t written_on_write = write(fd, (const char*)buffer + written_total, size - written_total);
        if (written_on_write < 0) {
            if (errno == EINTR) continue;
            perror("Error in safe write");
            return written_on_write;
        }
        written_total += written_on_write;
    }
    return written_total;
}

ssize_t saferead(int fd, void* buffer, size_t size) {
    assert(buffer);
    ssize_t read_total = 0;
    while ((size_t)read_total < size) {
        ssize_t read_on_read = read(fd, (char*)buffer + read_total, size - read_total);
        if (read_on_read < 0) {
            if (errno == EINTR) continue;
            perror("Error in safe read");
            return read_on_read;
        }
        if (read_on_read == 0) {
            break;
        }
        read_total += read_on_read;
    }
    return read_total;
}

void upd_buffer_size(buffer_t* buffer) {
    assert(buffer);
    if (buffer->end >= buffer->start) {
        buffer->size = buffer->end - buffer->start;
    } else {
        buffer->size = buffer->capacity - buffer->start + buffer->end;
    }
}

void shift_start_by_k(buffer_t* buffer, unsigned long n) {
    buffer->start = (buffer->start + n) % buffer->capacity;
}

void shift_end_by_k(buffer_t* buffer, unsigned long n) {
    buffer->end = (buffer->end + n) % buffer->capacity;
}

const char* get_read(buffer_t* buffer, semaphores_t* sems, unsigned long n) {
    sem_wait(&sems->mutex);
    DBG_PRINT("size: %ld\n", buffer->size);
    while (buffer->size < n) {
        sem_post(&sems->mutex);
        sem_wait(&sems->empty);
        sem_wait(&sems->mutex);
        DBG_PRINT("size: %ld\n", buffer->size);
    }
    const char* res = buffer->storage + buffer->start;
    sem_post(&sems->mutex);
    return res;
}

int complete_read(buffer_t* buffer, semaphores_t* sems, int dest_fd, const char* src, unsigned long n) {
    DBG_PRINT(".\n");
    safewrite(dest_fd, src, n);

    sem_wait(&sems->mutex);
    shift_start_by_k(buffer, n);
    upd_buffer_size(buffer);
    DBG_PRINT("size: %ld\n", buffer->size);
    sem_post(&sems->full);
    sem_post(&sems->mutex);

    return n;
}

int buf_read(buffer_t* buffer, semaphores_t* sems, int fd, unsigned long n) {
    DBG_PRINT(".\n");
    const char* src = get_read(buffer, sems, n);
    complete_read(buffer, sems, fd, src, n);
    return 0;
}

char* get_write(buffer_t* buffer, semaphores_t* sems, unsigned long n) {
    sem_wait(&sems->mutex);
    DBG_PRINT("size: %ld\n", buffer->size);
    while (buffer->capacity - buffer->size < n) {
        sem_post(&sems->mutex);
        sem_wait(&sems->full);
        sem_wait(&sems->mutex);
        DBG_PRINT("size: %ld\n", buffer->size);
    }
    char* res = buffer->storage + buffer->end;
    sem_post(&sems->mutex);
    return res;
}

int complete_write(buffer_t* buffer, semaphores_t* sems, int src_fd, char* dest, unsigned long n) {
    DBG_PRINT(".\n");
    saferead(src_fd, dest, n);

    sem_wait(&sems->mutex);
    shift_end_by_k(buffer, n);
    upd_buffer_size(buffer);
    DBG_PRINT("size: %ld\n", buffer->size);
    sem_post(&sems->empty);
    sem_post(&sems->mutex);

    return n;
}

int buf_write(buffer_t* buffer, semaphores_t* sems, int fd, unsigned long n) {
    DBG_PRINT(".\n");
    char* dest = get_write(buffer, sems, n);
    complete_write(buffer, sems, fd, dest, n);
    return 0;
}

off_t get_filesize(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return st.st_size;
}

int init_shared_memory(unsigned long capacity, const char* name_buffer, const char* name_storage, const char* name_sems) {
    size_t buffer_size = sizeof(buffer_t);
    size_t storage_size = capacity * sizeof(char);
    size_t sems_size = sizeof(semaphores_t);

    shm_fd_buffer = shm_open(name_buffer, O_CREAT | O_RDWR, S_IRWXU);
    if (shm_fd_buffer == -1) {
        perror("shm_open buffer");
        return -1;
    }
    if (ftruncate(shm_fd_buffer, buffer_size) == -1) {
        perror("ftruncate buffer");
        close(shm_fd_buffer);
        return -1;
    }
    shared_buffer = (buffer_t*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_buffer, 0);
    if (shared_buffer == MAP_FAILED) {
        perror("mmap buffer");
        close(shm_fd_buffer);
        return -1;
    }

    shm_fd_storage = shm_open(name_storage, O_CREAT | O_RDWR, S_IRWXU);
    if (shm_fd_storage == -1) {
        perror("shm_open storage");
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }
    if (ftruncate(shm_fd_storage, storage_size) == -1) {
        perror("ftruncate storage");
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }
    shared_storage = (char*)mmap(NULL, storage_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_storage, 0);
    if (shared_storage == MAP_FAILED) {
        perror("mmap storage");
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }

    shm_fd_sems = shm_open(name_sems, O_CREAT | O_RDWR, S_IRWXU);
    if (shm_fd_sems == -1) {
        perror("shm_open sems");
        munmap(shared_storage, storage_size);
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }
    if (ftruncate(shm_fd_sems, sems_size) == -1) {
        perror("ftruncate sems");
        close(shm_fd_sems);
        munmap(shared_storage, storage_size);
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }
    shared_sems = (semaphores_t*)mmap(NULL, sems_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_sems, 0);
    if (shared_sems == MAP_FAILED) {
        perror("mmap sems");
        close(shm_fd_sems);
        munmap(shared_storage, storage_size);
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }

    if (sem_init(&shared_sems->full, 1, shared_buffer->capacity) == -1) {
        perror("sem_init full");
        munmap(shared_sems, sems_size);
        close(shm_fd_sems);
        munmap(shared_storage, storage_size);
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }
    if (sem_init(&shared_sems->empty, 1, 0) == -1) {
        perror("sem_init empty");
        sem_destroy(&shared_sems->full);
        munmap(shared_sems, sems_size);
        close(shm_fd_sems);
        munmap(shared_storage, storage_size);
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }
    if (sem_init(&shared_sems->mutex, 1, 1) == -1) {
        perror("sem_init mutex");
        sem_destroy(&shared_sems->full);
        sem_destroy(&shared_sems->empty);
        munmap(shared_sems, sems_size);
        close(shm_fd_sems);
        munmap(shared_storage, storage_size);
        close(shm_fd_storage);
        munmap(shared_buffer, buffer_size);
        close(shm_fd_buffer);
        return -1;
    }

    shared_buffer->storage = shared_storage;
    shared_buffer->capacity = capacity;
    shared_buffer->size = 0;
    shared_buffer->start = 0;
    shared_buffer->end = 0;

    return 0;
}

void cleanup_shared_memory(unsigned long capacity, const char* name_buffer, const char* name_storage, const char* name_sems) {
    if (shared_sems) {
        sem_destroy(&shared_sems->full);
        sem_destroy(&shared_sems->empty);
        sem_destroy(&shared_sems->mutex);
        munmap(shared_sems, sizeof(semaphores_t));
    }
    if (shared_storage) {
        munmap(shared_storage, capacity * sizeof(char));
    }
    if (shared_buffer) {
        munmap(shared_buffer, sizeof(buffer_t));
    }
    if (shm_fd_sems != -1) {
        close(shm_fd_sems);
        shm_unlink(name_sems);
    }
    if (shm_fd_storage != -1) {
        close(shm_fd_storage);
        shm_unlink(name_storage);
    }
    if (shm_fd_buffer != -1) {
        close(shm_fd_buffer);
        shm_unlink(name_buffer);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const unsigned long BUFFER_CAPACITY = 4096;
    char name_buffer[256], name_storage[256], name_sems[256];
    snprintf(name_buffer, sizeof(name_buffer), "/buf_%d", getpid());
    snprintf(name_storage, sizeof(name_storage), "/stor_%d", getpid());
    snprintf(name_sems, sizeof(name_sems), "/sems_%d", getpid());

    if (init_shared_memory(BUFFER_CAPACITY, name_buffer, name_storage, name_sems) == -1) {
        fprintf(stderr, "Failed to initialize shared memory.\n");
        exit(EXIT_FAILURE);
    }

    int* fd_array = malloc(argc * sizeof(int));
    off_t* fsize_array = malloc(argc * sizeof(off_t));
    if (!fd_array || !fsize_array) {
        perror("malloc");
        cleanup_shared_memory(BUFFER_CAPACITY, name_buffer, name_storage, name_sems);
        free(fd_array);
        free(fsize_array);
        exit(EXIT_FAILURE);
    }

    for (int file_ind = 1; file_ind < argc; file_ind++) {
        fd_array[file_ind] = open(argv[file_ind], O_RDONLY);
        if (fd_array[file_ind] < 0) {
            perror("Error while opening file in O_RDONLY mode");
            for (int j = 1; j < file_ind; j++) {
                close(fd_array[j]);
            }
            free(fd_array);
            free(fsize_array);
            cleanup_shared_memory(BUFFER_CAPACITY, name_buffer, name_storage, name_sems);
            exit(EXIT_FAILURE);
        }
        fsize_array[file_ind] = get_filesize(fd_array[file_ind]);
        DBG_PRINT("File %s size: %ld\n", argv[file_ind], fsize_array[file_ind]);
    }

    pid_t writer_pid = fork();
    if (writer_pid == 0) {
        DBG_PRINT("Writer process started.\n");
        for (int file_ind = 1; file_ind < argc; file_ind++) {
            buf_write(shared_buffer, shared_sems, fd_array[file_ind], fsize_array[file_ind]);
        }
        DBG_PRINT("Writer process finished.\n");
        for (int file_ind = 1; file_ind < argc; file_ind++) {
            close(fd_array[file_ind]);
        }
        free(fd_array);
        free(fsize_array);
        _exit(0);
    }

    pid_t reader_pid = fork();
    if (reader_pid == 0) {
        DBG_PRINT("Reader process started.\n");
        for (int file_ind = 1; file_ind < argc; file_ind++) {
            buf_read(shared_buffer, shared_sems, STDOUT_FILENO, fsize_array[file_ind]);
        }
        DBG_PRINT("Reader process finished.\n");
        for (int file_ind = 1; file_ind < argc; file_ind++) {
            close(fd_array[file_ind]);
        }
        free(fd_array);
        free(fsize_array);
        _exit(0);
    }

    int status;
    pid_t wpid;
    while ((wpid = wait(&status)) > 0) {
        if (wpid == writer_pid) {
            DBG_PRINT("Writer process %d finished with status %d.\n", wpid, status);
        } else if (wpid == reader_pid) {
            DBG_PRINT("Reader process %d finished with status %d.\n", wpid, status);
        }
    }

    for (int file_ind = 1; file_ind < argc; file_ind++) {
        close(fd_array[file_ind]);
    }
    free(fd_array);
    free(fsize_array);

    cleanup_shared_memory(BUFFER_CAPACITY, name_buffer, name_storage, name_sems);

    return 0;
}

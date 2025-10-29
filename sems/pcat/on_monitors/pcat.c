#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/mman.h>

#ifdef DEBUG
#define DBG_PRINT(...)                                      \
printf("pid %d:%s:%d: ", getpid(), __func__, __LINE__);      \
printf(__VA_ARGS__)

#else

#define DBG_PRINT(...)

#endif

struct monitor_t {
    pthread_mutex_t mtx;
    pthread_cond_t full;
    pthread_cond_t empty;
};

struct buffer_t {
    int start;
    int end;
    unsigned long size;
    unsigned long capacity;

    char* storage;
};


ssize_t safewrite(int fd, const void* buffer, size_t size) {
    assert(buffer);

    ssize_t written_total = 0;
    while ((size_t)written_total < size) {
        ssize_t written_on_write = write(fd, buffer + written_total, size - written_total);
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
        ssize_t read_on_read = read(fd, buffer + read_total, size - read_total);
        if (read_on_read < 0) {
            if (errno == EINTR) continue;
            perror("Error in safe read");
            return read_on_read;
        }
        if (read_on_read == 0) {  // EOF
            break;
        }
        read_total += read_on_read;
    }
    return read_total;
}

void upd_buffer_size(struct buffer_t* buffer) {
    assert(buffer);
    if (buffer->end >= buffer->start) {
        buffer->size = buffer->end - buffer->start;
    } else {
        buffer->size = buffer->capacity - buffer->start + buffer->end;
    }
}

void monitor_init(struct monitor_t* monitor) {
    assert(monitor);

    pthread_mutex_init(&(monitor->mtx), NULL);
    pthread_cond_init(&(monitor->full), NULL);
    pthread_cond_init(&(monitor->empty), NULL);
}

void monitor_destroy(struct monitor_t* monitor) {
    assert(monitor);

    pthread_cond_destroy(&(monitor->full));
    pthread_cond_destroy(&(monitor->empty));
    pthread_mutex_destroy(&(monitor->mtx));
}

void buffer_init(struct buffer_t* buffer, unsigned long capacity) {
    assert(buffer);

    buffer->storage = (char*)calloc(capacity, sizeof(char));
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->start = 0;
    buffer->end = 0;
}

void buffer_destroy(struct buffer_t* buffer) {
    assert(buffer);
    assert(buffer->storage);

    free(buffer->storage);
}

void shift_start_by_k(struct buffer_t* buffer, unsigned long n) {
    buffer->start = (buffer->start + n) % buffer->capacity;
}

void shift_end_by_k(struct buffer_t* buffer, unsigned long n) {
    buffer->end = (buffer->end + n) % buffer->capacity;
}

//

const char* get_read(struct buffer_t* buffer, struct monitor_t* monitor, unsigned long n) {
    pthread_mutex_lock(&(monitor->mtx));

    DBG_PRINT(".\n");
    // upd_buffer_size(buffer);
    DBG_PRINT("size: %ld\n", buffer->size);
    while (buffer -> size < n) {
        pthread_cond_wait(&(monitor->empty), &(monitor->mtx));
        DBG_PRINT("size: %ld\n", buffer->size);
    }

    pthread_mutex_unlock(&(monitor->mtx));
    return buffer->storage + buffer->start;
}

int complete_read(  struct buffer_t* buffer, struct monitor_t* monitor,
                    int dest_fd, const char* src, unsigned long n) {
    DBG_PRINT(".\n");
    safewrite(dest_fd, src, n);

    pthread_mutex_lock(&(monitor->mtx));
    shift_start_by_k(buffer, n);                    // теперь помечаем как прочитанное
    upd_buffer_size(buffer);
    DBG_PRINT("size: %ld\n", buffer->size);
    pthread_cond_signal(&(monitor->full));          // зовем писателей
    pthread_mutex_unlock(&(monitor->mtx));

    return n;
}

int buf_read(   struct buffer_t* buffer, struct monitor_t* monitor,
                int fd, unsigned long n) {
    DBG_PRINT(".\n");
    const char* src = get_read(buffer, monitor, n);
    complete_read(buffer, monitor, fd, src, n);
    return 0;
}


char* get_write(struct buffer_t* buffer, struct monitor_t* monitor, unsigned long n) {
    pthread_mutex_lock(&(monitor->mtx));

    // upd_buffer_size(buffer);
    DBG_PRINT("size: %ld\n", buffer->size);
    while (buffer -> capacity - buffer -> size < n) {
        pthread_cond_wait(&(monitor->full), &(monitor->mtx));
        DBG_PRINT("size: %ld\n", buffer->size);
    }

    pthread_mutex_unlock(&(monitor->mtx));
    return buffer->storage + buffer->end;
}

int complete_write( struct buffer_t* buffer, struct monitor_t* monitor,
                    int src_fd, char* dest, unsigned long n) {
    DBG_PRINT(".\n");
    saferead(src_fd, dest, n);

    pthread_mutex_lock(&(monitor->mtx));
    shift_end_by_k(buffer, n);                      // теперь помечаем как записанное
    upd_buffer_size(buffer);
    DBG_PRINT("size: %ld\n", buffer->size);
    pthread_cond_signal(&(monitor->empty));         // зовем читателей
    pthread_mutex_unlock(&(monitor->mtx));

    return n;
}

int buf_write(  struct buffer_t* buffer, struct monitor_t* monitor,
                int fd, unsigned long n) {
    DBG_PRINT(".\n");
    char* dest = get_write(buffer, monitor, n);
    complete_write(buffer, monitor, fd, dest, n);
    return 0;
}

off_t get_filesize(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        assert(0);
        return -1;
    }

    return st.st_size;
};

struct writer_args {
    struct buffer_t* buffer;
    struct monitor_t* monitor;
    int* fd_array;
    off_t* fsize_array;
    int argc;
};

struct reader_args {
    struct buffer_t* buffer;
    struct monitor_t* monitor;
    off_t* fsize_array;
    int argc;
};

void* writer_thread(void* arg) {
    struct writer_args* args = (struct writer_args*)arg;

    for (int file_ind = 1; file_ind < args->argc; file_ind++) {
        buf_write(args->buffer, args->monitor, args->fd_array[file_ind], args->fsize_array[file_ind]);
    }

    return NULL;
}

void* reader_thread(void* arg) {
    struct reader_args* args = (struct reader_args*)arg;

    for (int file_ind = 1; file_ind < args->argc; file_ind++) {
        buf_read(args->buffer, args->monitor, STDOUT_FILENO, args->fsize_array[file_ind]);
    }

    return NULL;
}

int main(int argc, char* argv[]) {

    const unsigned long BUFFER_CAPACITY = 4096;
    struct monitor_t monitor;
    struct buffer_t  buffer;

    monitor_init(&monitor);
    buffer_init(&buffer, BUFFER_CAPACITY);

    // делаем массив файловых дескрипторов и их размеров
    int fd_array[argc] = {};
    off_t fsize_array[argc] = {};

    for (int file_ind=1; file_ind<argc; file_ind++) {
        int fd = open(argv[file_ind], O_RDONLY);
        if (fd < 0) {
            perror("Error while opening file in O_RDONLY mode: ");
            assert(0);
        }
        off_t fsize = get_filesize(fd);

        fd_array[file_ind] = fd;
        fsize_array[file_ind] = fsize;
    }

    struct writer_args w_args = {
        .buffer = &buffer,
        .monitor = &monitor,
        .fd_array = fd_array,
        .fsize_array = fsize_array,
        .argc = argc
    };

    struct reader_args r_args = {
        .buffer = &buffer,
        .monitor = &monitor,
        .fsize_array = fsize_array,
        .argc = argc
    };

    pthread_t writer_tid, reader_tid;

    pthread_create(&writer_tid, NULL, writer_thread, &w_args);
    pthread_create(&reader_tid, NULL, reader_thread, &r_args);

    pthread_join(writer_tid, NULL);
    pthread_join(reader_tid, NULL);


    for (int file_ind=1; file_ind<argc; file_ind++) {
        close(fd_array[file_ind]);
    }

    buffer_destroy(&buffer);
    monitor_destroy(&monitor);
}

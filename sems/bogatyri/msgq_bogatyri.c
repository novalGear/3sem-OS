#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <time.h>
#include <errno.h>

#define MAX_PROCESSES 128
#define MAX_STRING_LEN 1024
#define REGISTRY_FILE "/tmp/rank_registry.txt"
#define LOCK_FILE "/tmp/rank.lock"
#define MSG_QUEUE_KEY 0x12345678

// #define DEBUG

#ifdef DEBUG
#define DBG_PRINT(...) \
    fprintf(stderr, "[DEBUG PID=%d] ", getpid());   \
    fprintf(stderr, __VA_ARGS__);

#else
#define DBG_PRINT(...)
#endif

typedef struct {
    long mtype;           // Тип сообщения = ранг получателя + 1
    int current_index;    // Текущий индекс символа в строке (-1 для EOF)
} RankMessage;

void register_pid_in_file(pid_t my_pid) {
    int lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
    if (lock_fd < 0) {
        perror("open lock file");
        exit(1);
    }

    DBG_PRINT("Waiting for file lock...\n");
    flock(lock_fd, LOCK_EX);
    DBG_PRINT("File lock acquired\n");

    FILE *registry = fopen(REGISTRY_FILE, "a");
    if (!registry) {
        perror("fopen registry");
        exit(1);
    }
    fprintf(registry, "%d\n", my_pid);
    fclose(registry);

    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    DBG_PRINT("PID %d written to registry\n", my_pid);
}

void wait_for_all_registrations(int total_processes) {
    int registered = 0;
    int wait_cycles = 0;

    while (registered < total_processes) {
        FILE *f = fopen(REGISTRY_FILE, "r");
        if (f) {
            int lines = 0;
            char ch;
            while ((ch = fgetc(f)) != EOF) {
                if (ch == '\n') lines++;
            }
            fclose(f);
            registered = lines;
        }

        if (wait_cycles % 100 == 0) {
            DBG_PRINT("Waiting for processes: %d/%d registered\n",
                     registered, total_processes);
        }

        usleep(10000);
        wait_cycles++;
    }

    DBG_PRINT("All %d processes registered\n", total_processes);
}

int read_all_pids_from_file(pid_t pids[], int max_pids) {
    FILE *f = fopen(REGISTRY_FILE, "r");
    if (!f) {
        perror("fopen registry for read");
        exit(1);
    }

    int count = 0;
    while (fscanf(f, "%d", &pids[count]) == 1 && count < max_pids) {
        DBG_PRINT("Read PID: %d\n", pids[count]);
        count++;
    }
    fclose(f);

    return count;
}

void sort_pids(pid_t pids[], int count) {
    DBG_PRINT("Sorting %d PIDs...\n", count);
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (pids[i] > pids[j]) {
                pid_t temp = pids[i];
                pids[i] = pids[j];
                pids[j] = temp;
            }
        }
    }
}

int find_my_rank_in_sorted_pids(pid_t pids[], int count, pid_t my_pid) {
    for (int i = 0; i < count; i++) {
        if (pids[i] == my_pid) {
            DBG_PRINT("My rank is %d (out of %d processes)\n", i, count);
            return i;
        }
    }

    fprintf(stderr, "PID %d not found in registry!\n", my_pid);
    exit(1);
}

int get_my_rank(pid_t my_pid, int total_processes) {
    DBG_PRINT("Starting rank calculation for PID %d\n", my_pid);

    register_pid_in_file(my_pid);
    wait_for_all_registrations(total_processes);

    pid_t pids[MAX_PROCESSES];
    int count = read_all_pids_from_file(pids, MAX_PROCESSES);

    if (count != total_processes) {
        fprintf(stderr, "Expected %d processes, but found %d\n", total_processes, count);
        exit(1);
    }
    sort_pids(pids, count);
    return find_my_rank_in_sorted_pids(pids, count, my_pid);
}

void send_message_to_process(int msgqid, int target_rank, int current_index) {
    RankMessage msg;
    msg.mtype = target_rank + 1;
    msg.current_index = current_index;

    DBG_PRINT("Sending to rank %d (mtype: %d), index: %d\n",
             target_rank, target_rank + 1, current_index);

    if (msgsnd(msgqid, &msg, sizeof(RankMessage) - sizeof(long), 0) < 0) {
        perror("msgsnd to process");
    }
}

int receive_message_for_me(int msgqid, int my_rank) {
    RankMessage msg;
    DBG_PRINT("Waiting for message (mtype: %d)...\n", my_rank + 1);

    if (msgrcv(msgqid, &msg, sizeof(RankMessage) - sizeof(long),
               my_rank + 1, 0) < 0) {
        perror("msgrcv");
        exit(1);
    }
    DBG_PRINT("Received message with current_index: %d\n", msg.current_index);
    return msg.current_index;
}

void broadcast_eof_to_all(int msgqid, int my_rank, int total_processes) {
    DBG_PRINT("Sending EOF to all other processes\n");

    for (int i = 0; i < total_processes; i++) {
        if (i != my_rank) {
            RankMessage eof_msg;
            eof_msg.mtype = i + 1;
            eof_msg.current_index = -1;
            if (msgsnd(msgqid, &eof_msg, sizeof(RankMessage) - sizeof(long), IPC_NOWAIT) < 0) {
                if (errno != EAGAIN) {
                    perror("msgsnd EOF");
                }
            }
        }
    }
}

int process_my_characters_sequence(const char *input_str, int *current_index, int str_len, int my_rank) {
    int processed = 0;

    while (*current_index < str_len) {
        char current_char = input_str[*current_index];
        int target_rank = (unsigned char)current_char;

        if (target_rank == my_rank) {
            DBG_PRINT("Processing my character: '%c'\n", current_char);
            printf("%c", current_char);
            fflush(stdout);

            (*current_index)++;
            processed++;
        } else {
            break;
        }
    }

    return processed;
}

void process_my_single_character(const char *input_str, int current_index, int my_rank) {
    char current_char = input_str[current_index];
    DBG_PRINT("Processing my character: '%c'\n", current_char);
    printf("%c", current_char);
    // fflush(stdout);
}

void process_string_loop(int msgqid, const char *input_str, int str_len, int my_rank, int total_processes) {
    int current_index = 0;
    int processed_by_me = 0;

    DBG_PRINT("Starting processing loop. String length: %d\n", str_len);

    while (current_index < str_len) {
        char current_char = input_str[current_index];
        int target_rank = (unsigned char)current_char;

        DBG_PRINT("Index: %d, char: '%c' (ASCII: %d), target: %d, my rank: %d\n",
                 current_index, current_char, (unsigned char)current_char,
                 target_rank, my_rank);

        if (target_rank == my_rank) {
            // обрабатываем все, что можем
            int newly_processed = process_my_characters_sequence(input_str, &current_index, str_len, my_rank);
            processed_by_me += newly_processed;

            if (current_index >= str_len) {
                DBG_PRINT("String ended while processing my characters\n");
                break;
            }

            // Отправляем сообщение следующему процессу
            char next_char = input_str[current_index];
            int next_target = (unsigned char)next_char;
            send_message_to_process(msgqid, next_target, current_index);
        } else {
            int received_index = receive_message_for_me(msgqid, my_rank);
            if (received_index == -1) {
                DBG_PRINT("Received EOF signal\n");
                break;
            }

            current_index = received_index;
        }
    }

    DBG_PRINT("Processed %d characters total\n", processed_by_me);
}

void processing_phase(int my_rank, const char *input_str, int total_processes) {
    int msgqid = msgget(MSG_QUEUE_KEY, 0666);
    if (msgqid < 0) {
        perror("msgget");
        exit(1);
    }

    int str_len = strlen(input_str);
    pid_t my_pid = getpid();

    DBG_PRINT("Starting processing phase. String length: %d\n", str_len);

    if (str_len == 0) {
        DBG_PRINT("Empty string, nothing to process\n");
        broadcast_eof_to_all(msgqid, my_rank, total_processes);
        return;
    }

    DBG_PRINT("First character: '%c' (value: %d)\n",
              input_str[0], (unsigned char)input_str[0]);

    process_string_loop(msgqid, input_str, str_len, my_rank, total_processes);
    broadcast_eof_to_all(msgqid, my_rank, total_processes);

    DBG_PRINT("Processing phase complete\n");
}

void create_child_process(int child_index, const char *input_str, int total_processes,
                          pid_t children[]) {
    pid_t pid = fork();

    if (pid == 0) {
        pid_t my_pid = getpid();
        DBG_PRINT("Child process %d started (PID: %d)\n", child_index, my_pid);

        int my_rank = get_my_rank(my_pid, total_processes);
        processing_phase(my_rank, input_str, total_processes);
        exit(0);

    } else if (pid > 0) {
        children[child_index] = pid;
        DBG_PRINT("Parent: Started child %d with PID %d\n", child_index, pid);
    } else {
        perror("fork");
        exit(1);
    }
}

void launch_all_child_processes(int total, const char *input_str, pid_t children[]) {
    DBG_PRINT("Launching %d child processes...\n", total);

    for (int i = 0; i < total; i++) {
        create_child_process(i, input_str, total, children);
        // usleep(1000);
    }
}


void wait_for_all_children(int total, pid_t children[]) {
    DBG_PRINT("Waiting for all children to finish...\n");

    for (int i = 0; i < total; i++) {
        int status;
        waitpid(children[i], &status, 0);
    }
}

void initialize_system_resources() {

    int msgqid = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);
    if (msgqid < 0) {
        perror("msgget create");
        exit(1);
    }

    unlink(REGISTRY_FILE);
    FILE *f = fopen(REGISTRY_FILE, "w");
    if (f) fclose(f);
}

void cleanup_system_resources() {
    int msgqid = msgget(MSG_QUEUE_KEY, 0666);
    if (msgqid >= 0) {
        msgctl(msgqid, IPC_RMID, NULL);
    }

    unlink(REGISTRY_FILE);
    unlink(LOCK_FILE);
}

int main(int argc, char *argv[]) {

    const int total_processes = MAX_PROCESSES;
    char input_str[MAX_STRING_LEN];
    // printf("Enter string to process: ");

    if (fgets(input_str, MAX_STRING_LEN, stdin) == NULL) {
        perror("fgets");
        exit(1);
    }

    DBG_PRINT("Main: Starting with string: \"%s\" (length: %zu)\n",
             input_str, strlen(input_str));

    pid_t children[MAX_PROCESSES];
    initialize_system_resources();
    launch_all_child_processes(total_processes, input_str, children);
    wait_for_all_children(total_processes, children);
    cleanup_system_resources();

    return 0;
}

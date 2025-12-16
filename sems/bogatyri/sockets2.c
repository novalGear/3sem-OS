#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>

#define N 16
#define MAX_MSG_SIZE 1024
#define BASE_PORT 56000
#define BROADCAST_PORT 55555
#define BROADCAST_ADDR "255.255.255.255"

#ifdef DEBUG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

pid_t my_pid;
int my_rank = -1;
pid_t all_pids[N];
int pids_count = 0;
int active = 0;
int current_index = 0;
int str_len = 0;
char input_string[MAX_MSG_SIZE];
int server_fd = -1;

int compare_pids(const void *a, const void *b) {
    return (*(pid_t*)b - *(pid_t*)a);
}

void exchange_pids(int index) {
    int opt = 1;
    // Процесс 0 будет координатором
    if (index == 0) {
        int coord_sock = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(coord_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(BASE_PORT + 999);

        bind(coord_sock, (struct sockaddr*)&addr, sizeof(addr));
        listen(coord_sock, N);

        DBG_PRINT("Process 0: waiting for PIDs on port %d\n", BASE_PORT + 999);

        all_pids[0] = my_pid;
        pids_count = 1;

        // Ждем подключений от всех процессов
        for (int i = 1; i < N; i++) {
            int client = accept(coord_sock, NULL, NULL);
            if (client >= 0) {
                pid_t pid;
                recv(client, &pid, sizeof(pid), 0);
                all_pids[pids_count++] = pid;
                DBG_PRINT("Process 0: got PID %d from process %d\n", pid, i);
                close(client);
            }
        }

        close(coord_sock);

        qsort(all_pids, pids_count, sizeof(pid_t), compare_pids);

        // Находим свой rank
        for (int i = 0; i < pids_count; i++) {
            if (all_pids[i] == my_pid) {
                my_rank = i;
                break;
            }
        }

        DBG_PRINT("Process 0: I'm rank %d, have %d PIDs\n", my_rank, pids_count);

        // Рассылаем массив PID'ов всем
        for (int i = 1; i < pids_count; i++) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in target_addr;
            memset(&target_addr, 0, sizeof(target_addr));
            target_addr.sin_family = AF_INET;
            target_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            target_addr.sin_port = htons(BASE_PORT + 1000 + i);

            for (int attempt = 0; attempt < 5; attempt++) {
                if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) == 0) {
                    send(sock, &pids_count, sizeof(pids_count), 0);
                    send(sock, all_pids, pids_count * sizeof(pid_t), 0);
                    close(sock);
                    break;
                }
                usleep(100000);
            }
        }

    } else {
        // Остальные процессы отправляют свой PID process 0
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        server_addr.sin_port = htons(BASE_PORT + 999);

        // Подключаемся к process 0
        for (int attempt = 0; attempt < 10; attempt++) {
            if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
                send(sock, &my_pid, sizeof(my_pid), 0);
                close(sock);
                break;
            }
            usleep(100000);
        }

        // Создаем сервер для получения массива PID'ов
        int recv_sock = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(recv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in recv_addr;
        memset(&recv_addr, 0, sizeof(recv_addr));
        recv_addr.sin_family = AF_INET;
        recv_addr.sin_addr.s_addr = INADDR_ANY;
        recv_addr.sin_port = htons(BASE_PORT + 1000 + index);

        bind(recv_sock, (struct sockaddr*)&recv_addr, sizeof(recv_addr));
        listen(recv_sock, 1);

        // Ждем данные от process 0
        int client = accept(recv_sock, NULL, NULL);
        if (client >= 0) {
            recv(client, &pids_count, sizeof(pids_count), 0);
            recv(client, all_pids, pids_count * sizeof(pid_t), 0);
            close(client);
        }

        close(recv_sock);

        // Находим свой rank
        for (int i = 0; i < pids_count; i++) {
            if (all_pids[i] == my_pid) {
                my_rank = i;
                break;
            }
        }

        DBG_PRINT("Process %d: I'm rank %d (from %d PIDs)\n", index, my_rank, pids_count);
    }
}

int create_tcp_server(int rank) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(BASE_PORT + rank);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    if (listen(sock, N) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

int connect_to_process(int rank) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(BASE_PORT + rank);

    for (int attempt = 0; attempt < 5; attempt++) {
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            return sock;
        }
        usleep(100000);
    }

    close(sock);
    return -1;
}

void process_function(int index, char *str) {
    my_pid = getpid();
    strcpy(input_string, str);
    str_len = strlen(str);

    DBG_PRINT("\n=== Process %d: PID %d ===\n", index, my_pid);

    exchange_pids(index);

    server_fd = create_tcp_server(my_rank);
    if (server_fd < 0) {
        fprintf(stderr, "Process %d: failed to create server\n", index);
        exit(EXIT_FAILURE);
    }

    DBG_PRINT("Process %d: server ready on port %d\n", index, BASE_PORT + my_rank);

    sleep(2);

    if (my_rank == 0) {
        active = 1;
        current_index = 0;
        DBG_PRINT("Process %d (rank 0): ACTIVE, starting\n", index);
    } else {
        active = 0;
        DBG_PRINT("Process %d (rank %d): INACTIVE\n", index, my_rank);
    }

    int eof_received = 0;

    while (!eof_received) {
        if (active) {

            if (current_index >= str_len) {
                // Конец строки
                DBG_PRINT("Process %d: EOF reached\n", index);

                // Отправляем EOF всем через TCP
                for (int i = 0; i < pids_count; i++) {
                    if (i == my_rank) continue;

                    int sock = connect_to_process(i);
                    if (sock >= 0) {
                        int eof_signal = -1;
                        send(sock, &eof_signal, sizeof(eof_signal), 0);
                        close(sock);
                    }
                }
                break;
            }

            char c = input_string[current_index];
            int char_val = (unsigned char)c;
            int target_rank = char_val % pids_count;

            DBG_PRINT("Process %d: index=%d, char='%c'(%d) -> rank %d\n",
                     index, current_index, c, char_val, target_rank);

            if (target_rank == my_rank) {
                putchar(c);
                fflush(stdout);
                usleep(50000); // 50ms

                current_index++;

                if (current_index < str_len) {
                    char next_c = input_string[current_index];
                    int next_rank = (unsigned char)next_c % pids_count;

                    int sock = connect_to_process(next_rank);
                    if (sock >= 0) {
                        send(sock, &current_index, sizeof(current_index), 0);
                        close(sock);
                        active = 0;
                        DBG_PRINT("Process %d: sent index %d to rank %d\n",
                                 index, current_index, next_rank);
                    }
                }
            } else {
                int sock = connect_to_process(target_rank);
                if (sock >= 0) {
                    send(sock, &current_index, sizeof(current_index), 0);
                    close(sock);
                    active = 0;
                    DBG_PRINT("Process %d: forwarded to rank %d\n", index, target_rank);
                }
            }
        } else {
            struct pollfd pfd = {server_fd, POLLIN, 0};
            if (poll(&pfd, 1, 100) > 0) {
                int client = accept(server_fd, NULL, NULL);
                if (client >= 0) {
                    int msg;
                    if (recv(client, &msg, sizeof(msg), 0) == sizeof(msg)) {
                        if (msg == -1) {
                            eof_received = 1;
                            DBG_PRINT("Process %d: got EOF\n", index);
                        } else {
                            current_index = msg;
                            active = 1;
                            DBG_PRINT("Process %d: got index %d\n", index, current_index);
                        }
                    }
                    close(client);
                }
            }
        }
    }

    if (server_fd >= 0) {
        close(server_fd);
    }

    DBG_PRINT("Process %d (rank %d) finished\n", index, my_rank);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <string>\n", argv[0]);
        return 1;
    }

    printf("Processing: %s\n", argv[1]);
    printf("Creating %d processes...\n", N);

    for (int i = 0; i < N; i++) {
        if (fork() == 0) {
            process_function(i, argv[1]);
        }
        usleep(1000);
    }

    for (int i = 0; i < N; i++) {
        wait(NULL);
    }

    printf("\nDone!\n");
    return 0;
}


#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>

#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>


#ifdef DEBUG

#define DBG_PRINT(...)  printf("%s:%d ", __func__, __LINE__); \
                        printf(__VA_ARGS__);
#else
#define DBG_PRINT(...)
#endif

#define HARD_CHECK(cond, msg) if ( (int)(cond) < 0) { perror(msg); exit(EXIT_FAILURE); }


int SIG_0 = SIGUSR1;
int SIG_1 = SIGUSR2;

int SIG_GOT;

sigset_t full_mask;
sigset_t wait_mask;
sigset_t recieve_mask;

sig_atomic_t recieved_bit = -1;

void reciever_handler(int sign) {
    if (sign == SIG_0)
        recieved_bit = 0;
    else if (sign == SIG_1)
        recieved_bit = 1;
}

void sender_handler(int sign) {
    return;
}

int recieve_bit() {
    DBG_PRINT("Before sigsuspend, recieved_bit = %d\n", recieved_bit);
    sigsuspend(&recieve_mask);
    DBG_PRINT("After sigsuspend, recieved_bit = %d\n", recieved_bit);
    kill(getppid(), SIG_GOT);
    return recieved_bit;
}

int recieve_byte() {
    int byte = 0;
    for (int i = 0; i < 8; i++) {
        byte = byte + (recieve_bit() << i);
    }
    DBG_PRINT("byte: %d\n", byte);
    return byte;
}


void child () {

    pid_t my_pid = getpid();
    DBG_PRINT("pid %d: child\n", my_pid);
    struct sigaction action = {
        reciever_handler,
        full_mask,
        0,
        NULL
    };


    DBG_PRINT("pid %d: sigact SIG_0\n", my_pid);
    HARD_CHECK(sigaction(SIG_0, &action, NULL), "sigaction");
    DBG_PRINT("pid %d: sigact SIG_1\n", my_pid);
    HARD_CHECK(sigaction(SIG_1, &action, NULL), "sigaction");

    int ch = 0;
    DBG_PRINT("start rcv:\n");
    while ((ch = recieve_byte()) != 0) {
        DBG_PRINT("[%c](%d)", ch, ch);
        putc(ch, stdout);
    }
    DBG_PRINT("[%c](%d)", ch, ch);

    putc('\n', stdout);
    fflush(stdout);
    DBG_PRINT("rcv finished\n");
    exit(0);
}

void send_bit(int bit, pid_t pid) {
    if (bit == 0) {
        HARD_CHECK(kill(pid, SIG_0), "kill user 0");
    } else {
        HARD_CHECK(kill(pid, SIG_1), "kill user 1");
    }
    sigsuspend(&wait_mask);
}

void send_byte(int byte, pid_t pid) {
    for (int i = 0; i < 8; i++) {
        send_bit(byte & 1, pid);
        byte >>= 1;
    }
}

void send_message(pid_t pid, const char *msg, size_t size) {
    DBG_PRINT("pid: %d, size: %ld\n", (int)pid, size);
    for (size_t i = 0; i < size; i++) {
        DBG_PRINT("Send byte %c(%d)\n", msg[i], msg[i]);
        send_byte(msg[i], pid);
    }
}

int main(int argc, const char *argv[]) {
    SIG_GOT = SIG_0;
    sigemptyset(&full_mask);
    HARD_CHECK(sigaddset(&full_mask, SIG_1),   "delset usr1 fullmask");
    HARD_CHECK(sigaddset(&full_mask, SIG_0),   "delset usr0 fullmask");
    HARD_CHECK(sigaddset(&full_mask, SIG_GOT), "delset SIG_GOT fullmask");

    wait_mask = full_mask;
    recieve_mask = full_mask;

    HARD_CHECK(sigdelset(&wait_mask, SIG_GOT),  "delset waitmask");
    HARD_CHECK(sigdelset(&recieve_mask, SIG_0), "delset usr 0 rcvmask");
    HARD_CHECK(sigdelset(&recieve_mask, SIG_1), "delset usr 1 rcvmask");

    HARD_CHECK(sigprocmask(SIG_SETMASK, &full_mask, NULL), "fullmask procmask");

    struct sigaction action = {
        sender_handler,
        full_mask,
        0,
        NULL
    };

    HARD_CHECK(sigaction(SIG_GOT, &action, NULL), "sigaction SIG_GOT");
    DBG_PRINT("Init complete\n");

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork");
    } else if (pid == 0) {
        child();
    }

    DBG_PRINT("Parent: starting send\n");

    if (argc > 1) {
        const char *msg = argv[1];
        size_t msg_len = strlen(msg) + 1;
        send_message(pid, msg, msg_len);
        DBG_PRINT("msg sent by parent\n");
    } else {
        const char *filename = "main.c";
        struct stat st = {};
        stat(filename, &st);
        char *buffer = (char *) calloc(st.st_size + 1, sizeof(char));
        FILE * fp = fopen(filename, "r");
        size_t bytes_read = fread(buffer, sizeof(char), st.st_size, fp);

        if (bytes_read != st.st_size) {
            perror("Error when reading file\n");
        }
        buffer[st.st_size] = '\0';

        send_message(pid, buffer, st.st_size+1);
        DBG_PRINT("msg sent by parent\n");

        free(buffer);
        fclose(fp);
    }
    DBG_PRINT("sender: end\n");

    pid_t closed_pid;
    while ((closed_pid = wait(NULL)) != -1) {}

    return 0;
}

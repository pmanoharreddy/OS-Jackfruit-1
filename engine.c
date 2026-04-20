#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>

#define SOCK_PATH "/tmp/mini_runtime.sock"
#define MAX_CONTAINERS 10

typedef struct {
    char id[32];
    pid_t pid;
} container_t;

container_t containers[MAX_CONTAINERS];
int container_count = 0;

/* ---------------- CHILD FUNCTION ---------------- */

int child_func(void *arg) {
    char **args = (char **)arg;

    char *rootfs = args[0];
    char *cmd = args[1];

    if (chroot(rootfs) != 0) {
        perror("chroot failed");
        exit(1);
    }

    chdir("/");

    execlp(cmd, cmd, NULL);
    perror("exec failed");
    return 1;
}

/* ---------------- START CONTAINER ---------------- */

void start_container(char *id, char *rootfs, char *cmd) {
    char *stack = malloc(1024 * 1024);

    char *args[2];
    args[0] = rootfs;
    args[1] = cmd;

    pid_t pid = clone(child_func, stack + (1024 * 1024),
                      CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
                      args);

    if (pid < 0) {
        perror("clone failed");
        return;
    }

    strcpy(containers[container_count].id, id);
    containers[container_count].pid = pid;
    container_count++;

    printf("Started container %s (PID %d)\n", id, pid);
}

/* ---------------- STOP CONTAINER ---------------- */

void stop_container(char *id) {
    for (int i = 0; i < container_count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            kill(containers[i].pid, SIGKILL);
            printf("Stopped container %s\n", id);
            return;
        }
    }
    printf("Container not found\n");
}

/* ---------------- PRINT CONTAINERS ---------------- */

void list_containers(int client_fd) {
    char buffer[256] = {0};

    for (int i = 0; i < container_count; i++) {
        char line[64];
        sprintf(line, "%s RUNNING (pid=%d)\n",
                containers[i].id,
                containers[i].pid);
        strcat(buffer, line);
    }

    if (container_count == 0) {
        strcpy(buffer, "No containers running\n");
    }

    write(client_fd, buffer, strlen(buffer));
}

/* ---------------- SUPERVISOR ---------------- */

void run_supervisor() {
    int server_fd, client_fd;
    struct sockaddr_un addr;

    unlink(SOCK_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Supervisor running...\n");

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);

        char buffer[256] = {0};
        read(client_fd, buffer, sizeof(buffer));

        printf("Received: %s\n", buffer);

        char cmd[32], id[32], rootfs[128], prog[128];

        sscanf(buffer, "%s %s %s %s", cmd, id, rootfs, prog);

        if (strcmp(cmd, "start") == 0) {
            start_container(id, rootfs, prog);
            write(client_fd, "OK", 2);
        }
        else if (strcmp(cmd, "ps") == 0) {
            list_containers(client_fd);
        }
        else if (strcmp(cmd, "stop") == 0) {
            stop_container(id);
            write(client_fd, "STOPPED", 7);
        }
        else {
            write(client_fd, "Invalid command", 15);
        }

        close(client_fd);
    }
}

/* ---------------- CLIENT ---------------- */

void send_request(char *msg) {
    int sock;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        return;
    }

    write(sock, msg, strlen(msg));

    char buffer[256] = {0};
    read(sock, buffer, sizeof(buffer));

    printf("%s\n", buffer);

    close(sock);
}

/* ---------------- MAIN ---------------- */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("./engine supervisor\n");
        printf("./engine start <id> <rootfs> <cmd>\n");
        printf("./engine ps\n");
        printf("./engine stop <id>\n");
        return 1;
    }

    if (strcmp(argv[1], "supervisor") == 0) {
        run_supervisor();
    }
    else if (strcmp(argv[1], "start") == 0) {
        char msg[256];
        sprintf(msg, "start %s %s %s", argv[2], argv[3], argv[4]);
        send_request(msg);
    }
    else if (strcmp(argv[1], "ps") == 0) {
        send_request("ps");
    }
    else if (strcmp(argv[1], "stop") == 0) {
        char msg[256];
        sprintf(msg, "stop %s", argv[2]);
        send_request(msg);
    }
    else {
        printf("Invalid command\n");
    }

    return 0;
}

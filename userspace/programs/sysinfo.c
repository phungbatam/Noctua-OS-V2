#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    struct utsname {
        char sysname[64];
        char nodename[64];
        char release[64];
        char version[64];
        char machine[64];
    } un;

    char username[64];
    getusername(username, 64);

    printf("==============================\n");
    printf("  Noctua OS System Info\n");
    printf("==============================\n");

    if (uname(&un) == 0) {
        printf("  OS:        %s %s\n", un.sysname, un.release);
        printf("  Node:      %s\n", un.nodename);
        printf("  Version:   %s\n", un.version);
        printf("  Machine:   %s\n", un.machine);
    }

    printf("  User:      %s (UID=%d)\n", username, getuid());
    printf("  Group:     GID=%d\n", getgid());
    printf("  PID:       %d\n", getpid());
    printf("  PPID:      %d\n", getppid());
    printf("==============================\n");

    return 0;
}

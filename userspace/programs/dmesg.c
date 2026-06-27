#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    int fd = open("/dev/klog", 0);
    if (fd < 0) {
        printf("Kernel log not available\n");

        int pid = getpid();
        printf("System Information:\n");
        printf("  PID: %d\n", pid);
        printf("  PPID: %d\n", getppid());
        printf("  UID: %d\n", getuid());

        struct utsname {
            char sysname[64];
            char nodename[64];
            char release[64];
            char version[64];
            char machine[64];
        } un;

        if (uname(&un) == 0) {
            printf("  OS: %s\n", un.sysname);
            printf("  Node: %s\n", un.nodename);
            printf("  Release: %s\n", un.release);
            printf("  Version: %s\n", un.version);
            printf("  Machine: %s\n", un.machine);
        }
        return 0;
    }

    char buf[4096];
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    close(fd);
    return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    printf("Noctua OS Process Monitor\n");
    printf("PID  NAME            STATE\n");
    printf("---------------------------\n");

    int pid_fd = open("/proc", 0);
    if (pid_fd >= 0) {
        char buf[4096];
        int n = read(pid_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("%s", buf);
        }
        close(pid_fd);
    }

    printf("\nUse 'ps' for detailed process listing\n");
    return 0;
}

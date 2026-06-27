#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    printf("Noctua OS Memory Information\n");
    printf("----------------------------\n");

    int mem_fd = open("/dev/meminfo", 0);
    if (mem_fd >= 0) {
        char buf[1024];
        int n = read(mem_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("%s", buf);
        }
        close(mem_fd);
    } else {
        printf("Memory info not available via /dev\n");
        printf("Use kernel 'free' command\n");
    }
    return 0;
}

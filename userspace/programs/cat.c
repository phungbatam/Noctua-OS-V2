#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        char buf[256];
        int n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], 0);
        if (fd < 0) {
            printf("cat: %s: No such file\n", argv[i]);
            continue;
        }
        char buf[512];
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        close(fd);
    }
    return 0;
}

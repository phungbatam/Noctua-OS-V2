#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    const char *path = "/";
    if (argc >= 2) path = argv[1];

    struct stat st;
    if (stat(path, &st) < 0) {
        printf("ls: cannot access %s\n", path);
        return 1;
    }

    if (S_ISDIR(st.st_mode)) {
        printf("%s/:\n", path);
    } else {
        printf("%-32s %u bytes\n", path, st.st_size);
    }
    return 0;
}

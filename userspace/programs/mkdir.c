#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: mkdir <directory>\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i]) < 0) {
            printf("mkdir: %s: failed to create\n", argv[i]);
        }
    }
    return 0;
}

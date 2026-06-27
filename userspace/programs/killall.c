#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)envp;
    if (argc < 2) {
        printf("Usage: killall <process_name>\n");
        return 1;
    }

    printf("killall: %s - not yet implemented (use kernel 'kill' command)\n", argv[1]);
    return 0;
}

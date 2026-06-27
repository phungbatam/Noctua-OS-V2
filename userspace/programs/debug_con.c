#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    puts("Opening kernel debug console...");
    puts("Press F12 in kernel console or use 'debug' command");
    puts("Entering kernel debug mode...");

    debug_con();

    puts("Returned from debug console.");
    return 0;
}

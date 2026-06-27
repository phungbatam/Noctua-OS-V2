#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    printf("  PID  PPID PGRP  NAME\n");
    printf(" ----------------------\n");

    printf("  %-4d %-4d %-4d  kernel/scheduler\n", 0, 0, 0);
    printf("  %-4d %-4d %-4d  init\n", 1, 0, 1);

    int my_pid = getpid();
    printf("  %-4d %-4d %-4d  ps\n", my_pid, getppid(), my_pid);

    printf("\n  Use 'kill <pid>' to terminate a process\n");
    return 0;
}

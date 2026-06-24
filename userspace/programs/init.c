#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 256

int main(int argc, char *argv[], char *envp[]) {
    (void)argc; (void)argv; (void)envp;

    puts("Noctua OS init v2.0: starting system...");

    /* Mount devfs and procfs (handled by kernel now) */
    puts("init: kernel subsystems initialized");

    /* Try to run /etc/rc script */
    struct stat st;
    if (stat("/etc/rc", &st) == 0) {
        puts("init: running /etc/rc...");
        char *rc_argv[] = { "/bin/sh", "/etc/rc", NULL };
        char *rc_envp[] = { "PATH=/bin:/system", "HOME=/home/user", "SHELL=sh", NULL };
        pid_t pid = fork();
        if (pid == 0) {
            execve("/bin/sh", rc_argv, rc_envp);
            puts("init: rc exec failed");
            exit(1);
        }
        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            puts("init: /etc/rc completed");
        }
    }

    /* Start the shell */
    if (stat("/bin/sh", &st) == 0) {
        puts("init: starting shell on console...");

        while (1) {
            pid_t pid = fork();
            if (pid < 0) {
                puts("init: fork failed, halting");
                break;
            }
            if (pid == 0) {
                char *sh_argv[] = { "/bin/sh", NULL };
                char *sh_envp[] = { "PATH=/bin:/system", "HOME=/home/user", "SHELL=sh", "TERM=noctua", NULL };
                execve("/bin/sh", sh_argv, sh_envp);
                puts("init: execve shell failed");
                exit(1);
            }
            int status;
            waitpid(pid, &status, 0);
            puts("init: shell exited, restarting...");
        }
    } else {
        puts("init: /bin/sh not found, halting");
    }

    for (;;) {
        sleep(10000);
    }
    return 0;
}

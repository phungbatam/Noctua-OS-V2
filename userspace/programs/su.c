#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 256

int main(int argc, char *argv[], char *envp[]) {
    (void)envp;
    if (argc < 2) {
        printf("Usage: su <username>\n");
        return 1;
    }

    char password[MAX_LINE];
    printf("Password: ");
    int i = 0;
    while (1) {
        char c;
        if (read(0, &c, 1) != 1) break;
        if (c == '\n') break;
        if (c == '\b' && i > 0) { i--; continue; }
        if (i < MAX_LINE - 1) password[i++] = c;
    }
    password[i] = 0;
    printf("\n");

    if (auth_login(argv[1], password) != 0) {
        printf("su: authentication failed\n");
        return 1;
    }

    printf("Switched to user: %s\n", argv[1]);

    for (;;) {
        pid_t pid = fork();
        if (pid < 0) break;
        if (pid == 0) {
            char *sh_argv[] = { "/bin/sh", NULL };
            char env[3][64];
            snprintf(env[0], 64, "USER=%s", argv[1]);
            snprintf(env[1], 64, "HOME=/home/%s", argv[1]);
            snprintf(env[2], 64, "SHELL=/bin/sh");
            char *envp2[] = { env[0], env[1], env[2], NULL };
            execve("/bin/sh", sh_argv, envp2);
            exit(1);
        }
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}

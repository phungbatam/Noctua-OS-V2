#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_ATTEMPTS 3
#define MAX_LINE 256

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    char username[MAX_LINE];
    char password[MAX_LINE];
    int attempts = 0;

    puts("\n================================");
    puts("    Noctua OS v1.0 Login");
    puts("================================");

    while (attempts < MAX_ATTEMPTS) {
        printf("login: ");
        int i = 0;
        while (1) {
            char c;
            if (read(0, &c, 1) != 1) break;
            if (c == '\n') break;
            if (c == '\b' && i > 0) { i--; continue; }
            if (i < MAX_LINE - 1) username[i++] = c;
        }
        username[i] = 0;

        printf("password: ");
        i = 0;
        while (1) {
            char c;
            if (read(0, &c, 1) != 1) break;
            if (c == '\n') break;
            if (c == '\b' && i > 0) { i--; continue; }
            if (i < MAX_LINE - 1) password[i++] = c;
        }
        password[i] = 0;
        printf("\n");

        if (auth_login(username, password) == 0) {
            printf("\nWelcome, %s!\n", username);

            struct stat st;

            if (stat("/bin/sh", &st) == 0) {
                char *shell_argv[] = { "/bin/sh", NULL };
                char shell_envp[4][64];
                snprintf(shell_envp[0], 64, "USER=%s", username);
                snprintf(shell_envp[1], 64, "HOME=/home/%s", username);
                snprintf(shell_envp[2], 64, "SHELL=/bin/sh");
                snprintf(shell_envp[3], 64, "TERM=noctua");
                char *env[] = { shell_envp[0], shell_envp[1], shell_envp[2], shell_envp[3], NULL };

                for (;;) {
                    pid_t pid = fork();
                    if (pid < 0) {
                        puts("login: fork failed");
                        break;
                    }
                    if (pid == 0) {
                        execve("/bin/sh", shell_argv, env);
                        puts("login: shell exec failed");
                        exit(1);
                    }
                    int status;
                    waitpid(pid, &status, 0);
                }
            } else {
                puts("login: /bin/sh not found");
            }
            break;
        } else {
            attempts++;
            printf("Login failed (%d/%d)\n", attempts, MAX_ATTEMPTS);
        }
    }

    if (attempts >= MAX_ATTEMPTS) {
        puts("Too many failed attempts. System halted.");
    }

    for (;;) sleep(10);
    return 0;
}

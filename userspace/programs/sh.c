#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_ARGS 64
#define MAX_LINE 512
#define PATH_MAX 128
#define MAX_CMDS 16

static void parse_line(char *line, int *argc, char *argv[]) {
    *argc = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*argc >= MAX_ARGS - 1) break;
        if (*p == '"') {
            p++;
            argv[*argc] = p;
            while (*p && *p != '"') p++;
            if (*p) { *p = '\0'; p++; }
        } else {
            argv[*argc] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) { *p = '\0'; p++; }
        }
        (*argc)++;
    }
    argv[*argc] = NULL;
}

static int try_exec(char *path, char *argv[], char *envp[]) {
    struct stat st;
    if (stat(path, &st) == 0) {
        execve(path, argv, envp);
    }
    return -1;
}

static int is_builtin(char *cmd) {
    return (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "help") == 0 || strcmp(cmd, "clear") == 0 ||
            strcmp(cmd, "echo") == 0);
}

static char shell_cwd[PATH_MAX] = "/";

static void shell_set_cwd(const char *path) {
    strncpy(shell_cwd, path, PATH_MAX - 1);
    shell_cwd[PATH_MAX - 1] = 0;
}

static int exec_builtin(int argc, char *argv[]) {
    if (strcmp(argv[0], "exit") == 0) {
        puts("logout");
        return 2;
    }
    if (strcmp(argv[0], "cd") == 0) {
        if (argc < 2 || strcmp(argv[1], "~") == 0) {
            chdir("/home/user");
            shell_set_cwd("/home/user");
            return 0;
        }
        if (strcmp(argv[1], "..") == 0) {
            char *last = shell_cwd + strlen(shell_cwd);
            while (last > shell_cwd && *last != '/') last--;
            if (last > shell_cwd) *last = '\0';
            else shell_cwd[1] = '\0';
            chdir(shell_cwd);
            return 0;
        }
        struct stat st;
        if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
            chdir(argv[1]);
            shell_set_cwd(argv[1]);
        } else {
            printf("cd: %s: No such directory\n", argv[1]);
        }
        return 0;
    }
    if (strcmp(argv[0], "help") == 0) {
        puts("Noctua OS Shell built-in commands:");
        puts("  cd <dir>  - change directory");
        puts("  exit      - logout");
        puts("  help      - this help");
        puts("  clear     - clear screen");
        puts("  echo      - print text");
        puts("Also: pipe (|) and redirection (>, <) supported");
        return 0;
    }
    if (strcmp(argv[0], "clear") == 0) {
        write(1, "\033[2J\033[H", 7);
        return 0;
    }
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) putchar(' ');
            printf("%s", argv[i]);
        }
        putchar('\n');
        return 0;
    }
    return 0;
}

static void run_command(char *cmds[], int ncmds, char *envp[]) {
    int prev_pipe[2] = {-1, -1};
    int next_pipe[2];
    pid_t children[MAX_CMDS];
    int nchildren = 0;

    for (int i = 0; i < ncmds; i++) {
        char line[MAX_LINE];
        strcpy(line, cmds[i]);
        int argc;
        char *argv[MAX_ARGS];
        parse_line(line, &argc, argv);

        if (argc == 0) continue;

        char *infile = NULL;
        char *outfile = NULL;
        int outappend = 0;
        int newargc = 0;
        char *newargv[MAX_ARGS];

        for (int j = 0; j < argc; j++) {
            if (strcmp(argv[j], ">") == 0 && j + 1 < argc) {
                outfile = argv[++j];
                outappend = 0;
            } else if (strcmp(argv[j], ">>") == 0 && j + 1 < argc) {
                outfile = argv[++j];
                outappend = 1;
            } else if (strcmp(argv[j], "<") == 0 && j + 1 < argc) {
                infile = argv[++j];
            } else {
                newargv[newargc++] = argv[j];
            }
        }
        newargv[newargc] = NULL;

        if (is_builtin(newargv[0])) {
            if (ncmds == 1 && !infile && !outfile) {
                int ret = exec_builtin(newargc, newargv);
                if (ret == 2) exit(0);
            } else {
                printf("sh: builtins not supported in pipes\n");
            }
            continue;
        }

        if (i < ncmds - 1) {
            pipe(next_pipe);
        }

        pid_t pid = fork();
        if (pid < 0) {
            puts("sh: fork failed");
            return;
        }

        if (pid == 0) {
            if (infile) {
                int fd = open(infile, 0);
                if (fd < 0) {
                    printf("sh: %s: No such file\n", infile);
                    exit(1);
                }
                dup2(fd, 0);
                close(fd);
            }
            if (outfile) {
                int flags = 1;
                int fd = open(outfile, flags);
                if (fd < 0) {
                    fd = open(outfile, 0x42);
                }
                if (fd < 0) exit(1);
                dup2(fd, 1);
                close(fd);
            }
            if (prev_pipe[0] >= 0) {
                dup2(prev_pipe[0], 0);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            if (i < ncmds - 1) {
                close(next_pipe[0]);
                dup2(next_pipe[1], 1);
                close(next_pipe[1]);
            }

            char path[PATH_MAX];
            if (newargv[0][0] == '/') {
                try_exec(newargv[0], newargv, envp);
            } else {
                struct stat st;
                int found = 0;
                const char *paths[] = {"/bin/", "/system/", NULL};
                for (int k = 0; paths[k]; k++) {
                    strcpy(path, paths[k]);
                    strcat(path, newargv[0]);
                    if (stat(path, &st) == 0) {
                        execve(path, newargv, envp);
                        found = 1;
                    }
                }
                if (!found) {
                    printf("sh: %s: not found\n", newargv[0]);
                }
            }
            exit(1);
        }

        children[nchildren++] = pid;

        if (prev_pipe[0] >= 0) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }
        if (i < ncmds - 1) {
            prev_pipe[0] = next_pipe[0];
            prev_pipe[1] = next_pipe[1];
        }
    }

    for (int i = 0; i < nchildren; i++) {
        int status;
        waitpid(children[i], &status, 0);
    }
}

int main(int argc, char *argv[], char *envp[]) {
    (void)argc; (void)argv;

    puts("Noctua OS Shell v2.0 - Real OS Edition");
    puts("Type 'help' for commands");

    char line[MAX_LINE];

    char *shell_envp[] = {
        "PATH=/bin:/system",
        "HOME=/home/user",
        "SHELL=sh",
        NULL
    };
    if (envp && envp[0]) shell_envp[0] = envp[0];

    char cwd[PATH_MAX] = "/";

    while (1) {
        printf("%s $ ", cwd);
        if (!gets(line)) break;

        char *p = line;
        while (*p == ' ') p++;
        if (!*p) continue;

        int bg = 0;
        int len = strlen(p);
        if (len > 0 && p[len - 1] == '&') {
            bg = 1;
            p[len - 1] = '\0';
            while (len > 1 && p[len - 2] == ' ') { p[--len - 1] = '\0'; }
        }

        char *cmds[MAX_CMDS];
        int ncmds = 0;
        cmds[ncmds++] = p;
        for (char *q = p; *q && ncmds < MAX_CMDS; q++) {
            if (*q == '|') {
                *q = '\0';
                cmds[ncmds++] = q + 1;
            }
        }

        if (ncmds == 1) {
            int argc;
            char *argv[MAX_ARGS];
            parse_line(p, &argc, argv);
            if (argc > 0 && is_builtin(argv[0])) {
                int ret = exec_builtin(argc, argv);
                if (ret == 2) break;
                continue;
            }
        }

        pid_t shell_pid = fork();
        if (shell_pid < 0) {
            puts("sh: fork failed");
            continue;
        }

        if (shell_pid == 0) {
            run_command(cmds, ncmds, shell_envp);
            exit(0);
        }

        if (!bg) {
            int status;
            waitpid(shell_pid, &status, 0);
        } else {
            printf("[%d] %d\n", shell_pid, shell_pid);
        }
    }
    return 0;
}

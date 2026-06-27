#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    char username[64];
    getusername(username, 64);

    printf("uid=%d(%s) gid=%d euid=%d egid=%d\n",
           getuid(), username, getgid(), geteuid(), getegid());
    return 0;
}

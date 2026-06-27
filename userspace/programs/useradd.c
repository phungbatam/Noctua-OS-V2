#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)envp;
    if (argc < 3) {
        printf("Usage: useradd <username> <password> [uid]\n");
        return 1;
    }

    unsigned int uid = 0;
    if (argc >= 4) {
        uid = 0;
        int i = 0;
        while (argv[3][i]) {
            uid = uid * 10 + (argv[3][i] - '0');
            i++;
        }
    } else {
        uid = usercount();
    }

    if (useradd(argv[1], argv[2], uid, uid) == 0) {
        printf("User '%s' created (uid=%d)\n", argv[1], uid);
        return 0;
    } else {
        printf("Failed to create user '%s'\n", argv[1]);
        return 1;
    }
}

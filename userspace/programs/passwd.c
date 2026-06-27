#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_LINE 256

int main(int argc, char *argv[], char *envp[]) {
    (void)envp;

    char username[MAX_LINE];
    char newpass[MAX_LINE];
    char confirm[MAX_LINE];

    if (argc >= 2) {
        strncpy(username, argv[1], MAX_LINE - 1);
    } else {
        getusername(username, MAX_LINE);
    }

    printf("Changing password for %s\n", username);
    printf("New password: ");
    int i = 0;
    while (1) {
        char c;
        if (read(0, &c, 1) != 1) break;
        if (c == '\n') break;
        if (c == '\b' && i > 0) { i--; continue; }
        if (i < MAX_LINE - 1) newpass[i++] = c;
    }
    newpass[i] = 0;
    printf("\n");

    printf("Retype new password: ");
    i = 0;
    while (1) {
        char c;
        if (read(0, &c, 1) != 1) break;
        if (c == '\n') break;
        if (c == '\b' && i > 0) { i--; continue; }
        if (i < MAX_LINE - 1) confirm[i++] = c;
    }
    confirm[i] = 0;
    printf("\n");

    if (strcmp(newpass, confirm) != 0) {
        printf("Passwords do not match\n");
        return 1;
    }

    if (setpasswd(username, newpass) == 0) {
        printf("Password changed successfully\n");
        return 0;
    } else {
        printf("Failed to change password\n");
        return 1;
    }
}

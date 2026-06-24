#include <stdio.h>
#include <unistd.h>

struct utsname {
    char sysname[64];
    char nodename[64];
    char release[64];
    char version[64];
    char machine[64];
};

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    struct utsname buf;
    if (uname(&buf) < 0) {
        printf("uname: failed\n");
        return 1;
    }
    printf("%s %s %s %s %s\n", buf.sysname, buf.nodename, buf.release, buf.version, buf.machine);
    return 0;
}

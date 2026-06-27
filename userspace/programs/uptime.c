#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    unsigned int ticks = 0;
    int tick_fd = open("/dev/uptime", 0);
    if (tick_fd >= 0) {
        char buf[16];
        int n = read(tick_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            ticks = 0;
            for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
                ticks = ticks * 10 + (buf[i] - '0');
        }
        close(tick_fd);
    }

    unsigned int hours = ticks / 3600;
    unsigned int minutes = (ticks % 3600) / 60;
    unsigned int seconds = ticks % 60;

    printf("Uptime: %uh %um %us\n", hours, minutes, seconds);
    return 0;
}

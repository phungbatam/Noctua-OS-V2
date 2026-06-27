#include <unistd.h>
#include <stdio.h>

int main(void) {
    puts("Noctua OS: System halted (use Ctrl+C in QEMU)");
    for (;;) {
        sleep(10);
    }
    return 0;
}

#include <unistd.h>

int main(void) {
    const char cls[] = "\033[2J\033[H";
    write(STDOUT_FILENO, cls, 7);
    return 0;
}

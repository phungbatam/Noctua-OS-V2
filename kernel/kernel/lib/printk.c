#include "printk.h"
#include "vsprintf.h"
#include "string.h"
#include "serial.h"
#include "pit.h"
#include "klog.h"

static int pit_ready = 0;

void printk_init(void) {
    pit_ready = 0;
    klog_init();
}

void printk(const char *fmt, ...) {
    if (!fmt) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const char *p = buf;
    while (*p) {
        serial_write_char(COM1_PORT, *p);
        p++;
    }
    serial_write_char(COM1_PORT, '\n');

    if (pit_ready) {
        klog_write("%s", buf);
    }
}

void printk_set_pit_ready(void) {
    pit_ready = 1;
}

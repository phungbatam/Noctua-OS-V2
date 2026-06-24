#include "klog.h"
#include "vsprintf.h"
#include "string.h"
#include "pit.h"
#include "screen.h"

static char klog_buf[KLOG_BUF_SIZE][KLOG_LINE_MAX];
static int klog_count = 0;
static int klog_pos = 0;
static int klog_ready = 0;
static uint32_t klog_boot_ticks = 0;

void klog_init(void) {
    klog_count = 0;
    klog_pos = 0;
    klog_ready = 1;
    klog_boot_ticks = 0;
}

static void klog_timestamp(char *out, int len) {
    if (!klog_ready) {
        out[0] = 0;
        return;
    }
    uint32_t ticks;
    if (klog_boot_ticks == 0) {
        klog_boot_ticks = pit_get_ticks();
        ticks = 0;
    } else {
        ticks = pit_get_ticks() - klog_boot_ticks;
    }
    unsigned long secs = ticks / TICK_HZ;
    unsigned long usecs = (ticks % TICK_HZ) * (1000000 / TICK_HZ);
    snprintf(out, len, "[%5lu.%06lu] ", secs, usecs);
}

void klog_write(const char *fmt, ...) {
    if (!klog_ready) return;

    char ts[32];
    klog_timestamp(ts, sizeof(ts));

    char msg[KLOG_LINE_MAX - 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    snprintf(klog_buf[klog_pos], KLOG_LINE_MAX, "%s%s", ts, msg);
    klog_pos = (klog_pos + 1) % KLOG_BUF_SIZE;
    if (klog_count < KLOG_BUF_SIZE) klog_count++;
}

int klog_get_count(void) {
    return klog_count;
}

const char *klog_get_line(int idx) {
    if (idx < 0 || idx >= klog_count) return 0;
    int start = klog_pos - klog_count;
    if (start < 0) start += KLOG_BUF_SIZE;
    int actual = (start + idx) % KLOG_BUF_SIZE;
    return klog_buf[actual];
}

void klog_dump(void) {
    for (int i = 0; i < klog_count; i++) {
        const char *line = klog_get_line(i);
        if (line) {
            const char *p = line;
            while (*p) {
                screen_term_putchar(*p);
                p++;
            }
            screen_term_putchar('\n');
        }
    }
}

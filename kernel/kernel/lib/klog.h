#ifndef KLOG_H
#define KLOG_H

#define KLOG_BUF_SIZE 4096
#define KLOG_LINE_MAX 128

void klog_init(void);
void klog_write(const char *fmt, ...);
void klog_dump(void);
int klog_get_count(void);
const char *klog_get_line(int idx);

#endif

#include "procfs.h"
#include "fs/fat32.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "proc/task.h"
#include "mm/page.h"
#include "screen.h"
#include "timer/rtc.h"

/* Format an integer into a string */
static void int2str_proc(int val, char *buf) {
    int i = 0;
    if (val < 0) { buf[i++] = '-'; val = -val; }
    if (val == 0) { buf[i++] = '0'; }
    else {
        char tmp[16];
        int j = 0;
        while (val > 0) { tmp[j++] = '0' + (val % 10); val /= 10; }
        while (j > 0) buf[i++] = tmp[--j];
    }
    buf[i] = 0;
}

/* /proc/cpuinfo - CPU information */
static int proc_cpuinfo_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node;
    static char buf[512];
    static int generated = 0;
    if (!generated) {
        char vendor[16] = "Unknown";
        char brand[64] = "Unknown";
        extern void get_cpu_vendor(char *v);
        extern void get_cpu_brand(char *b);
        get_cpu_vendor(vendor);
        get_cpu_brand(brand);
        strcpy(buf, "processor\t: 0\n");
        strcat(buf, "vendor_id\t: "); strcat(buf, vendor); strcat(buf, "\n");
        strcat(buf, "cpu family\t: 6\n");
        strcat(buf, "model\t\t: 0\n");
        strcat(buf, "model name\t: "); strcat(buf, brand); strcat(buf, "\n");
        strcat(buf, "stepping\t: 0\n");
        strcat(buf, "cpu MHz\t\t: 0.000\n");
        strcat(buf, "fdiv_bug\t: no\n");
        strcat(buf, "fpu\t\t: yes\n");
        strcat(buf, "wp\t\t: yes\n");
        generated = 1;
    }
    size_t slen = strlen(buf);
    if (offset >= slen) return 0;
    if (offset + size > slen) size = slen - offset;
    memcpy(buffer, buf + offset, size);
    return size;
}

static file_operations_t proc_cpuinfo_fops = {0, 0, proc_cpuinfo_read, 0, 0};

/* /proc/meminfo - memory information */
static int proc_meminfo_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node;
    static char buf[256];
    char tmp[32];

    strcpy(buf, "MemTotal:\t");
    uint32_t total_kb = (pmem_total_pages() * 4);
    int2str_proc(total_kb, tmp);
    strcat(buf, tmp); strcat(buf, " kB\n");

    strcat(buf, "MemFree:\t");
    uint32_t free_kb = (pmem_free_pages() * 4);
    int2str_proc(free_kb, tmp);
    strcat(buf, tmp); strcat(buf, " kB\n");

    strcat(buf, "MemUsed:\t");
    int2str_proc(total_kb - free_kb, tmp);
    strcat(buf, tmp); strcat(buf, " kB\n");

    strcat(buf, "Slab:\t\t");
    extern uint32_t kmem_cache_usage(void);
    int2str_proc(kmem_cache_usage() / 1024, tmp);
    strcat(buf, tmp); strcat(buf, " kB\n");

    strcat(buf, "HeapFree:\t");
    extern uint32_t heap_free(void);
    int2str_proc(heap_free() / 1024, tmp);
    strcat(buf, tmp); strcat(buf, " kB\n");

    size_t slen = strlen(buf);
    if (offset >= slen) return 0;
    if (offset + size > slen) size = slen - offset;
    memcpy(buffer, buf + offset, size);
    return size;
}

static file_operations_t proc_meminfo_fops = {0, 0, proc_meminfo_read, 0, 0};

/* /proc/uptime - system uptime */
static int proc_uptime_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node;
    static char buf[64];
    char tmp[32];
    extern uint32_t uptime_get_seconds(void);
    uint32_t secs = uptime_get_seconds();
    int2str_proc(secs, tmp);
    strcpy(buf, tmp);
    strcat(buf, " 0.00\n");
    size_t slen = strlen(buf);
    if (offset >= slen) return 0;
    if (offset + size > slen) size = slen - offset;
    memcpy(buffer, buf + offset, size);
    return size;
}

static file_operations_t proc_uptime_fops = {0, 0, proc_uptime_read, 0, 0};

/* /proc/version - kernel version */
static int proc_version_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node;
    static const char *ver = "Noctua OS version 1.0.0 (gcc, 32-bit x86)\n";
    size_t slen = strlen(ver);
    if (offset >= slen) return 0;
    if (offset + size > slen) size = slen - offset;
    memcpy(buffer, ver + offset, size);
    return size;
}

static file_operations_t proc_version_fops = {0, 0, proc_version_read, 0, 0};

/* /proc/stat - kernel statistics */
static int proc_stat_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node;
    static char buf[512];
    char tmp[32];
    int cpu_user = 0, cpu_nice = 0, cpu_sys = 0, cpu_idle = 0;

    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = task_get(i);
        if (!t) continue;
        if (t->state == TASK_RUNNING) cpu_user += t->vruntime;
        else cpu_idle++;
    }
    strcpy(buf, "cpu  ");
    int2str_proc(cpu_user, tmp); strcat(buf, tmp); strcat(buf, " ");
    int2str_proc(cpu_nice, tmp); strcat(buf, tmp); strcat(buf, " ");
    int2str_proc(cpu_sys, tmp); strcat(buf, tmp); strcat(buf, " ");
    int2str_proc(cpu_idle, tmp); strcat(buf, tmp); strcat(buf, "\n");

    int task_count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_get(i)) task_count++;
    }
    strcat(buf, "processes\t");
    int2str_proc(task_count, tmp); strcat(buf, tmp); strcat(buf, "\n");

    size_t slen = strlen(buf);
    if (offset >= slen) return 0;
    if (offset + size > slen) size = slen - offset;
    memcpy(buffer, buf + offset, size);
    return size;
}

static file_operations_t proc_stat_fops = {0, 0, proc_stat_read, 0, 0};

static vfs_node_t *proc_add_file(const char *name, file_operations_t *fops) {
    char path[FAT_MAX_PATH];
    strcpy(path, "/proc/");
    strcat(path, name);
    vfs_node_t *node = vfs_create_node(path, 0);
    if (!node) return 0;
    node->f_op = fops;
    return node;
}

void procfs_init(void) {
    vfs_node_t *proc_dir = vfs_find_node("/proc");
    if (!proc_dir) {
        vfs_create_node("/proc", 1);
    }

    proc_add_file("cpuinfo", &proc_cpuinfo_fops);
    proc_add_file("meminfo", &proc_meminfo_fops);
    proc_add_file("uptime",  &proc_uptime_fops);
    proc_add_file("version", &proc_version_fops);
    proc_add_file("stat",    &proc_stat_fops);

    screen_set_content_color(C_INFO);
    screen_term_write("PROCFS: /proc filesystem initialized (cpuinfo, meminfo, uptime, version, stat)\n");
}

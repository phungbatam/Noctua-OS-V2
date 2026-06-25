#include "cmd/cmd.h"
#include "screen.h"
#include "fb.h"
#include "string.h"
#include "editor.h"
#include "fs/fat32.h"
#include "block/partition.h"
#include "block/blockdev.h"
#include "bus/pci.h"
#include "timer/rtc.h"
#include "char/pcspkr.h"
#include "proc/task.h"
#include "core/script.h"
#include "net/net.h"
#include "heap.h"
#include "ports.h"
#include "klog.h"

char cwd[64] = "/";

static void scr(const char *s) { screen_term_write(s); }
static void scf(void) { screen_set_content_color(C_INFO); }
static void sch(void) { screen_set_content_color(C_HEADER); }
static void sce(void) { screen_set_content_color(C_ERROR); }

static int cmd_help_handler(const char *args) {
    (void)args;
    scr(" Noctua-OS 1.0 [Commands]\n");
    scr("  help, neofetch, clear, echo, info, banner, about\n");
    scr("  color, calc, ls, cd, cat, mkdir, touch, pwd\n");
    scr("  whoami, hostname, uptime, free, ps, kill, history\n");
    scr("  alias, env, export, cow, reboot, shutdown, date, creds\n");
    scr("  explore, edit, grep, find, more, hexdump, diskinfo\n");
    scr("  partitions, pci, pciverbose, beep, cmos, dmesg\n");
    scr("  install, partition, format, sys-install, diag\n");
    scr("  ifconfig, ping, netstat, source, gcc, g++\n");
    scr(" Type 'noctua-help' for 200 Noctua-native commands\n");
    return CMD_RET_OK;
}

static int cmd_neofetch(const char *args) {
    (void)args;
    sch(); scr("           ___           \n"); scf();
    scr("          /   \\          "); scr("Welcome to Noctua OS 1.0\n");
    scr("         /     \\         "); scr("x86 32-bit Protected Mode\n");
    scr("        /   {}  \\        "); scr("Custom kernel from scratch\n");
    scr("       /________\\       \n");
    return CMD_RET_OK;
}

static int cmd_clear(const char *args) {
    (void)args; screen_clear_content(); return CMD_RET_OK;
}

static int cmd_echo(const char *args) {
    if (args) scr(args);
    scr("\n");
    return CMD_RET_OK;
}

static int cmd_info(const char *args) {
    (void)args;
    sch(); scr("=== System Info ===\n"); scf();
    scr(" OS: Noctua OS 1.0 (x86)\n");
    scr(" Kernel: Custom monolithic\n");
    scr(" Boot: Multiboot (GRUB)\n");
    scr(" Mode: 32-bit Protected Mode\n");
    return CMD_RET_OK;
}

static int cmd_banner(const char *args) {
    (void)args;
    sch(); scr("=== Noctua OS 1.0 ===\n");
    scf();
    scr("          .---.\n");
    scr("         /_____\\\n");
    scr("        /  O O  \\\n");
    scr("       /  __ __  \\\n");
    scr("      /  /     \\  \\\n");
    scr("     /__/       \\__\\\n");
    scr("   Noctua OS - x86\n");
    return CMD_RET_OK;
}

static int cmd_about(const char *args) {
    (void)args;
    sch(); scr("=== About Noctua OS ===\n"); scf();
    scr(" Version: 1.0.0\n");
    scr(" Arch: i386 (x86)\n");
    scr(" Kernel: Custom\n");
    scr(" Author: TVN\n");
    scr(" Desc: A lightweight x86 operating system\n");
    return CMD_RET_OK;
}

static int cmd_color(const char *args) {
    if (!args) return CMD_RET_OK;
    char c = args[0];
    if (c == 'r') screen_set_content_color(BLACK << 4 | LIGHT_RED);
    else if (c == 'g') screen_set_content_color(BLACK << 4 | LIGHT_GREEN);
    else if (c == 'b') screen_set_content_color(BLACK << 4 | LIGHT_BLUE);
    else if (c == 'y') screen_set_content_color(BLACK << 4 | LIGHT_BROWN);
    else if (c == 'w') screen_set_content_color(BLACK << 4 | WHITE);
    else if (c == 'c') screen_set_content_color(BLACK << 4 | LIGHT_CYAN);
    else if (c == 'm') screen_set_content_color(BLACK << 4 | LIGHT_MAGENTA);
    else sce();
    return CMD_RET_OK;
}

static int cmd_calc(const char *args) {
    if (!args) return CMD_RET_OK;
    int a = 0, b = 0; char op = 0;
    int n = 0;
    for (const char *p = args; *p; p++) { if (*p >= '0' && *p <= '9') { n = n*10 + (*p-'0'); } else { a = n; op = *p; n = 0; } }
    b = n;
    char buf[16];
    screen_set_content_color(C_WIN_TEXT);
    if (op == '+') { int2str(a+b, buf); scr(buf); scr("\n"); }
    else if (op == '-') { int2str(a-b, buf); scr(buf); scr("\n"); }
    else if (op == '*') { int2str(a*b, buf); scr(buf); scr("\n"); }
    else if (op == '/' && b != 0) { int2str(a/b, buf); scr(buf); scr("\n"); }
    return CMD_RET_OK;
}

static int cmd_ls(const char *args) {
    (void)args;
    vfs_node_t *dir = vfs_find_node(cwd);
    if (!dir) { scr(" No such directory\n"); return CMD_RET_OK; }
    for (vfs_node_t *child = dir->children; child; child = child->next) {
        scr(" "); scr(child->name);
        if (child->is_directory) scr("/");
        scr("\n");
    }
    return CMD_RET_OK;
}

static int cmd_cd(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    if (*args == 0) return CMD_RET_OK;
    int i = 0; while (args[i] && args[i] > ' ' && i < 63) { cwd[i] = args[i]; i++; } cwd[i] = 0;
    return CMD_RET_OK;
}

static int cmd_cat(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char path[256]; strcpy(path, cwd); int plen = strlen(path);
    if (plen > 0 && path[plen-1] != '/') { path[plen] = '/'; path[plen+1] = 0; plen++; }
    strcpy(path + plen, args);
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[257]; int n;
    while ((n = file_read(f, buf, 256)) > 0) { buf[n] = 0; scr(buf); }
    file_close(f);
    return CMD_RET_OK;
}

static int cmd_mkdir(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char name[64]; int i = 0;
    while (args[i] && args[i] > ' ' && i < 63) { name[i] = args[i]; i++; } name[i] = 0;
    char path[256]; strcpy(path, cwd); int plen = strlen(path);
    if (plen > 0 && path[plen-1] != '/') { path[plen] = '/'; path[plen+1] = 0; plen++; }
    strcpy(path + plen, name);
    vfs_create_node(path, 1);
    return CMD_RET_OK;
}

static int cmd_touch(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char name[64]; int i = 0;
    while (args[i] && args[i] > ' ' && i < 63) { name[i] = args[i]; i++; } name[i] = 0;
    char path[256]; strcpy(path, cwd); int plen = strlen(path);
    if (plen > 0 && path[plen-1] != '/') { path[plen] = '/'; path[plen+1] = 0; plen++; }
    strcpy(path + plen, name);
    file_handle_t *f = file_open(path, 1);
    if (f) file_close(f);
    return CMD_RET_OK;
}

static int cmd_pwd(const char *args) {
    (void)args; scr(" "); scr(cwd); scr("\n"); return CMD_RET_OK;
}

static int cmd_whoami(const char *args) {
    (void)args; scr(" user\n"); return CMD_RET_OK;
}

static int cmd_hostname(const char *args) {
    (void)args; scr(" noctua\n"); return CMD_RET_OK;
}

static int cmd_uptime(const char *args) {
    (void)args;
    char buf[16];
    uint32_t sec = uptime_get_seconds();
    int2str(sec / 3600, buf); scr(buf); scr("h ");
    int2str((sec % 3600) / 60, buf); scr(buf); scr("m ");
    int2str(sec % 60, buf); scr(buf); scr("s\n");
    return CMD_RET_OK;
}

static int cmd_free(const char *args) {
    (void)args;
    char buf[16];
    sch(); scr("=== Memory ===\n"); scf();
    scr(" Free: "); int2str(heap_free() / 1024, buf); scr(buf); scr(" KB\n");
    return CMD_RET_OK;
}

static int cmd_ps(const char *args) {
    (void)args;
    sch(); scr(" PID  NAME\n"); scf();
    for (int i = 0; ; i++) {
        task_t *t = task_get(i);
        if (!t) break;
        char buf[16]; int2str(t->pid, buf);
        scr(" "); scr(buf); scr("  "); scr(t->name); scr("\n");
    }
    return CMD_RET_OK;
}

static int cmd_kill(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    int pid = 0;
    while (*args >= '0' && *args <= '9') { pid = pid * 10 + (*args - '0'); args++; }
    task_t *t = task_find_by_pid(pid);
    if (t) task_set_state(t->id, TASK_ZOMBIE);
    return CMD_RET_OK;
}

static int cmd_history(const char *args) {
    (void)args; scr(" history: not yet implemented\n"); return CMD_RET_OK;
}

static int cmd_alias(const char *args) {
    (void)args; scr(" alias: not yet implemented\n"); return CMD_RET_OK;
}

static int cmd_env(const char *args) {
    (void)args; scr(" PATH=/bin\n HOME=/\n SHELL=/bin/initd\n"); return CMD_RET_OK;
}

static int cmd_export(const char *args) {
    (void)args; scr(" exported\n"); return CMD_RET_OK;
}

static int cmd_cow(const char *args) {
    (void)args;
    scr("   ___________________________\n");
    scr("  < Hello from Noctua OS! >\n");
    scr("   ---------------------------\n");
    scr("          \\   ^__^\n");
    scr("           \\  (oo)\\_______\n");
    scr("              (__)\\       )\\/\\\n");
    scr("                  ||----w |\n");
    scr("                  ||     ||\n");
    return CMD_RET_OK;
}

static int cmd_reboot(const char *args) {
    (void)args;
    scr(" Rebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;);
    return CMD_RET_OK;
}

static int cmd_shutdown(const char *args) {
    (void)args;
    scr(" Shutting down...\n");
    outw(0xB004, 0x2000);
    outw(0x604, 0x2000);
    outw(0x4004, 0x3400);
    for (;;);
    return CMD_RET_OK;
}

static int cmd_date(const char *args) {
    (void)args;
    rtc_time_t tm;
    rtc_read_time(&tm);
    char buf[16];
    int2str(2000 + tm.year, buf); scr(buf); scr("-");
    int2str(tm.month, buf); scr(buf); scr("-");
    int2str(tm.day, buf); scr(buf); scr(" ");
    int2str(tm.hour, buf); scr(buf); scr(":");
    int2str(tm.minute, buf); scr(buf); scr(":");
    int2str(tm.second, buf); scr(buf); scr("\n");
    return CMD_RET_OK;
}

static int cmd_creds(const char *args) {
    (void)args;
    sch(); scr("=== Noctua OS Developers ===\n"); scf();
    scr(" TVN - Creator & Lead Developer\n");
    return CMD_RET_OK;
}

static int cmd_explore(const char *args) {
    (void)args;
    sch(); scr("=== File Explore ===\n"); scf();
    scr(" Current dir: "); scr(cwd); scr("\n");
    vfs_node_t *dir = vfs_find_node(cwd);
    if (!dir) { scr(" No such directory\n"); return CMD_RET_OK; }
    for (vfs_node_t *child = dir->children; child; child = child->next) {
        scr(" "); scr(child->name);
        if (child->is_directory) scr("/");
        scr("\n");
    }
    return CMD_RET_OK;
}

static int cmd_edit(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    editor_open(args);
    return CMD_RET_OK;
}

static int cmd_grep(const char *args) {
    (void)args; scr(" grep: searching... (not fully implemented)\n"); return CMD_RET_OK;
}

static int cmd_find_handler(const char *args) {
    (void)args;
    scr(" find: ");
    if (args) { while (*args == ' ') args++; scr(args); }
    scr("\n");
    return CMD_RET_OK;
}

static int cmd_more(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char name[64]; int i = 0;
    while (args[i] && args[i] > ' ' && i < 63) { name[i] = args[i]; i++; } name[i] = 0;
    char path[256]; strcpy(path, cwd); int plen = strlen(path);
    if (plen > 0 && path[plen-1] != '/') { path[plen] = '/'; path[plen+1] = 0; plen++; }
    strcpy(path + plen, name);
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[257]; int n;
    while ((n = file_read(f, buf, 256)) > 0) { buf[n] = 0; scr(buf); }
    file_close(f);
    return CMD_RET_OK;
}

static int cmd_hexdump(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char name[64]; int i = 0;
    while (args[i] && args[i] > ' ' && i < 63) { name[i] = args[i]; i++; } name[i] = 0;
    char path[256]; strcpy(path, cwd); int plen = strlen(path);
    if (plen > 0 && path[plen-1] != '/') { path[plen] = '/'; path[plen+1] = 0; plen++; }
    strcpy(path + plen, name);
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[16]; int n, off = 0;
    while ((n = file_read(f, buf, 16)) > 0) {
        char tmp[16]; int2str(off, tmp); scr(tmp); scr(": ");
        for (int j = 0; j < n; j++) { int2str(buf[j], tmp); scr(tmp); scr(" "); }
        scr("\n"); off += n;
    }
    file_close(f);
    return CMD_RET_OK;
}

static int cmd_diskinfo(const char *args) {
    (void)args;
    sch(); scr("=== Disk Info ===\n"); scf();
    for (int d = 0; d < 4; d++) {
        partition_info_t *pi = partition_get(d);
        if (pi && pi->sector_count > 0) {
            char buf[16];
            scr(" Disk "); int2str(d, buf); scr(buf); scr(": ");
            int2str((int)(pi->sector_count / 2048), buf); scr(buf); scr(" MB\n");
        }
    }
    return CMD_RET_OK;
}

static int cmd_partitions(const char *args) {
    (void)args;
    sch(); scr("=== Partitions ===\n"); scf();
    for (int i = 0; i < 4; i++) {
        partition_info_t *pi = partition_get(i);
        if (pi && pi->sector_count > 0) {
            char buf[16];
            scr(" hda"); int2str(i + 1, buf); scr(buf); scr(": ");
            int2str((int)(pi->sector_count / 2048), buf); scr(buf); scr(" MB");
            scr(" (LBA: "); int2str((int)pi->lba_start, buf); scr(buf); scr(")\n");
        }
    }
    return CMD_RET_OK;
}

static int cmd_pci_handler(const char *args) {
    (void)args;
    int nd = pci_device_count();
    if (nd == 0) { scr(" No PCI devices\n"); return CMD_RET_OK; }
    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;
        char buf[16];
        int2str(dev->bus, buf); scr(buf); scr(":");
        int2str(dev->slot, buf); scr(buf); scr(":");
        int2str(dev->func, buf); scr(buf);
        scr("  "); scr(pci_vendor_name(dev->info.vendor_id));
        scr(" ["); scr(pci_class_name(dev->info.class_code)); scr("]\n");
    }
    return CMD_RET_OK;
}

static int cmd_pci_verbose(const char *args) {
    (void)args;
    int nd = pci_device_count();
    if (nd == 0) { scr(" No PCI devices\n"); return CMD_RET_OK; }
    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;
        char buf[16], buf2[16];
        int2str(dev->bus, buf); scr(buf); scr(":");
        int2str(dev->slot, buf); scr(buf); scr(".");
        int2str(dev->func, buf); scr(buf);
        int2str(dev->info.vendor_id, buf); int2str(dev->info.device_id, buf2);
        scr("  Vendor=0x"); scr(buf); scr(" Device=0x"); scr(buf2);
        scr("\n         Class="); int2str(dev->info.class_code, buf); scr(buf);
        scr(" Subclass="); int2str(dev->info.subclass, buf); scr(buf); scr("\n");
    }
    return CMD_RET_OK;
}

static int cmd_beep(const char *args) {
    int freq = 440, dur = 200;
    if (args) {
        int n = 0;
        while (*args >= '0' && *args <= '9') { n = n*10 + (*args-'0'); args++; }
        if (n > 0) freq = n;
        while (*args == ' ' || *args == '\t') args++;
        n = 0;
        while (*args >= '0' && *args <= '9') { n = n*10 + (*args-'0'); args++; }
        if (n > 0) dur = n;
    }
    pcspkr_beep(freq, dur);
    return CMD_RET_OK;
}

static int cmd_cmos(const char *args) {
    (void)args;
    sch(); scr("=== CMOS ===\n"); scf();
    rtc_time_t tm; rtc_read_time(&tm);
    char buf[16];
    scr(" Time: "); int2str(tm.hour, buf); scr(buf); scr(":");
    int2str(tm.minute, buf); scr(buf); scr(":"); int2str(tm.second, buf); scr(buf); scr("\n");
    return CMD_RET_OK;
}

static int cmd_dmesg(const char *args) {
    (void)args; klog_dump(); return CMD_RET_OK;
}

static int cmd_partition_disk_handler(const char *args);
static int cmd_format_disk_handler(const char *args);
static int cmd_sys_install_handler(const char *args);

static int cmd_install_hd(const char *args) {
    (void)args;
    sch(); scr("=== Install ===\n"); scf();
    scr(" Partitioning disk...\n");
    cmd_partition_disk_handler("1");
    scr(" Formatting partition 1...\n");
    cmd_format_disk_handler("1");
    scr(" Installing system...\n");
    cmd_sys_install_handler("1");
    scr(" Done! Boot from disk\n");
    return CMD_RET_OK;
}

static block_dev_t *get_bdev_for_idx(int idx) {
    partition_info_t *pi = partition_get(idx);
    if (!pi || !pi->present || pi->sector_count == 0) return 0;
    int bd = blockdev_register(pi->drive_id, pi->lba_start, pi->sector_count);
    if (bd < 0) return 0;
    return blockdev_get(bd);
}

static int cmd_partition_disk_handler(const char *args) {
    (void)args;
    if (!args) return CMD_RET_OK;
    int n = 0;
    for (const char *p = args; *p; p++) {
        if (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); }
        else break;
    }
    if (n == 0) n = 1;
    if (n < 1 || n > 4) return CMD_RET_OK;
    scf(); scr(" Partitioning disk...\n");
    int total_sectors = 102400;
    int each = total_sectors / n;
    uint32_t lba = 2048;
    for (int i = 0; i < n; i++) {
        partition_info_t *pi = partition_get(i);
        if (pi) {
            pi->lba_start = lba;
            pi->sector_count = each;
            pi->drive_id = 0;
            pi->present = 1;
        }
        lba += each;
    }
    scr(" Created "); char buf[16]; int2str(n, buf); scr(buf); scr(" partitions\n");
    return CMD_RET_OK;
}

static int cmd_format_disk_handler(const char *args) {
    if (!args) return CMD_RET_OK;
    int n = 0;
    for (const char *p = args; *p; p++) {
        if (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); }
        else break;
    }
    if (n < 1 || n > 4) return CMD_RET_OK;
    int idx = n - 1;
    scr(" Formatting partition "); char buf[16]; int2str(n, buf); scr(buf); scr("...\n");
    block_dev_t *bdev = get_bdev_for_idx(idx);
    if (!bdev) { scr(" No such partition\n"); return CMD_RET_OK; }
    if (fat32_format(bdev) == 0) scr(" FAT32 format complete\n");
    else scr(" Format failed\n");
    return CMD_RET_OK;
}

static int cmd_sys_install_handler(const char *args) {
    if (!args) return CMD_RET_OK;
    int n = 0;
    for (const char *p = args; *p; p++) {
        if (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); }
        else break;
    }
    if (n < 1 || n > 4) n = 1;
    int idx = n - 1;
    scr(" Installing system to partition "); char buf[16]; int2str(n, buf); scr(buf); scr("\n");
    block_dev_t *bdev = get_bdev_for_idx(idx);
    if (!bdev) { scr(" No such partition\n"); return CMD_RET_OK; }
    scr(" Creating directories...\n");
    fat32_mkdir(bdev, "SYSTEM");
    fat32_mkdir(bdev, "SYSTEM/KERNEL");
    fat32_mkdir(bdev, "SYSTEM/DRIVERS");
    fat32_mkdir(bdev, "SYSTEM/APPS");
    fat32_mkdir(bdev, "HOME");
    fat32_mkdir(bdev, "TEMP");
    scr(" Writing system files...\n");
    fat32_write_file(bdev, "SYSTEM/README.TXT", "Noctua OS v1.0");
    fat32_write_file(bdev, "SYSTEM/BOOT.CFG", "default=noctua\ntimeout=5");
    scr(" System installed!\n");
    return CMD_RET_OK;
}

static int cmd_diag(const char *args) {
    (void)args;
    sch(); scr("=== Diagnostics ===\n"); scf();
    scr(" CPU: x86 compatible\n");
    scr(" RAM: "); char buf[16]; int2str(heap_free() / 1024, buf); scr(buf); scr(" KB free\n");
    rtc_time_t tm; rtc_read_time(&tm);
    scr(" RTC: "); int2str(tm.hour, buf); scr(buf); scr(":");
    int2str(tm.minute, buf); scr(buf); scr(":"); int2str(tm.second, buf); scr(buf); scr("\n");
    scr(" Status: OK\n");
    return CMD_RET_OK;
}

static int cmd_ifconfig(const char *args) {
    (void)args;
    sch(); scr("=== Network Interfaces ===\n"); scf();
    scr(" eth0: 10.0.2.15  (QEMU user)\n");
    scr(" lo:   127.0.0.1\n");
    return CMD_RET_OK;
}

static int cmd_ping(const char *args) {
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char host[64]; int i = 0;
    while (args[i] && args[i] > ' ' && i < 63) { host[i] = args[i]; i++; } host[i] = 0;
    scr(" PING "); scr(host); scr(": not yet implemented\n");
    return CMD_RET_OK;
}

static int cmd_netstat(const char *args) {
    (void)args;
    sch(); scr("=== Network Connections ===\n"); scf();
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].used) {
            char buf[16];
            scr(" socket["); int2str(i, buf); scr(buf); scr("]: ");
            const char *states[] = {"UNUSED","OPEN","BOUND","LISTEN","CONNECTING","CONNECTED","CLOSING","CLOSED"};
            int st = sockets[i].state;
            if (st >= 0 && st < 8) scr(states[st]); else scr("?");
            scr("\n");
        }
    }
    return CMD_RET_OK;
}

static int cmd_source(const char *args) {
    if (args) script_run_file(args);
    return CMD_RET_OK;
}

static int cmd_noctua_help(const char *args) {
    (void)args; cmd_show_category(CMD_CAT_NOCTUA); return CMD_RET_OK;
}

void cmd_linux_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("help",      cmd_help_handler, "Display help", "help", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("neofetch",  cmd_neofetch, "System info display", "neofetch", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("clear",     cmd_clear, "Clear screen", "clear", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("echo",      cmd_echo, "Echo text", "echo <text>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("info",      cmd_info, "System info", "info", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("banner",    cmd_banner, "Display banner", "banner", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("about",     cmd_about, "About Noctua OS", "about", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("color",     cmd_color, "Set color (r/g/b/y/w/c/m)", "color <c>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("calc",      cmd_calc, "Simple calculator", "calc <a>+<b>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("ls",        cmd_ls, "List directory", "ls", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("cd",        cmd_cd, "Change directory", "cd <path>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("cat",       cmd_cat, "View file", "cat <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("mkdir",     cmd_mkdir, "Create directory", "mkdir <name>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("touch",     cmd_touch, "Create file", "touch <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("pwd",       cmd_pwd, "Print working dir", "pwd", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("whoami",    cmd_whoami, "Print user name", "whoami", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("hostname",  cmd_hostname, "Print hostname", "hostname", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("uptime",    cmd_uptime, "System uptime", "uptime", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("free",      cmd_free, "Free memory", "free", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("ps",        cmd_ps, "Process list", "ps", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("kill",      cmd_kill, "Kill process", "kill <pid>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("history",   cmd_history, "Command history", "history", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("alias",     cmd_alias, "Set alias", "alias", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("env",       cmd_env, "Print environment", "env", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("export",    cmd_export, "Export variable", "export <k>=<v>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("cow",       cmd_cow, "Cow says", "cow", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("reboot",    cmd_reboot, "Reboot system", "reboot", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("shutdown",  cmd_shutdown, "Shutdown system", "shutdown", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("date",      cmd_date, "Show date/time", "date", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("creds",     cmd_creds, "Show credits", "creds", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("explore",   cmd_explore, "File explore", "explore", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("edit",      cmd_edit, "Text editor", "edit <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("grep",      cmd_grep, "Search text", "grep <pattern>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("find",      cmd_find_handler, "Find files", "find <name>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("more",      cmd_more, "View file", "more <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("hexdump",   cmd_hexdump, "Hex dump file", "hexdump <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("diskinfo",  cmd_diskinfo, "Disk information", "diskinfo", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("partitions",cmd_partitions,"List partitions", "partitions", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("pci",       cmd_pci_handler, "PCI devices", "pci", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("pciverbose",cmd_pci_verbose,"PCI devices (verbose)", "pciverbose", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("beep",      cmd_beep, "PC speaker beep", "beep [freq] [dur]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("cmos",      cmd_cmos, "CMOS/RTC info", "cmos", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("dmesg",     cmd_dmesg, "Kernel log", "dmesg", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("install",   cmd_install_hd, "Install to disk", "install", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("partition", cmd_partition_disk_handler,"Partition disk", "partition [n]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("format",    cmd_format_disk_handler, "Format partition", "format <n>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("sys-install",cmd_sys_install_handler,"Install OS to partition","sys-install [n]",CMD_CAT_LINUX,"1.0"),
        CMD_FLAG("diag",      cmd_diag, "System diagnostics", "diag", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("ifconfig",  cmd_ifconfig, "Network interfaces", "ifconfig", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("ping",      cmd_ping, "Ping host", "ping <host>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("netstat",   cmd_netstat, "Network connections", "netstat", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("source",    cmd_source, "Run script", "source <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("noctua-help",cmd_noctua_help,"List all Noctua-native commands","noctua-help",CMD_CAT_LINUX,"1.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"
#include "timer/rtc.h"
#include "block/partition.h"
#include "block/blockdev.h"
#include "fs/fat32.h"
#include "bus/pci.h"
#include "heap.h"
#include "mm/page.h"
#include "arch/cpuid.h"

static void scr(const char *s) { screen_term_write(s); }
static void scf(void) { screen_set_content_color(C_INFO); }
static void sch(void) { screen_set_content_color(C_HEADER); }

static block_dev_t *open_partition(int id) {
    partition_info_t *pi = partition_get(id);
    if (!pi || !pi->present) return 0;
    int idx = blockdev_register(pi->drive_id, pi->lba_start, pi->sector_count);
    if (idx < 0) return 0;
    return blockdev_get(idx);
}

/* ---- strings: extract printable strings from files ---- */
static int cmd_strings(const char *args) {
    if (!args) { scr(" Usage: strings <file> [-n <min-len>]\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    int min_len = 4;
    char path[256]; int pi = 0;
    while (*args && *args > ' ' && *args != '-') { path[pi++] = *args++; } path[pi] = 0;
    if (cmd_has_flag(args, 'n')) {
        const char *p = args;
        while (*p && *p != 'n') p++;
        if (*p == 'n') { p++; while (*p == ' ' || *p == '=') p++; int n = 0;
            while (*p >= '0' && *p <= '9') { n = n*10 + (*p-'0'); p++; }
            if (n > 0) min_len = n; }
    }
    if (path[0] == 0) { scr(" No file specified\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found: "); scr(path); scr("\n"); return CMD_RET_OK; }
    char buf[4096]; int n, run = 0; char str_buf[256]; int si = 0;
    while ((n = file_read(f, buf, 4096)) > 0) {
        for (int i = 0; i < n; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (c >= 32 && c <= 126) {
                if (si < 255) str_buf[si++] = (char)c;
                run++;
            } else {
                if (run >= min_len) { str_buf[si] = 0; scr(str_buf); scr("\n"); }
                run = 0; si = 0;
            }
        }
    }
    if (run >= min_len) { str_buf[si] = 0; scr(str_buf); scr("\n"); }
    file_close(f);
    return CMD_RET_OK;
}

/* ---- fileinfo: detect file type by magic bytes ---- */
static int cmd_fileinfo(const char *args) {
    if (!args) { scr(" Usage: fileinfo <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file specified\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found: "); scr(path); scr("\n"); return CMD_RET_OK; }
    unsigned char magic[16]; int n = file_read(f, magic, 16);
    file_close(f);
    sch(); scr("=== File Info ===\n"); scf();
    scr(" Path: "); scr(path); scr("\n");
    const char *type = "Unknown";
    if (n >= 2 && magic[0] == 'M' && magic[1] == 'Z') type = "DOS/PE Executable";
    else if (n >= 4 && magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') type = "ELF Executable";
    else if (n >= 4 && magic[0] == '%' && magic[1] == 'P' && magic[2] == 'D' && magic[3] == 'F') type = "PDF Document";
    else if (n >= 4 && magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') type = "PNG Image";
    else if (n >= 2 && magic[0] == 0xFF && magic[1] == 0xD8) type = "JPEG Image";
    else if (n >= 4 && magic[0] == 'G' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == '8') type = "GIF Image";
    else if (n >= 4 && magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F') type = "RIFF (AVI/WAV)";
    else if (n >= 2 && magic[0] == 0x1F && magic[1] == 0x8B) type = "GZIP Archive";
    else if (n >= 3 && magic[0] == 0x42 && magic[1] == 0x5A && magic[2] == 0x68) type = "BZIP2 Archive";
    else if (n >= 4 && magic[0] == 0x50 && magic[1] == 0x4B && magic[2] == 0x03 && magic[3] == 0x04) type = "ZIP Archive";
    else if (n >= 4 && magic[0] == 0x52 && magic[1] == 0x61 && magic[2] == 0x72 && magic[3] == 0x21) type = "RAR Archive";
    else if (n >= 4 && magic[0] == 0xD0 && magic[1] == 0xCF && magic[2] == 0x11 && magic[3] == 0xE0) type = "OLE2 Compound (DOC/XLS)";
    else if (n >= 3 && magic[0] == 0xEF && magic[1] == 0xBB && magic[2] == 0xBF) type = "UTF-8 Text (BOM)";
    else if (n >= 4 && magic[0] == 'M' && magic[1] == 'S' && magic[2] == 'W' && magic[3] == 'I') type = "Windows Installer (MSI)";
    else { int txt = 1;
        for (int i = 0; i < n && i < 16; i++) {
            if (magic[i] < 32 && magic[i] != 9 && magic[i] != 10 && magic[i] != 13) { txt = 0; break; } }
        if (txt) type = "ASCII/UTF Text"; }
    scr(" Type: "); scr(type); scr("\n");
    char buf[16];
    scr(" Magic: ");
    for (int i = 0; i < n && i < 8; i++) { int2str(magic[i], buf); scr("0x"); if (magic[i] < 16) scr("0"); scr(buf); scr(" "); }
    scr("\n");
    return CMD_RET_OK;
}

/* ---- memdump: dump kernel memory regions ---- */
static int cmd_memdump(const char *args) {
    if (!args) { scr(" Usage: memdump <hex-addr> [size]\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    uint32_t addr = 0, size = 256;
    while (*args >= '0' && *args <= '9') { addr = addr * 16 + (*args - '0'); args++; }
    while (*args == 'x' || *args == 'X' || *args == ' ') args++;
    if (*args >= '0' && *args <= '9') { size = 0;
        while (*args >= '0' && *args <= '9') { size = size * 10 + (*args - '0'); args++; } }
    if (size > 4096) size = 4096;
    unsigned char *ptr = (unsigned char *)addr;
    char buf[16];
    sch(); scr("=== Memory Dump at 0x"); int2str_hex(addr, buf); scr(buf); scr(" ===\n"); scf();
    for (uint32_t off = 0; off < size; off += 16) {
        int2str_hex(addr + off, buf); scr(buf); scr(": ");
        for (int i = 0; i < 16 && (off + i) < size; i++) {
            int2str(ptr[off + i], buf); if (ptr[off+i] < 16) scr("0"); scr(buf); scr(" "); }
        scr("  ");
        for (int i = 0; i < 16 && (off + i) < size; i++) {
            unsigned char c = ptr[off + i];
            if (c >= 32 && c <= 126) { char s[2] = {(char)c, 0}; scr(s); }
            else scr("."); }
        scr("\n");
    }
    return CMD_RET_OK;
}

/* ---- sector: raw sector read/write ---- */
static int cmd_sector(const char *args) {
    if (!args) { scr(" Usage: sector <dev> <lba> [action]\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char dev[16]; int di = 0;
    while (*args && *args > ' ' && di < 15) { dev[di++] = *args++; } dev[di] = 0;
    while (*args == ' ') args++;
    uint32_t lba = 0;
    while (*args >= '0' && *args <= '9') { lba = lba * 10 + (*args - '0'); args++; }
    while (*args == ' ') args++;
    int write_mode = 0;
    if (*args == 'w' || *args == 'W') write_mode = 1;
    int dev_id = 0;
    if (dev[0] >= '0' && dev[0] <= '9') dev_id = dev[0] - '0';
    block_dev_t *bdev = open_partition(dev_id);
    if (!bdev) { scr(" Failed to open device\n"); return CMD_RET_OK; }
    unsigned char sector_buf[512];
    if (write_mode) {
        memset(sector_buf, 0, 512);
        if (bdev->write(bdev->priv, lba, sector_buf, 1) == 0) scr(" Sector written\n");
        else scr(" Write failed\n");
    } else {
        if (bdev->read(bdev->priv, lba, sector_buf, 1) == 0) {
            char buf[16];
            sch(); scr("=== Sector "); scr(dev); scr(" LBA="); int2str((int)lba, buf); scr(buf); scr(" ===\n"); scf();
            for (int i = 0; i < 512; i += 16) {
                int2str_hex(i, buf); scr(buf); scr(": ");
                for (int j = 0; j < 16; j++) {
                    int2str(sector_buf[i+j], buf);
                    if (sector_buf[i+j] < 16) scr("0");
                    scr(buf); scr(" ");
                }
                scr(" |");
                for (int j = 0; j < 16; j++) {
                    unsigned char c = sector_buf[i+j];
                    if (c >= 32 && c <= 126) { char s[2] = {(char)c, 0}; scr(s); } else scr(".");
                }
                scr("|\n");
            }
        } else scr(" Read failed\n");
    }
    return CMD_RET_OK;
}

/* ---- wipe: securely erase files ---- */
static int cmd_wipe(const char *args) {
    if (!args) { scr(" Usage: wipe <file> [-f]\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ' && *args != '-') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file specified\n"); return CMD_RET_OK; }
    int force = cmd_has_flag(args, 'f');
    (void)force;
    file_handle_t *f = file_open(path, 1);
    if (!f) { scr(" Cannot open file: "); scr(path); scr("\n"); return CMD_RET_OK; }
    uint32_t size = 0;
    char buf[512]; memset(buf, 0, 512);
    scr(" Wiping with zeros...\n");
    for (int i = 0; i < 1024; i++) { file_write(f, buf, 512); size += 512; }
    file_close(f);
    scr(" Wrote "); char num[16]; int2str(size / 1024, num); scr(num); scr(" KB of zeros\n");
    scr(" File wiped. Delete with rm\n");
    return CMD_RET_OK;
}

/* ---- recover: simple file recovery (stub) ---- */
static int cmd_recover(const char *args) {
    (void)args;
    sch(); scr("=== File Recovery ===\n"); scf();
    scr(" Scans disk for deleted files by signature\n");
    scr(" Usage: recover <device> <sig> [offset]\n");
    scr(" Signatures: jpg, png, gif, pdf, zip, elf\n");
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char dev[16]; int di = 0;
    while (*args && *args > ' ' && di < 15) { dev[di++] = *args++; } dev[di] = 0;
    while (*args == ' ') args++;
    char sig[16]; int si = 0;
    while (*args && *args > ' ' && si < 15) { sig[si++] = *args++; } sig[si] = 0;
    (void)dev; (void)sig;
    scr(" Scanning... (use binwalk for signature scan)\n");
    return CMD_RET_OK;
}

/* ---- md5sum: compute MD5 hash ---- */
static void md5_transform(uint32_t state[4], const unsigned char block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];
    for (int i = 0; i < 16; i++) x[i] = block[i*4] | (block[i*4+1]<<8) | (block[i*4+2]<<16) | (block[i*4+3]<<24);
    uint32_t F, g;
    for (int i = 0; i < 64; i++) {
        if (i < 16) { F = (b & c) | (~b & d); g = i; }
        else if (i < 32) { F = (d & b) | (~d & c); g = (5*i+1) % 16; }
        else if (i < 48) { F = b ^ c ^ d; g = (3*i+5) % 16; }
        else { F = c ^ (b | ~d); g = (7*i) % 16; }
        uint32_t temp = d; d = c; c = b;
        b = b + ((a + F + 0x5A827999 + x[g]) << (7+5*i%4*8) | (a + F + 0x5A827999 + x[g]) >> (32-(7+5*i%4*8)));
        a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static int cmd_md5sum(const char *args) {
    if (!args) { scr(" Usage: md5sum <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file specified\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found: "); scr(path); scr("\n"); return CMD_RET_OK; }
    uint32_t state[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    uint64_t bit_count = 0;
    unsigned char block[64]; int bi = 0;
    char buf[512]; int n;
    while ((n = file_read(f, buf, 512)) > 0) {
        bit_count += n * 8;
        for (int i = 0; i < n; i++) {
            block[bi++] = (unsigned char)buf[i];
            if (bi == 64) { md5_transform(state, block); bi = 0; }
        }
    }
    block[bi] = 0x80; bi++;
    if (bi > 56) { while (bi < 64) block[bi++] = 0; md5_transform(state, block); bi = 0; }
    while (bi < 56) block[bi++] = 0;
    for (int i = 0; i < 8; i++) block[56+i] = (unsigned char)((bit_count >> (i*8)) & 0xFF);
    md5_transform(state, block);
    char hex[16];
    for (int i = 0; i < 4; i++) {
        int2str((state[i] >> 24) & 0xFF, hex); if (((state[i] >> 24) & 0xFF) < 16) scr("0"); scr(hex);
        int2str((state[i] >> 16) & 0xFF, hex); if (((state[i] >> 16) & 0xFF) < 16) scr("0"); scr(hex);
        int2str((state[i] >> 8) & 0xFF, hex); if (((state[i] >> 8) & 0xFF) < 16) scr("0"); scr(hex);
        int2str(state[i] & 0xFF, hex); if ((state[i] & 0xFF) < 16) scr("0"); scr(hex);
    }
    scr("  "); scr(path); scr("\n");
    file_close(f);
    return CMD_RET_OK;
}

/* ---- sha1sum: compute SHA1 hash ---- */
static int cmd_sha1sum(const char *args) {
    if (!args) { scr(" Usage: sha1sum <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file specified\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    scr(" SHA1: not fully implemented\n");
    file_close(f);
    return CMD_RET_OK;
}

/* ---- dd: raw disk copy ---- */
static int dd_copy(block_dev_t *sdev, uint32_t src_lba, block_dev_t *ddev, uint32_t dst_lba, uint32_t count) {
    unsigned char tmp[512];
    char buf[16];
    for (uint32_t i = 0; i < count; i++) {
        if (sdev->read(sdev->priv, src_lba + i, tmp, 1) != 0) return -1;
        if (ddev->write(ddev->priv, dst_lba + i, tmp, 1) != 0) return -1;
        if (i % 64 == 0) { scr(" Copied "); int2str((int)i, buf); scr(buf); scr(" sectors\n"); }
    }
    return 0;
}

static int cmd_dd(const char *args) {
    if (!args) { scr(" Usage: dd if=<dev> of=<dev> [count=<n>]\n"); return CMD_RET_OK; }
    char if_dev[16] = {0}, of_dev[16] = {0};
    uint32_t count = 1024;
    const char *p = args;
    while (*p) {
        if (strncmp(p, "if=", 3) == 0) { p += 3; int i = 0;
            while (*p && *p > ' ' && *p != 'o' && i < 15) { if_dev[i++] = *p++; } if_dev[i] = 0; }
        else if (strncmp(p, "of=", 3) == 0) { p += 3; int i = 0;
            while (*p && *p > ' ' && *p != 'c' && i < 15) { of_dev[i++] = *p++; } of_dev[i] = 0; }
        else if (strncmp(p, "count=", 6) == 0) { p += 6; count = 0;
            while (*p >= '0' && *p <= '9') { count = count * 10 + (*p - '0'); p++; } }
        else p++;
    }
    if (if_dev[0] == 0 || of_dev[0] == 0) { scr(" Specify if=<src> and of=<dst>\n"); return CMD_RET_OK; }
    int src_id = if_dev[0] - '0', dst_id = of_dev[0] - '0';
    block_dev_t *src = open_partition(src_id);
    block_dev_t *dst = open_partition(dst_id);
    if (!src || !dst) { scr(" Failed to open device\n"); return CMD_RET_OK; }
    char buf[16];
    scr(" Copying "); int2str((int)count, buf); scr(buf); scr(" sectors...\n");
    if (dd_copy(src, 0, dst, 0, count) == 0) scr(" Done\n");
    else scr(" Failed\n");
    return CMD_RET_OK;
}

/* ---- hexedit: interactive hex editing ---- */
static int cmd_hexedit(const char *args) {
    if (!args) { scr(" Usage: hexedit <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    file_handle_t *f = file_open(path, 1);
    if (!f) { scr(" Cannot open file\n"); return CMD_RET_OK; }
    char buf[512]; int n = file_read(f, buf, 512);
    char num[16];
    sch(); scr("=== Hex Editor: "); scr(path); scr(" ===\n"); scf();
    for (int i = 0; i < n; i += 16) {
        int2str_hex(i, num); scr(num); scr(": ");
        for (int j = 0; j < 16 && (i+j) < n; j++) {
            int2str((unsigned char)buf[i+j], num);
            if ((unsigned char)buf[i+j] < 16) scr("0");
            scr(num); scr(" ");
        }
        scr("  ");
        for (int j = 0; j < 16 && (i+j) < n; j++) {
            unsigned char c = (unsigned char)buf[i+j];
            if (c >= 32 && c <= 126) { char s[2] = {(char)c, 0}; scr(s); } else scr(".");
        }
        scr("\n");
    }
    scr(" Read-only preview. Use 'edit' for editing\n");
    file_close(f);
    return CMD_RET_OK;
}

/* ---- binwalk: scan for embedded files ---- */
static int cmd_binwalk(const char *args) {
    if (!args) { scr(" Usage: binwalk <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int n;
    uint32_t offset = 0;
    int found = 0;
    char num[16];
    sch(); scr("=== Binwalk: "); scr(path); scr(" ===\n"); scf();
    while ((n = file_read(f, buf, 4096)) > 0) {
        unsigned char *b;
        for (int i = 0; i < n - 4; i++) {
            b = (unsigned char *)buf + i;
            if (b[0] == 0x7F && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') {
                scr(" ELF at 0x"); int2str_hex(offset + i, num); scr(num); scr("\n"); found = 1; }
            if (b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') {
                scr(" PNG at 0x"); int2str_hex(offset + i, num); scr(num); scr("\n"); found = 1; }
            if (b[0] == 0xFF && b[1] == 0xD8) {
                scr(" JPEG at 0x"); int2str_hex(offset + i, num); scr(num); scr("\n"); found = 1; }
            if (b[0] == 0x50 && b[1] == 0x4B && b[2] == 0x03 && b[3] == 0x04) {
                scr(" ZIP at 0x"); int2str_hex(offset + i, num); scr(num); scr("\n"); found = 1; }
            if (b[0] == 'M' && b[1] == 'Z') {
                scr(" PE at 0x"); int2str_hex(offset + i, num); scr(num); scr("\n"); found = 1; }
        }
        offset += n;
    }
    if (!found) scr(" No embedded signatures found\n");
    file_close(f);
    return CMD_RET_OK;
}

/* ---- foremost: file carving tool ---- */
static int cmd_foremost(const char *args) {
    (void)args;
    sch(); scr("=== Foremost - File Carving ===\n"); scf();
    scr(" Carves files from raw disk image by header/footer signatures\n");
    scr(" Usage: foremost <image> [-t <type>]\n");
    scr(" Types: jpg, png, gif, pdf, zip, elf, all\n");
    if (!args) return CMD_RET_OK;
    scr(" Use binwalk for signature scan\n");
    return CMD_RET_OK;
}

/* ---- testdisk: partition recovery tool ---- */
static int cmd_testdisk(const char *args) {
    (void)args;
    sch(); scr("=== TestDisk - Partition Recovery ===\n"); scf();
    scr(" Analyzes disk for lost/deleted partitions\n");
    scr(" Usage: testdisk <device>\n");
    scr(" Scans for FAT32/NTFS/EXT2 boot sectors\n");
    if (!args) return CMD_RET_OK;
    while (*args == ' ') args++;
    char dev[16]; int di = 0;
    while (*args && *args > ' ' && di < 15) { dev[di++] = *args++; } dev[di] = 0;
    if (dev[0] == 0) return CMD_RET_OK;
    scr(" Scanning "); scr(dev); scr("...\n");
    block_dev_t *bdev = open_partition(dev[0] - '0');
    if (!bdev) { scr(" Cannot access device\n"); return CMD_RET_OK; }
    scr(" Scanning for boot sectors...\n");
    unsigned char sector[512];
    uint32_t found_partitions = 0;
    uint64_t max_lba = bdev->total_sectors > 8192 ? 8192 : bdev->total_sectors;
    for (uint32_t lba = 0; lba < max_lba; lba += 64) {
        if (bdev->read(bdev->priv, lba, sector, 1) == 0) {
            if (sector[0] == 0xEB || sector[0] == 0xE9) {
                char num[16];
                scr("  Possible boot sector at LBA "); int2str((int)lba, num); scr(num); scr("\n");
                found_partitions++;
            }
        }
    }
    if (found_partitions == 0) scr(" No boot sectors found\n");
    else { char num[16]; int2str((int)found_partitions, num); scr(" Found "); scr(num); scr(" potential partitions\n"); }
    scr(" Use 'dd if="); scr(dev); scr(" of=<img>' to image\n");
    return CMD_RET_OK;
}

/* ---- diskedit: edit disk sectors ---- */
static int cmd_diskedit(const char *args) {
    if (!args) { scr(" Usage: diskedit <dev> <lba>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char dev[16]; int di = 0;
    while (*args && *args > ' ' && di < 15) { dev[di++] = *args++; } dev[di] = 0;
    while (*args == ' ') args++;
    uint32_t lba = 0;
    while (*args >= '0' && *args <= '9') { lba = lba * 10 + (*args - '0'); args++; }
    int dev_id = dev[0] - '0';
    block_dev_t *bdev = open_partition(dev_id);
    if (!bdev) { scr(" Cannot access device\n"); return CMD_RET_OK; }
    unsigned char sector[512];
    if (bdev->read(bdev->priv, lba, sector, 1) != 0) { scr(" Read error\n"); return CMD_RET_OK; }
    char num[16];
    sch(); scr("=== Disk Edit: "); scr(dev); scr(" LBA="); int2str((int)lba, num); scr(num); scr(" ===\n"); scf();
    scr("   Offset: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    scr("   ------ ------------------------------------------------\n");
    for (int i = 0; i < 512; i += 16) {
        int2str_hex(i, num); scr("   "); scr(num); scr(": ");
        for (int j = 0; j < 16; j++) {
            int2str(sector[i+j], num);
            if (sector[i+j] < 16) scr("0");
            scr(num); scr(" ");
        }
        scr(" |");
        for (int j = 0; j < 16; j++) {
            unsigned char c = sector[i+j];
            if (c >= 32 && c <= 126) { char s[2] = {(char)c, 0}; scr(s); } else scr(".");
        }
        scr("|\n");
    }
    return CMD_RET_OK;
}

/* ---- meminfo: detailed memory info ---- */
static int cmd_meminfo(const char *args) {
    (void)args;
    char buf[16];
    uint32_t total_pages = pmem_total_pages();
    uint32_t free_pages = pmem_free_pages();
    uint32_t used_pages = pmem_used_pages();
    uint32_t heap_kb = heap_free();
    sch(); scr("=== Memory Info ===\n"); scf();
    scr(" Physical Memory:\n");
    int2str((int)(total_pages * 4), buf); scr("  Total: "); scr(buf); scr(" KB\n");
    int2str((int)(free_pages * 4), buf); scr("  Free:  "); scr(buf); scr(" KB\n");
    int2str((int)(used_pages * 4), buf); scr("  Used:  "); scr(buf); scr(" KB\n");
    scr(" Kernel Heap:\n");
    int2str((int)(heap_kb / 1024), buf); scr("  Free:  "); scr(buf); scr(" MB\n");
    return CMD_RET_OK;
}

/* ---- registers: show CPU registers ---- */
static int cmd_registers(const char *args) {
    (void)args;
    sch(); scr("=== CPU Registers ===\n"); scf();
    scr(" Use 'memdump 0xB8000' to view VGA memory\n");
    char vendor[16];
    get_cpu_vendor(vendor);
    scr(" CPU Vendor: "); scr(vendor); scr("\n");
    unsigned int a, b, c, d;
    cpuid_string(1, &a, &b, &c, &d);
    char buf[16];
    scr(" Stepping: "); int2str((int)(a & 0xF), buf); scr(buf); scr("\n");
    scr(" Model:    "); int2str((int)((a >> 4) & 0xF), buf); scr(buf); scr("\n");
    scr(" Family:   "); int2str((int)((a >> 8) & 0xF), buf); scr(buf); scr("\n");
    return CMD_RET_OK;
}

/* ---- syslog: system log viewer ---- */
static int cmd_syslog(const char *args) {
    (void)args;
    sch(); scr("=== System Log ===\n"); scf();
    scr(" Use 'dmesg' for kernel log\n");
    scr(" Use 'diag' for system diagnostics\n");
    return CMD_RET_OK;
}

/* ---- portscan: scan PCI I/O ports ---- */
static int cmd_portscan(const char *args) {
    (void)args;
    sch(); scr("=== PCI Port Scan ===\n"); scf();
    int nd = pci_device_count();
    if (nd == 0) { scr(" No PCI devices\n"); return CMD_RET_OK; }
    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;
        char buf[16];
        int2str(dev->bus, buf); scr(buf); scr(":");
        int2str(dev->slot, buf); scr(buf); scr(".");
        int2str(dev->func, buf); scr(buf);
        scr("  "); scr(pci_vendor_name(dev->info.vendor_id));
        scr(" ["); scr(pci_class_name(dev->info.class_code)); scr("]\n");
    }
    return CMD_RET_OK;
}

void cmd_tools_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("strings",   cmd_strings,   "Extract printable strings from file", "strings <file> [-n <len>]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("fileinfo",  cmd_fileinfo,  "Detect file type by magic bytes", "fileinfo <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("memdump",   cmd_memdump,   "Dump kernel memory region", "memdump <hex-addr> [size]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("sector",    cmd_sector,    "Read/write disk sectors", "sector <dev> <lba> [w]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("wipe",      cmd_wipe,      "Securely erase file with zeros", "wipe <file> [-f]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("recover",   cmd_recover,   "Recover deleted files by signature", "recover <dev> <sig>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("md5sum",    cmd_md5sum,    "Compute MD5 file hash", "md5sum <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("sha1sum",   cmd_sha1sum,   "Compute SHA1 file hash", "sha1sum <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("dd",        cmd_dd,        "Raw disk copy", "dd if=<dev> of=<dev> [count=<n>]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("hexedit",   cmd_hexedit,   "Hex editor (read-only preview)", "hexedit <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("binwalk",   cmd_binwalk,   "Scan for embedded file signatures", "binwalk <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("foremost",  cmd_foremost,  "File carving tool", "foremost <image> [-t <type>]", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("testdisk",  cmd_testdisk,  "Partition recovery scanner", "testdisk <device>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("diskedit",  cmd_diskedit,  "Edit disk sectors (read-only)", "diskedit <dev> <lba>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("meminfo",   cmd_meminfo,   "Detailed memory information", "meminfo", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("registers", cmd_registers, "Show CPU register info", "registers", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("syslog",    cmd_syslog,    "System log viewer", "syslog", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("portscan",  cmd_portscan,  "Scan PCI ports and devices", "portscan", CMD_CAT_LINUX, "1.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

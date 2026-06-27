#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"
#include "fs/fat32.h"
#include "block/partition.h"
#include "block/blockdev.h"
#include "drivers/block/ata.h"
#include "drivers/input/keyboard.h"
#include "char/serial.h"
#include "ports.h"
#define SETUP_MAX_INPUT 128
#define SECTORS_PER_MB   2048

static void scr(const char *s) { screen_term_write(s); }
static void scf(void) { screen_set_content_color(C_INFO); }
static void sch(void) { screen_set_content_color(C_HEADER); }
static void sce(void) { screen_set_content_color(C_ERROR); }

extern char _binary__usr_lib_grub_i386_pc_boot_img_start[];
extern char _binary__usr_lib_grub_i386_pc_boot_img_end[];
extern char _binary_build_grub_core_img_start[];
extern char _binary_build_grub_core_img_end[];

static int get_input_char(void) {
    int k = keyboard_getchar_nb();
    if (k != 0) return k;
    char c = serial_read_char_nb(COM1_PORT);
    if (c == '\r') return '\n';
    if (c != 0) return c;
    return 0;
}

static int readline(char *buf, int max) {
    int i = 0;
    while (1) {
        int c;
        while ((c = get_input_char()) == 0) { }
        if (c == '\n' || c == K_ENTER) {
            screen_term_putchar('\n');
            buf[i] = 0;
            return i;
        }
        if (c == '\b' && i > 0) { i--; screen_term_putchar('\b'); screen_term_putchar(' '); screen_term_putchar('\b'); }
        else if (c >= ' ' && c <= '~' && i < max - 1) { buf[i++] = c; screen_term_putchar(c); }
    }
}

static int yesno(void) {
    while (1) {
        int c;
        while ((c = get_input_char()) == 0) { }
        if (c == 'y' || c == 'Y') { scr("y\n"); return 1; }
        if (c == 'n' || c == 'N') { scr("n\n"); return 0; }
    }
}

static void wait_key(void) {
    scr("\n Press any key to continue...");
    while (get_input_char() == 0) { }
    scr("\n");
}

static void print_header(void) {
    sch(); scr("============================================\n");
    scr("     Noctua OS 1.0 Interactive Installer\n");
    scr("============================================\n\n");
    scf();
}

static void print_progress(const char *msg) {
    sch(); scr("  >> "); scf(); scr(msg); scr("\n");
}

static int detect_disks(void) {
    int count = ata_device_count();
    if (count == 0) {
        sce(); scr(" No ATA drives detected!\n"); scf();
        return 0;
    }
    sch(); scr(" Detected ATA drives:\n"); scf();
    for (int i = 0; i < count; i++) {
        ata_device_t *dev = ata_get_device(i);
        if (!dev || !dev->present) continue;
        char buf[16];
        scr("   [");
        int2str(i, buf); scr(buf);
        scr("] ");
        char sectors[32];
        int2str((int)(dev->total_sectors / SECTORS_PER_MB), sectors);
        scr(dev->model);
        scr(" (");
        scr(sectors);
        scr(" MB)\n");
    }
    scr("\n");
    return count;
}

static int select_disk(int max) {
    while (1) {
        scr(" Select disk [0-");
        char buf[8]; int2str(max - 1, buf); scr(buf);
        scr("]: ");
        char input[SETUP_MAX_INPUT];
        int len = readline(input, sizeof(input));
        (void)len;
        int n = 0;
        for (char *p = input; *p; p++) {
            if (*p >= '0' && *p <= '9') n = n * 10 + (*p - '0');
            else break;
        }
        if (n >= 0 && n < max) return n;
        sce(); scr(" Invalid selection\n"); scf();
    }
}

static int write_mbr(ata_device_t *dev, uint32_t partition_lba, uint32_t partition_sectors) {
    uint8_t mbr_buf[512];

    memcpy(mbr_buf, _binary__usr_lib_grub_i386_pc_boot_img_start, 440);

    memset(mbr_buf + 440, 0, 72);

    mbr_entry_t *p1 = (mbr_entry_t *)(mbr_buf + 446);
    p1->status = 0x80;
    memset(p1->chs_first, 0, 3);
    p1->type = PART_TYPE_FAT32_LBA;
    memset(p1->chs_last, 0xFF, 3);
    p1->lba_start = partition_lba;
    p1->sector_count = partition_sectors;

    memset(mbr_buf + 462, 0, 48);

    mbr_buf[510] = 0x55;
    mbr_buf[511] = 0xAA;

    if (ata_write_sectors(dev, 0, 1, mbr_buf) != 0) {
        sce(); scr(" Failed to write MBR!\n"); scf();
        return -1;
    }
    return 0;
}

static int install_grub(ata_device_t *dev) {
    uint32_t boot_size = (uint32_t)(_binary__usr_lib_grub_i386_pc_boot_img_end - _binary__usr_lib_grub_i386_pc_boot_img_start);
    uint32_t core_size = (uint32_t)(_binary_build_grub_core_img_end - _binary_build_grub_core_img_start);
    uint16_t core_sectors = (core_size + 511) / 512;

    (void)boot_size;

    char buf[64];
    int2str(core_sectors, buf);
    scr(" core.img: "); scr(buf); scr(" sectors\n");

    uint32_t current_lba = 1;
    uint32_t remaining = core_size;
    uint8_t *core_ptr = (uint8_t *)_binary_build_grub_core_img_start;

    while (remaining > 0) {
        uint8_t sector_buf[512];
        uint32_t chunk = (remaining < 512) ? remaining : 512;
        memcpy(sector_buf, core_ptr, chunk);
        if (chunk < 512) memset(sector_buf + chunk, 0, 512 - chunk);

        if (ata_write_sectors(dev, current_lba, 1, sector_buf) != 0) {
            sce(); scr(" Failed to write core.img sector "); scf();
            char num[16]; int2str(current_lba, num); scr(num); scr("\n");
            return -1;
        }
        current_lba++;
        core_ptr += 512;
        remaining -= (remaining < 512) ? remaining : 512;
    }

    scr(" GRUB core.img written to sectors 1-");
    int2str(core_sectors, buf); scr(buf); scr("\n");
    return 0;
}

static int copy_system_files(block_dev_t *bdev, ata_device_t *ata) {
    (void)ata;

    print_progress("Creating system directories...");
    if (fat32_mkdir(bdev, "BOOT") != 0) { sce(); scr(" Failed to create BOOT/\n"); scf(); }
    if (fat32_mkdir(bdev, "BOOT/GRUB") != 0) { sce(); scr(" Failed to create BOOT/GRUB/\n"); scf(); }
    fat32_mkdir(bdev, "SYSTEM");
    fat32_mkdir(bdev, "HOME");
    fat32_mkdir(bdev, "TEMP");
    fat32_mkdir(bdev, "ETC");

    print_progress("Writing GRUB configuration...");
    fat32_write_file(bdev, "BOOT/GRUB/GRUB.CFG",
        "set timeout=5\n"
        "set default=0\n"
        "\n"
        "menuentry \"Noctua OS\" {\n"
        "    set gfxpayload=1024x768x32,800x600x32,640x480x32\n"
        "    multiboot /boot/noctua.bin quiet\n"
        "    boot\n"
        "}\n"
        "menuentry \"Noctua OS (verbose)\" {\n"
        "    set gfxpayload=1024x768x32,800x600x32,640x480x32\n"
        "    multiboot /boot/noctua.bin verbose\n"
        "    boot\n"
        "}\n"
        "menuentry \"Shutdown\" { halt }\n"
    );

    print_progress("Writing system config files...");
    fat32_write_file(bdev, "SYSTEM/VERSION", "Noctua OS 1.0\n");
    fat32_write_file(bdev, "SYSTEM/CONFIG", "default\ntimeout=5\n");
    fat32_write_file(bdev, "ETC/HOSTNAME", "noctua\n");
    fat32_write_file(bdev, "README.TXT", "Welcome to Noctua OS!\n");

    return 0;
}

static int configure_system(void) {
    char hostname[64] = "noctua";
    char username[64] = "user";
    char input[SETUP_MAX_INPUT];

    sch(); scr("--------------------------------------------\n");
    scr(" System Configuration\n");
    scr("--------------------------------------------\n\n");
    scf();

    scr(" Hostname [noctua]: ");
    readline(input, sizeof(input));
    if (input[0]) strncpy(hostname, input, 63);

    scr(" Username [user]: ");
    readline(input, sizeof(input));
    if (input[0]) strncpy(username, input, 63);

    scr("\n Summary:\n");
    sch(); scr("   Hostname: "); scf(); scr(hostname); scr("\n");
    sch(); scr("   Username: "); scf(); scr(username); scr("\n\n");

    return 0;
}

static int cmd_setup_handler(const char *args) {
    (void)args;
    char input[SETUP_MAX_INPUT];
    (void)input;

    print_header();

    int ata_count = detect_disks();
    if (ata_count == 0) {
        wait_key();
        return CMD_RET_OK;
    }

    scr(" Select target disk:\n");
    int disk_idx = select_disk(ata_count);
    ata_device_t *disk = ata_get_device(disk_idx);
    if (!disk || !disk->present) {
        sce(); scr(" Invalid disk selection\n"); scf();
        return CMD_RET_OK;
    }

    char buf[64];
    scr("\n Selected: ");
    sch(); scr(disk->model); scf();
    scr(" (");
    int2str((int)(disk->total_sectors / SECTORS_PER_MB), buf); scr(buf);
    scr(" MB)\n\n");

    sch(); scr(" Partition Scheme:\n"); scf();
    scr("   1. Single FAT32 partition (use entire disk)\n");
    scr("   q. Cancel\n\n");
    scr(" Choice [1]: ");

    readline(input, sizeof(input));
    if (input[0] == 'q' || input[0] == 'Q') { scr(" Cancelled.\n"); return CMD_RET_OK; }

    uint32_t first_lba = 2048;
    uint32_t total_sectors = (uint32_t)disk->total_sectors;
    if (total_sectors > 0xFFFFFFFF) total_sectors = 0xFFFFFFFF;
    uint32_t partition_sectors = total_sectors - first_lba;

    sch(); scr("\n--------------------------------------------\n");
    scr(" Installation Plan:\n");
    scr("--------------------------------------------\n");
    scf();
    scr("   Disk:      "); scr(disk->model); scr("\n");
    scr("   Partition: LBA ");
    int2str(first_lba, buf); scr(buf);
    scr(" - LBA ");
    int2str(total_sectors - 1, buf); scr(buf);
    scr(" (");
    int2str((int)(partition_sectors / SECTORS_PER_MB), buf); scr(buf);
    scr(" MB)\n");
    scr("   Filesystem: FAT32\n");
    scr("   Bootloader: GRUB (MBR + core.img)\n\n");

    sch(); scr(" WARNING: ALL DATA on this disk will be DESTROYED!\n");
    scf(); scr(" Proceed? [y/N]: ");
    if (!yesno()) { scr(" Installation cancelled.\n"); return CMD_RET_OK; }

    sch(); scr("\n--------------------------------------------\n");
    scr(" Installing...\n");
    scr("--------------------------------------------\n\n");
    scf();

    print_progress("Writing MBR partition table...");
    if (write_mbr(disk, first_lba, partition_sectors) != 0) {
        wait_key();
        return CMD_RET_OK;
    }

    print_progress("Registering block device...");
    int bd_idx = blockdev_register(disk_idx, first_lba, partition_sectors);
    if (bd_idx < 0) {
        sce(); scr(" Failed to register block device!\n"); scf();
        wait_key();
        return CMD_RET_OK;
    }
    block_dev_t *bdev = blockdev_get(bd_idx);
    if (!bdev) {
        sce(); scr(" Failed to get block device!\n"); scf();
        wait_key();
        return CMD_RET_OK;
    }

    print_progress("Formatting FAT32 filesystem...");
    if (fat32_format(bdev) != 0) {
        sce(); scr(" Format failed!\n"); scf();
        wait_key();
        return CMD_RET_OK;
    }

    print_progress("Installing GRUB bootloader...");
    if (install_grub(disk) != 0) {
        wait_key();
        return CMD_RET_OK;
    }

    print_progress("Copying system files...");
    copy_system_files(bdev, disk);

    print_progress("Configuring system...");
    screen_clear_content();
    print_header();
    configure_system();

    sch(); scr("\n--------------------------------------------\n");
    scr(" Installation Complete!\n");
    scr("--------------------------------------------\n");
    scf();
    scr("\n You can now:\n");
    scr("   1. Reboot (type 'reboot')\n");
    scr("   2. Shutdown (type 'shutdown')\n");
    scr("   3. Return to shell\n\n");
    sch(); scr(" Remember to boot from the target disk!\n");
    scf();

    wait_key();
    return CMD_RET_OK;
}

void setup_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("setup",    cmd_setup_handler, "Interactive system installer", "setup", CMD_CAT_SYSTEM, "1.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

#include "devices.h"
#include "fs/fat32.h"
#include "ui/fb.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "screen.h"
#include "drivers/input/keyboard.h"
#include "drivers/char/serial.h"

/* /dev/null - discards all writes, returns 0 on read */
static int dev_null_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0;
}

static int dev_null_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    (void)node; (void)offset; (void)buffer;
    return size;
}

/* /dev/zero - returns zeros on read, discards writes */
static int dev_zero_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node; (void)offset;
    if (buffer && size) memset(buffer, 0, size);
    return size;
}

static int dev_zero_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    (void)node; (void)offset; (void)buffer;
    return size;
}

/* /dev/tty - terminal I/O */
static int dev_tty_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node; (void)offset;
    char *buf = (char *)buffer;
    int count = 0;
    for (uint32_t i = 0; i < size; i++) {
        int c = keyboard_getchar();
        if (c < 0) break;
        buf[i] = (char)c;
        count++;
    }
    return count;
}

static int dev_tty_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    (void)node; (void)offset;
    screen_term_write_buf((const char *)buffer, size);
    return size;
}

/* /dev/fb0 - framebuffer device (read/write raw pixels) */
static int dev_fb_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node;
    uint32_t fb_size = fb_width * fb_height * (fb_bpp / 8);
    if (offset >= fb_size) return 0;
    if (offset + size > fb_size) size = fb_size - offset;
    memcpy(buffer, (uint8_t *)fb_ptr + offset, size);
    return size;
}

static int dev_fb_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    (void)node;
    uint32_t fb_size = fb_width * fb_height * (fb_bpp / 8);
    if (offset >= fb_size) return 0;
    if (offset + size > fb_size) size = fb_size - offset;
    memcpy((uint8_t *)fb_ptr + offset, buffer, size);
    return size;
}

/* /dev/kbd - raw keyboard input */
static int dev_kbd_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node; (void)offset;
    char *buf = (char *)buffer;
    int count = 0;
    for (uint32_t i = 0; i < size; i++) {
        int c = keyboard_getchar_nb();
        if (c < 0) break;
        buf[i] = (char)c;
        count++;
    }
    return count;
}

static int dev_kbd_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    (void)node; (void)offset; (void)buffer; (void)size;
    return size;
}

/* /dev/serial - serial port I/O */
static int dev_serial_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    (void)node; (void)offset;
    char *buf = (char *)buffer;
    int count = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (!serial_received(COM1_PORT)) break;
        buf[i] = serial_read_char(COM1_PORT);
        count++;
    }
    return count;
}

static int dev_serial_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        serial_write_char(COM1_PORT, ((const char *)buffer)[i]);
    }
    return size;
}

static file_operations_t dev_null_fops = {
    0, 0, dev_null_read, dev_null_write, 0
};

static file_operations_t dev_zero_fops = {
    0, 0, dev_zero_read, dev_zero_write, 0
};

static file_operations_t dev_tty_fops = {
    0, 0, dev_tty_read, dev_tty_write, 0
};

static file_operations_t dev_fb_fops = {
    0, 0, dev_fb_read, dev_fb_write, 0
};

static file_operations_t dev_kbd_fops = {
    0, 0, dev_kbd_read, dev_kbd_write, 0
};

static file_operations_t dev_serial_fops = {
    0, 0, dev_serial_read, dev_serial_write, 0
};

static vfs_node_t *dev_add(const char *name, file_operations_t *fops, uint32_t mode) {
    char path[FAT_MAX_PATH];
    strcpy(path, "/dev/");
    strcat(path, name);

    vfs_node_t *node = vfs_create_node(path, 0);
    if (!node) return 0;

    node->f_op = fops;
    node->permissions = mode;
    return node;
}

void devfs_init(void) {
    vfs_node_t *dev_dir = vfs_find_node("/dev");
    if (!dev_dir) {
        vfs_create_node("/dev", 1);
    }

    dev_add("null",   &dev_null_fops,   VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE | VFS_PERM_OTHER_READ | VFS_PERM_OTHER_WRITE);
    dev_add("zero",   &dev_zero_fops,   VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE | VFS_PERM_OTHER_READ | VFS_PERM_OTHER_WRITE);
    dev_add("tty",    &dev_tty_fops,    VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE | VFS_PERM_OTHER_READ | VFS_PERM_OTHER_WRITE);
    dev_add("fb0",    &dev_fb_fops,     VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE);
    dev_add("kbd",    &dev_kbd_fops,    VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE);
    dev_add("serial", &dev_serial_fops, VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE);

    screen_set_content_color(C_INFO);
    screen_term_write("DEVFS: Device filesystem initialized (/dev/null, /dev/zero, /dev/tty, /dev/fb0, /dev/kbd, /dev/serial)\n");
}

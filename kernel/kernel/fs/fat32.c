#include "fat32.h"
#include "blockdev.h"
#include "string.h"
#include "heap.h"

/* Static storage for VFS */
static vfs_node_t vfs_nodes[MAX_VFS_NODES];
static int vfs_node_count = 0;
static vfs_node_t *root_node = 0;

static dentry_t dentry_cache[MAX_DENTRY_CACHE];
static int dentry_count = 0;

static mount_t mount_table[MAX_MOUNTS];

/* File handles */
static file_handle_t file_handles[MAX_OPEN_FILES];
static int file_handle_count = 0;

/* FAT state */
static block_dev_t *fat_bdev = 0;
static fat_boot_sector_t boot_sector_copy;
static uint8_t *fat_table = 0;
static uint32_t fat_size = 0;
static uint32_t fat_sectors = 0;
static uint32_t data_start = 0;
static uint32_t reserved_sectors = 0;
static uint32_t num_fats = 0;
static uint32_t sectors_per_cluster = 0;
static uint32_t bytes_per_cluster = 0;
static int fat_type = FAT_TYPE_FAT32; /* Default to FAT32 */

static int fat_read_sectors(uint64_t lba, void *buffer, uint32_t count) {
    if (!fat_bdev) return -1;
    return fat_bdev->read(fat_bdev->priv, lba, buffer, count);
}

static int fat_read_sector(uint64_t lba, void *buffer) {
    return fat_read_sectors(lba, buffer, 1);
}

/* Detect FAT type from boot sector */
int fat_detect_type(fat_boot_sector_t *bs) {
    uint32_t total_sectors = bs->total_sectors_16 ? bs->total_sectors_16 : bs->total_sectors_32;
    uint32_t fat_size = bs->fat_size_16 ? bs->fat_size_16 : bs->fat_size_32;
    uint32_t root_dir_sectors = ((bs->root_entry_count * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;
    uint32_t data_sectors = total_sectors - (bs->reserved_sectors + (bs->num_fats * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / bs->sectors_per_cluster;

    if (cluster_count < 4085) {
        return FAT_TYPE_FAT12;
    } else if (cluster_count < 65525) {
        return FAT_TYPE_FAT16;
    } else {
        return FAT_TYPE_FAT32;
    }
}

/* Initialize FAT filesystem (generic for FAT12/16/32) */
int fat_init(struct block_dev *bdev) {
    fat_bdev = bdev;

    root_node = &vfs_nodes[vfs_node_count++];
    strcpy(root_node->name, "root");
    root_node->size = 0;
    root_node->is_directory = 1;
    root_node->attributes = FAT_ATTR_DIRECTORY;
    root_node->permissions = VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE | VFS_PERM_OWNER_EXEC | VFS_PERM_GROUP_READ | VFS_PERM_GROUP_EXEC | VFS_PERM_OTHER_READ | VFS_PERM_OTHER_EXEC;
    root_node->uid = 0;
    root_node->gid = 0;
    root_node->create_time = 0;
    root_node->modify_time = 0;
    root_node->parent = 0;
    root_node->children = 0;
    root_node->next = 0;
    root_node->first_cluster = 0;
    vfs_setup_operations(root_node);

    if (!bdev) {
        /* Virtual mode - no real disk */
        vfs_create_node("/bin", 1);
        vfs_create_node("/home", 1);
        vfs_create_node("/home/user", 1);
        vfs_create_node("/system", 1);
        vfs_create_node("/tmp", 1);
        vfs_create_node("/dev", 1);
        vfs_create_node("/etc", 1);

        vfs_node_t *readme = vfs_create_node("/home/user/readme.txt", 0);
        if (readme) {
            const char *welcome = "Chao mung ban den voi TVN_OS!\n";
            readme->size = strlen(welcome);
            vfs_setup_operations(readme);
        }
        return 0;
    }

    /* Read boot sector from disk */
    if (fat_read_sector(0, &boot_sector_copy) < 0)
        return -1;

    if (boot_sector_copy.bytes_per_sector != FAT_SECTOR_SIZE)
        return -1;

    /* Detect FAT type */
    fat_type = fat_detect_type(&boot_sector_copy);
    
    sectors_per_cluster = boot_sector_copy.sectors_per_cluster;
    bytes_per_cluster = sectors_per_cluster * FAT_SECTOR_SIZE;
    reserved_sectors = boot_sector_copy.reserved_sectors;
    num_fats = boot_sector_copy.num_fats;

    /* Set FAT size based on type */
    if (fat_type == FAT_TYPE_FAT32) {
        fat_sectors = boot_sector_copy.fat_size_32;
    } else {
        fat_sectors = boot_sector_copy.fat_size_16;
    }
    fat_size = fat_sectors * FAT_SECTOR_SIZE;

    data_start = reserved_sectors + (num_fats * fat_sectors);
    
    /* FAT12/16 have fixed root directory, FAT32 uses cluster */
    if (fat_type == FAT_TYPE_FAT32) {
        root_node->first_cluster = boot_sector_copy.root_cluster;
    } else {
        root_node->first_cluster = 0; /* Fixed root directory */
        /* Add root directory sectors to data_start */
        uint32_t root_dir_sectors = ((boot_sector_copy.root_entry_count * 32) + (boot_sector_copy.bytes_per_sector - 1)) / boot_sector_copy.bytes_per_sector;
        data_start += root_dir_sectors;
    }

    /* Read FAT table into memory */
    fat_table = (uint8_t *)kmalloc(fat_size);
    if (!fat_table) return -1;

    if (fat_read_sectors(reserved_sectors, fat_table, fat_sectors) < 0) {
        kfree(fat_table);
        fat_table = 0;
        return -1;
    }

    return 0;
}

/* Read a cluster from disk */
int fat_read_cluster(uint32_t cluster, void *buffer) {
    if (!buffer || cluster < 2 || !fat_bdev) return -1;

    uint64_t cluster_lba = data_start + (uint64_t)(cluster - 2) * sectors_per_cluster;

    return fat_read_sectors(cluster_lba, buffer, sectors_per_cluster);
}

/* Write a cluster to disk */
int fat_write_cluster(uint32_t cluster, const void *buffer) {
    if (!buffer || cluster < 2 || !fat_bdev) return -1;

    uint64_t cluster_lba = data_start + (uint64_t)(cluster - 2) * sectors_per_cluster;

    return fat_bdev->write(fat_bdev->priv, cluster_lba, buffer, sectors_per_cluster);
}

/* Get next cluster in chain (handles FAT12/16/32) */
uint32_t fat_get_next_cluster(uint32_t cluster) {
    if (cluster < 2 || !fat_table) return 0xFFFFFFFF;

    uint32_t fat_entry;
    
    if (fat_type == FAT_TYPE_FAT32) {
        fat_entry = ((uint32_t *)fat_table)[cluster];
    } else if (fat_type == FAT_TYPE_FAT16) {
        fat_entry = ((uint16_t *)fat_table)[cluster];
    } else { /* FAT12 */
        uint32_t offset = cluster + (cluster / 2);
        uint16_t fat_value = ((uint16_t *)fat_table)[offset];
        if (cluster & 1) {
            fat_entry = fat_value >> 4;
        } else {
            fat_entry = fat_value & 0x0FFF;
        }
    }

    if (fat_entry >= 0xFFFFFF8) return 0xFFFFFFFF;

    return fat_entry;
}

/* Set next cluster in chain (handles FAT12/16/32) */
int fat_set_next_cluster(uint32_t cluster, uint32_t next) {
    if (cluster < 2 || !fat_table) return -1;

    if (fat_type == FAT_TYPE_FAT32) {
        ((uint32_t *)fat_table)[cluster] = next;
    } else if (fat_type == FAT_TYPE_FAT16) {
        ((uint16_t *)fat_table)[cluster] = next;
    } else { /* FAT12 */
        uint32_t offset = cluster + (cluster / 2);
        uint16_t fat_value = ((uint16_t *)fat_table)[offset];
        if (cluster & 1) {
            fat_value = (fat_value & 0x000F) | ((next & 0x0FFF) << 4);
        } else {
            fat_value = (fat_value & 0xF000) | (next & 0x0FFF);
        }
        ((uint16_t *)fat_table)[offset] = fat_value;
    }

    return 0;
}

/* Find a free cluster (handles FAT12/16/32) */
uint32_t fat_get_free_cluster(void) {
    if (!fat_table) return 0;

    uint32_t num_entries;
    if (fat_type == FAT_TYPE_FAT32) {
        num_entries = fat_size / 4;
    } else if (fat_type == FAT_TYPE_FAT16) {
        num_entries = fat_size / 2;
    } else { /* FAT12 */
        num_entries = (fat_size * 2) / 3; /* Approximate */
    }

    for (uint32_t i = 2; i < num_entries; i++) {
        uint32_t entry = fat_get_next_cluster(i);
        if (entry == 0) return i;
    }

    return 0;
}

/* VFS - Get root node */
vfs_node_t *vfs_get_root(void) {
    return root_node;
}

/* Find a node by path */
vfs_node_t *vfs_find_node(const char *path) {
    if (!path || !root_node) return 0;
    
    /* Start from root */
    vfs_node_t *current = root_node;
    
    /* Skip leading slash */
    if (path[0] == '/') path++;
    
    char path_copy[FAT_MAX_PATH];
    strcpy(path_copy, path);
    char *token = path_copy;
    while (*token && current) {
        /* Bỏ qua các dấu '/' liên tiếp (tương tự strtok) */
        while (*token == '/') token++;
        if (!*token) break;

        char *end = token;
        while (*end && *end != '/') end++;
        
        char orig = *end;
        *end = 0; /* Ngắt chuỗi tại dấu '/' */
        
        /* Search in children */
        vfs_node_t *child = current->children;
        vfs_node_t *found = 0;
        while (child) {
            if (strcmp(child->name, token) == 0) {
                found = child;
                break;
            }
            child = child->next;
        }
        if (!found) return 0; /* Not found */
        current = found;
        
        if (orig == 0) break;
        token = end + 1;
    }
    
    return current;
}

/* Create a new node */
vfs_node_t *vfs_create_node(const char *path, uint8_t is_directory) {
    if (!path || vfs_node_count >= MAX_VFS_NODES) return 0;
    
    /* Extract parent path and name */
    char parent_path[FAT_MAX_PATH];
    char name[FAT_MAX_FILENAME];
    
    const char *last_slash = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last_slash = p; /* Handle both / and \ */
    }
    
    if (!last_slash) {
        strcpy(parent_path, "/");
        strcpy(name, path);
    } else {
        int len = last_slash - path;
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;
        strcpy(name, last_slash + 1);
    }
    
    /* Find parent */
    vfs_node_t *parent = vfs_find_node(parent_path);
    if (!parent) return 0;
    
    /* Check if already exists */
    vfs_node_t *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child; /* Already exists */
        }
        child = child->next;
    }
    
    /* Create new node */
    vfs_node_t *new_node = &vfs_nodes[vfs_node_count++];
    strcpy(new_node->name, name);
    new_node->size = 0;
    new_node->is_directory = is_directory;
    new_node->attributes = is_directory ? FAT_ATTR_DIRECTORY : FAT_ATTR_ARCHIVE;
    new_node->parent = parent;
    new_node->children = 0;
    new_node->next = 0;
    new_node->first_cluster = 0;
    vfs_setup_operations(new_node);
    
    /* Add to parent's children list */
    if (!parent->children) {
        parent->children = new_node;
    } else {
        vfs_node_t *last = parent->children;
        while (last->next) last = last->next;
        last->next = new_node;
    }

    dentry_add(parent, new_node);
    
    return new_node;
}

/* Delete a node */
int vfs_delete_node(const char *path) {
    vfs_node_t *node = vfs_find_node(path);
    if (!node) return -1;
    
    if (node->children && node->is_directory) {
        return -1; /* Directory not empty */
    }
    
    vfs_node_t *parent = node->parent;
    if (!parent) return -1;
    
    /* Remove from parent's children list */
    if (parent->children == node) {
        parent->children = node->next;
    } else {
        vfs_node_t *prev = parent->children;
        while (prev && prev->next != node) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = node->next;
        }
    }
    
    return 0;
}

/* List directory contents */
vfs_node_t *vfs_list_directory(vfs_node_t *dir) {
    if (!dir || !dir->is_directory) return 0;
    return dir->children;
}

/* File operations */
file_handle_t *file_open(const char *path, uint8_t write_mode) {
    (void)write_mode;  /* <-- Thêm dòng này để tắt warning */
    if (!path || file_handle_count >= MAX_OPEN_FILES) return 0;
    
    vfs_node_t *node = vfs_find_node(path);
    if (!node) {
        /* Try to create the file */
        node = vfs_create_node(path, 0);
        if (!node) return 0;
    }
    
    if (node->is_directory) return 0;
    
    file_handle_t *handle = &file_handles[file_handle_count++];
    handle->node = node;
    handle->position = 0;
    handle->is_open = 1;
    
    return handle;
}

int file_read(file_handle_t *file, void *buffer, uint32_t size) {
    if (!file || !file->is_open || !buffer) return -1;

    vfs_node_t *node = file->node;
    if (!node->f_op || !node->f_op->read) return -1;

    if (file->position + size > node->size)
        size = node->size - file->position;

    int ret = node->f_op->read(node, file->position, size, buffer);
    if (ret > 0) file->position += ret;
    return ret;
}

int file_write(file_handle_t *file, const void *buffer, uint32_t size) {
    if (!file || !file->is_open || !buffer) return -1;

    vfs_node_t *node = file->node;
    if (!node->f_op || !node->f_op->write) return -1;

    int ret = node->f_op->write(node, file->position, size, buffer);
    if (ret > 0) file->position += ret;
    return ret;
}

int file_close(file_handle_t *file) {
    if (!file) return -1;
    
    file->is_open = 0;
    file->node = 0;
    file->position = 0;
    
    return 0;
}

uint32_t file_seek(file_handle_t *file, uint32_t position) {
    if (!file || !file->is_open) return 0;
    
    vfs_node_t *node = file->node;
    if (position > node->size) position = node->size;
    
    file->position = position;
    return position;
}

uint32_t file_tell(file_handle_t *file) {
    if (!file || !file->is_open) return 0;
    return file->position;
}

/* ---- Vtable operations (Linux-style) ---- */

static int fat_file_open(struct vfs_node *node) {
    if (!node) return -1;
    return 0;
}

static int fat_file_close(struct vfs_node *node) {
    if (!node) return -1;
    return 0;
}

static int fat_file_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    if (!node || !buffer || !bytes_per_cluster) return -1;
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    if (node->first_cluster && size > 0) {
        uint32_t cluster = node->first_cluster;
        uint32_t pos = offset;
        uint32_t buf_pos = 0;
        uint8_t *temp = (uint8_t *)kmalloc(bytes_per_cluster);
        if (!temp) return -1;

        while (pos < offset + size && cluster < 0xFFFFFF8) {
            fat_read_cluster(cluster, temp);
            uint32_t cluster_off = pos % bytes_per_cluster;
            uint32_t to_read = bytes_per_cluster - cluster_off;
            if (to_read > offset + size - pos) to_read = offset + size - pos;
            memcpy((uint8_t *)buffer + buf_pos, temp + cluster_off, to_read);
            buf_pos += to_read;
            pos += to_read;
            cluster = fat_get_next_cluster(cluster);
        }
        kfree(temp);
    }

    return size;
}

static int fat_file_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    if (!node || !buffer || !bytes_per_cluster) return -1;

    uint32_t new_size = offset + size;
    if (new_size > node->size) node->size = new_size;

    if (node->first_cluster && size > 0 && fat_bdev) {
        uint32_t cluster = node->first_cluster;
        uint32_t pos = offset;
        uint32_t buf_pos = 0;
        uint8_t *temp = (uint8_t *)kmalloc(bytes_per_cluster);
        if (!temp) return -1;

        while (pos < offset + size && cluster < 0xFFFFFF8) {
            uint32_t cluster_off = pos % bytes_per_cluster;
            uint32_t to_write = bytes_per_cluster - cluster_off;
            if (to_write > offset + size - pos) to_write = offset + size - pos;

            if (to_write == bytes_per_cluster) {
                memcpy(temp, (uint8_t *)buffer + buf_pos, to_write);
            } else {
                fat_read_cluster(cluster, temp);
                memcpy(temp + cluster_off, (uint8_t *)buffer + buf_pos, to_write);
            }
            fat_write_cluster(cluster, temp);

            buf_pos += to_write;
            pos += to_write;
            cluster = fat_get_next_cluster(cluster);
        }
        kfree(temp);
    }

    return size;
}

static int fat_readdir(struct vfs_node *node, uint32_t index, struct vfs_node **child) {
    if (!node || !node->is_directory || !child) return -1;
    struct vfs_node *c = node->children;
    uint32_t i = 0;
    while (c) {
        if (i == index) { *child = c; return 0; }
        i++;
        c = c->next;
    }
    return -1;
}

static int fat_lookup(struct vfs_node *parent, const char *name, struct vfs_node **result) {
    if (!parent || !name || !result) return -1;
    for (struct vfs_node *c = parent->children; c; c = c->next) {
        if (strcmp(c->name, name) == 0) { *result = c; return 0; }
    }
    return -1;
}

static uint32_t str_hash(const char *s) {
    uint32_t h = 0;
    while (*s) { h = h * 33 + (uint8_t)*s++; }
    return h;
}

void vfs_mount(const char *path, const char *device, const char *fs_type) {
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].used) {
            int j;
            for (j = 0; path[j] && j < (int)sizeof(mount_table[i].mount_point) - 1; j++)
                mount_table[i].mount_point[j] = path[j];
            mount_table[i].mount_point[j] = 0;
            for (j = 0; device[j] && j < (int)sizeof(mount_table[i].device) - 1; j++)
                mount_table[i].device[j] = device[j];
            mount_table[i].device[j] = 0;
            for (j = 0; fs_type[j] && j < (int)sizeof(mount_table[i].fs_type) - 1; j++)
                mount_table[i].fs_type[j] = fs_type[j];
            mount_table[i].fs_type[j] = 0;
            mount_table[i].used = 1;
            return;
        }
    }
}

int vfs_umount(const char *path) {
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].used && strcmp(mount_table[i].mount_point, path) == 0) {
            mount_table[i].used = 0;
            return 0;
        }
    }
    return -1;
}

vfs_node_t *vfs_resolve(const char *path) {
    if (!path || path[0] == 0) return vfs_get_root();
    if (path[0] == '/') return vfs_find_node(path);
    char full[FAT_MAX_PATH];
    full[0] = '/';
    int i = 1, j = 0;
    while (path[j] && i < (int)sizeof(full) - 1) full[i++] = path[j++];
    full[i] = 0;
    return vfs_find_node(full);
}

dentry_t *dentry_lookup(const char *name, vfs_node_t *parent) {
    if (!name || !parent) return 0;
    uint32_t h = str_hash(name);
    uint32_t ph = (uint32_t)(uintptr_t)parent;
    for (int i = 0; i < dentry_count; i++) {
        if (dentry_cache[i].hash == h &&
            dentry_cache[i].parent_hash == ph &&
            strcmp(dentry_cache[i].name, name) == 0) {
            return &dentry_cache[i];
        }
    }
    return 0;
}

void dentry_add(vfs_node_t *parent, vfs_node_t *child) {
    if (!parent || !child || dentry_count >= MAX_DENTRY_CACHE) return;
    int i;
    for (i = 0; child->name[i] && i < (int)sizeof(dentry_cache[dentry_count].name) - 1; i++)
        dentry_cache[dentry_count].name[i] = child->name[i];
    dentry_cache[dentry_count].name[i] = 0;
    dentry_cache[dentry_count].hash = str_hash(child->name);
    dentry_cache[dentry_count].parent_hash = (uint32_t)(uintptr_t)parent;
    dentry_cache[dentry_count].node = child;
    dentry_cache[dentry_count].next = 0;
    dentry_count++;
}

/* Format a block device as FAT32 */
int fat32_format(struct block_dev *bdev) {
    if (!bdev || !bdev->write) return -1;

    uint32_t total_sectors = (uint32_t)bdev->total_sectors;
    if (total_sectors < 1000) return -1;

    uint16_t bps = 512;
    uint8_t spc = 1;
    if (total_sectors > 65536) spc = 8;
    if (total_sectors > 262144) spc = 16;
    if (total_sectors > 1048576) spc = 32;
    if (total_sectors > 4194304) spc = 64;

    uint16_t reserved = 32;
    uint8_t num_fats = 2;

    uint32_t fat_sectors = (total_sectors + spc * 128 - 1) / (spc * 128);
    if (fat_sectors < 1) fat_sectors = 1;

    uint32_t data_sectors = total_sectors - reserved - num_fats * fat_sectors;
    uint32_t clusters = data_sectors / spc;

    while (clusters > 0x0FFFFFF5 && fat_sectors < 10000) {
        fat_sectors++;
        data_sectors = total_sectors - reserved - num_fats * fat_sectors;
        clusters = data_sectors / spc;
    }

    /* Build boot sector */
    uint8_t boot[512];
    memset(boot, 0, 512);
    fat_boot_sector_t *bs = (fat_boot_sector_t *)boot;

    bs->jmp_boot[0] = 0xEB;
    bs->jmp_boot[1] = 0x58;
    bs->jmp_boot[2] = 0x90;
    memcpy(bs->oem_name, "NOCTUA1.0", 8);
    bs->bytes_per_sector = bps;
    bs->sectors_per_cluster = spc;
    bs->reserved_sectors = reserved;
    bs->num_fats = num_fats;
    bs->root_entry_count = 0;
    bs->total_sectors_16 = 0;
    bs->media = 0xF8;
    bs->fat_size_16 = 0;
    bs->sectors_per_track = 63;
    bs->num_heads = 255;
    bs->hidden_sectors = 0;
    bs->total_sectors_32 = total_sectors;
    bs->fat_size_32 = fat_sectors;
    bs->ext_flags = 0;
    bs->fs_version = 0;
    bs->root_cluster = 2;
    bs->fs_info = 1;
    bs->backup_boot_sector = 6;
    memset(bs->reserved, 0, 12);
    bs->drive_number = 0x80;
    bs->reserved1 = 0;
    bs->ext_boot_signature = 0x29;
    bs->volume_id = 0x12345678;
    memcpy(bs->volume_label, "NOCTUA OS ", 11);
    memcpy(bs->fs_type, "FAT32   ", 8);
    /* Signature at offset 510 */
    boot[510] = 0x55;
    boot[511] = 0xAA;

    /* Write boot sector */
    if (bdev->write(bdev->priv, 0, boot, 1) < 0) return -1;

    /* Write FSInfo sector */
    uint8_t fsinfo[512];
    memset(fsinfo, 0, 512);
    *(uint32_t *)(fsinfo + 0)   = 0x41615252;
    *(uint32_t *)(fsinfo + 484) = 0x61417272;
    *(uint32_t *)(fsinfo + 488) = 0xFFFFFFFF;
    *(uint32_t *)(fsinfo + 492) = 0xFFFFFFFF;
    *(uint32_t *)(fsinfo + 508) = 0xAA550000;
    if (bdev->write(bdev->priv, 1, fsinfo, 1) < 0) return -1;

    /* Write backup boot sector + backup FSInfo */
    if (bdev->write(bdev->priv, 6, boot, 1) < 0) return -1;
    if (bdev->write(bdev->priv, 7, fsinfo, 1) < 0) return -1;

    /* Build and write FAT tables */
    uint32_t fat_bytes = fat_sectors * 512;
    uint8_t *fat = (uint8_t *)kmalloc(fat_bytes);
    if (!fat) return -1;
    memset(fat, 0, fat_bytes);

    /* Cluster 0: media ID byte */
    fat[0] = 0xF8;
    fat[1] = 0xFF;
    fat[2] = 0xFF;
    fat[3] = 0x0F;
    /* Cluster 1: EOC */
    fat[4] = 0xFF;
    fat[5] = 0xFF;
    fat[6] = 0xFF;
    fat[7] = 0x0F;
    /* Cluster 2: EOC (root directory) */
    fat[8]  = 0xFF;
    fat[9]  = 0xFF;
    fat[10] = 0xFF;
    fat[11] = 0x0F;

    /* Write FAT1 */
    if (bdev->write(bdev->priv, reserved, fat, fat_sectors) < 0) {
        kfree(fat);
        return -1;
    }
    /* Write FAT2 */
    if (bdev->write(bdev->priv, reserved + fat_sectors, fat, fat_sectors) < 0) {
        kfree(fat);
        return -1;
    }
    kfree(fat);

    /* Write root directory cluster (cluster 2) */
    uint32_t root_lba = reserved + num_fats * fat_sectors + (2 - 2) * spc;
    uint8_t *root_dir = (uint8_t *)kmalloc(spc * 512);
    if (!root_dir) return -1;
    memset(root_dir, 0, spc * 512);

    /* Volume label entry */
    fat_dir_entry_t *vol = (fat_dir_entry_t *)root_dir;
    memcpy(vol->name, "NOCTUA OS ", 11);
    vol->attributes = FAT_ATTR_VOLUME_ID;
    vol->write_date = 0x4A85;
    vol->write_time = 0x0000;

    if (bdev->write(bdev->priv, root_lba, root_dir, spc) < 0) {
        kfree(root_dir);
        return -1;
    }
    kfree(root_dir);

    return 0;
}

/* Read boot sector and extract FAT32 geometry */
static int fat32_read_geo(struct block_dev *bdev, uint32_t *reserved,
                          uint32_t *fat_sectors, uint8_t *spc,
                          uint8_t *num_fats, uint32_t *root_cluster) {
    uint8_t buf[512];
    if (bdev->read(bdev->priv, 0, buf, 1) < 0) return -1;
    fat_boot_sector_t *bs = (fat_boot_sector_t *)buf;
    if (bs->bytes_per_sector != 512) return -1;
    *reserved = bs->reserved_sectors;
    *fat_sectors = bs->fat_size_32;
    *spc = bs->sectors_per_cluster;
    *num_fats = bs->num_fats;
    *root_cluster = bs->root_cluster;
    return 0;
}

/* Allocate a free cluster, read-modify-write FAT table */
static int fat32_alloc_cluster(struct block_dev *bdev, uint32_t reserved,
                                uint32_t fat_sectors, uint32_t *new_cluster) {
    uint32_t fat_bytes = fat_sectors * 512;
    uint8_t *fat = (uint8_t *)kmalloc(fat_bytes);
    if (!fat) return -1;
    if (bdev->read(bdev->priv, reserved, fat, fat_sectors) < 0) {
        kfree(fat); return -1;
    }
    uint32_t num_entries = fat_bytes / 4;
    *new_cluster = 0;
    for (uint32_t i = 2; i < num_entries; i++) {
        if (((uint32_t *)fat)[i] == 0) {
            *new_cluster = i;
            break;
        }
    }
    if (*new_cluster == 0) { kfree(fat); return -1; }
    ((uint32_t *)fat)[*new_cluster] = 0x0FFFFFFF; /* EOC */
    if (bdev->write(bdev->priv, reserved, fat, fat_sectors) < 0) {
        kfree(fat); return -1;
    }
    if (bdev->write(bdev->priv, reserved + fat_sectors, fat, fat_sectors) < 0) {
        kfree(fat); return -1;
    }
    kfree(fat);
    return 0;
}

/* Add a directory entry into a parent cluster */
static int fat32_add_entry(struct block_dev *bdev, uint32_t parent_lba,
                            uint8_t spc, const char *name, uint8_t attr,
                            uint32_t first_cluster, uint32_t file_size) {
    uint32_t clus_bytes = spc * 512;
    uint8_t *data = (uint8_t *)kmalloc(clus_bytes);
    if (!data) return -1;
    if (bdev->read(bdev->priv, parent_lba, data, spc) < 0) {
        kfree(data); return -1;
    }
    fat_dir_entry_t *entry = 0;
    int max_entries = clus_bytes / 32;
    for (int i = 0; i < max_entries; i++) {
        fat_dir_entry_t *e = (fat_dir_entry_t *)data + i;
        if (e->name[0] == 0 || e->name[0] == 0xE5) { entry = e; break; }
    }
    if (!entry) { kfree(data); return -1; }
    memset(entry, 0, sizeof(fat_dir_entry_t));
    memset(entry->name, ' ', 11);
    int j;
    for (j = 0; name[j] && j < 8; j++) entry->name[j] = name[j];
    entry->attributes = attr;
    entry->first_cluster_low = first_cluster & 0xFFFF;
    entry->first_cluster_high = (first_cluster >> 16) & 0xFFFF;
    entry->write_date = 0x4A85;
    entry->write_time = 0x0000;
    entry->file_size = file_size;
    if (bdev->write(bdev->priv, parent_lba, data, spc) < 0) {
        kfree(data); return -1;
    }
    kfree(data);
    return 0;
}

/* Create a directory on a formatted FAT32 partition */
int fat32_mkdir(struct block_dev *bdev, const char *name) {
    if (!bdev || !name || !name[0]) return -1;
    uint32_t reserved, fat_secs, root_cluster;
    uint8_t spc, num_fats;
    if (fat32_read_geo(bdev, &reserved, &fat_secs, &spc, &num_fats, &root_cluster) < 0)
        return -1;

    uint32_t new_cluster;
    if (fat32_alloc_cluster(bdev, reserved, fat_secs, &new_cluster) < 0)
        return -1;

    /* Write "." and ".." entries in the new directory */
    uint32_t clus_bytes = spc * 512;
    uint8_t *data = (uint8_t *)kmalloc(clus_bytes);
    if (!data) return -1;
    memset(data, 0, clus_bytes);

    fat_dir_entry_t *dot = (fat_dir_entry_t *)data;
    memset(dot->name, ' ', 11);
    dot->name[0] = '.';
    dot->attributes = FAT_ATTR_DIRECTORY;
    dot->first_cluster_low = new_cluster & 0xFFFF;
    dot->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    dot->write_date = 0x4A85;

    fat_dir_entry_t *dotdot = (fat_dir_entry_t *)(data + 32);
    memset(dotdot->name, ' ', 11);
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    dotdot->attributes = FAT_ATTR_DIRECTORY;
    dotdot->first_cluster_low = root_cluster & 0xFFFF;
    dotdot->first_cluster_high = (root_cluster >> 16) & 0xFFFF;
    dotdot->write_date = 0x4A85;

    uint32_t data_start = reserved + num_fats * fat_secs;
    uint32_t new_lba = data_start + (new_cluster - 2) * spc;
    if (bdev->write(bdev->priv, new_lba, data, spc) < 0) {
        kfree(data); return -1;
    }
    kfree(data);

    /* Add entry in root directory */
    uint32_t root_lba = data_start + (root_cluster - 2) * spc;
    if (fat32_add_entry(bdev, root_lba, spc, name, FAT_ATTR_DIRECTORY, new_cluster, 0) < 0)
        return -1;

    return 0;
}

/* Write a text file to the root directory of a formatted FAT32 partition */
int fat32_write_file(struct block_dev *bdev, const char *name, const char *content) {
    if (!bdev || !name || !name[0]) return -1;
    uint32_t reserved, fat_secs, root_cluster;
    uint8_t spc, num_fats;
    if (fat32_read_geo(bdev, &reserved, &fat_secs, &spc, &num_fats, &root_cluster) < 0)
        return -1;

    uint32_t clus_bytes = spc * 512;
    uint32_t len = strlen(content);
    uint32_t clusters_needed = (len + clus_bytes - 1) / clus_bytes;
    if (clusters_needed < 1) clusters_needed = 1;

    /* Allocate cluster chain */
    uint32_t fat_bytes = fat_secs * 512;
    uint8_t *fat = (uint8_t *)kmalloc(fat_bytes);
    if (!fat) return -1;
    if (bdev->read(bdev->priv, reserved, fat, fat_secs) < 0) {
        kfree(fat); return -1;
    }

    uint32_t first_cluster = 0;
    uint32_t prev = 0;
    uint32_t num_entries = fat_bytes / 4;
    for (uint32_t c = 0; c < clusters_needed; c++) {
        uint32_t nc = 0;
        for (uint32_t i = 2; i < num_entries; i++) {
            if (((uint32_t *)fat)[i] == 0) { nc = i; break; }
        }
        if (nc == 0) { kfree(fat); return -1; }
        if (c == 0) first_cluster = nc;
        if (prev) ((uint32_t *)fat)[prev] = nc;
        prev = nc;
    }
    if (prev) ((uint32_t *)fat)[prev] = 0x0FFFFFFF;

    if (bdev->write(bdev->priv, reserved, fat, fat_secs) < 0) {
        kfree(fat); return -1;
    }
    if (bdev->write(bdev->priv, reserved + fat_secs, fat, fat_secs) < 0) {
        kfree(fat); return -1;
    }
    kfree(fat);

    /* Write content to clusters */
    uint32_t data_start = reserved + num_fats * fat_secs;
    uint32_t cluster = first_cluster;
    uint32_t offset = 0;
    while (cluster < 0x0FFFFFF8 && offset < len) {
        uint32_t lba = data_start + (cluster - 2) * spc;
        uint32_t chunk = len - offset;
        if (chunk > clus_bytes) chunk = clus_bytes;
        uint8_t *buf = (uint8_t *)kmalloc(clus_bytes);
        if (!buf) return -1;
        memset(buf, 0, clus_bytes);
        memcpy(buf, content + offset, chunk);
        if (bdev->write(bdev->priv, lba, buf, spc) < 0) {
            kfree(buf); return -1;
        }
        kfree(buf);
        offset += chunk;
        /* Read next cluster from FAT */
        uint8_t *fat2 = (uint8_t *)kmalloc(fat_bytes);
        if (!fat2) return -1;
        bdev->read(bdev->priv, reserved, fat2, fat_secs);
        cluster = ((uint32_t *)fat2)[cluster];
        kfree(fat2);
    }

    /* Add entry in root directory */
    uint32_t root_lba = data_start + (root_cluster - 2) * spc;
    if (fat32_add_entry(bdev, root_lba, spc, name, FAT_ATTR_ARCHIVE, first_cluster, len) < 0)
        return -1;

    return 0;
}

/* Gán vtable operations cho node */
void vfs_setup_operations(vfs_node_t *node) {
    if (!node) return;

    static file_operations_t def_fops = {
        fat_file_open, fat_file_close,
        fat_file_read, fat_file_write,
        fat_readdir
    };

    static inode_operations_t def_iops = {
        0,  /* create */
        0,  /* unlink */
        0,  /* mkdir */
        0,  /* rmdir */
        fat_lookup
    };

    node->f_op = &def_fops;
    node->i_op = &def_iops;
}

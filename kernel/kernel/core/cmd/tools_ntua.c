#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"
#include "heap.h"
#include "fs/fat32.h"
#include "net/net.h"
#include "keyboard.h"

static void scr(const char *s) { screen_term_write(s); }
static void scf(void) { screen_set_content_color(C_INFO); }
static void sch(void) { screen_set_content_color(C_HEADER); }

#define REPO_PORT 4242
static char REPO_SERVER_IP[16] = "10.0.2.2";

typedef struct {
    char name[64];
    uint32_t size;
} __attribute__((packed)) pkg_hdr_t;

static int has_flag(const char *args, char flag) {
    if (!args) return 0;
    for (const char *p = args; *p; p++)
        if (*p == '-' && p[1] == flag) return 1;
    return 0;
}

static int str_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    for (const char *h = haystack; *h; h++) {
        const char *n = needle, *p = h;
        while (*n && *p && *n == *p) { n++; p++; }
        if (!*n) return 1;
    }
    return 0;
}

static int next_arg(const char *args, char *out, int max) {
    while (*args == ' ') args++;
    int i = 0;
    while (*args && *args > ' ' && i < max - 1) out[i++] = *args++;
    out[i] = 0;
    return i;
}

static int tcp_download(const char *request, uint8_t *buf, uint32_t buf_size, uint32_t *out_len) {
    uint8_t ip[4];
    if (net_str_to_ip(REPO_SERVER_IP, ip) < 0) return -1;
    int sock = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return -1;
    sock_addr_t addr;
    for (int i = 0; i < 4; i++) addr.ip[i] = ip[i];
    addr.port = (REPO_PORT >> 8) | ((REPO_PORT & 0xFF) << 8);
    if (socket_connect(sock, &addr) < 0) { socket_close(sock); return -1; }
    socket_send(sock, (const uint8_t *)request, strlen(request), 0);
    int total = 0, n;
    while (total < (int)buf_size) {
        n = socket_recv(sock, buf + total, buf_size - total, 0);
        if (n <= 0) break;
        total += n;
    }
    socket_close(sock);
    *out_len = total;
    return 0;
}

static int cmd_download(const char *args) {
    if (!args || has_flag(args, 'h')) {
        scr(" Usage: ntua download <package>\n");
        return CMD_RET_OK;
    }
    char pkg[64]; next_arg(args, pkg, sizeof(pkg));
    if (pkg[0] == 0) { scr(" No package specified\n"); return CMD_RET_OK; }
    char req[128]; strcpy(req, "DOWNLOAD "); strcat(req, pkg); strcat(req, "\n");
    uint8_t *buf = kmalloc(65536);
    if (!buf) { scr(" Out of memory\n"); return CMD_RET_OK; }
    uint32_t len = 0;
    sch(); scr("=== Download "); scr(pkg); scr(" ===\n"); scf();
    if (tcp_download(req, buf, 65536, &len) < 0 || len == 0) {
        scr(" Download failed\n"); kfree(buf); return CMD_RET_OK;
    }
    pkg_hdr_t *hdr = (pkg_hdr_t *)buf;
    scr(" Package: "); scr(hdr->name); scr("\n");
    char num[16]; int2str((int)hdr->size, num);
    scr(" Size:    "); scr(num); scr(" bytes\n");
    file_handle_t *f = file_open(pkg, 1);
    if (!f) { scr(" Cannot write file\n"); kfree(buf); return CMD_RET_OK; }
    file_write(f, hdr + 1, hdr->size);
    file_close(f);
    scr(" Saved to "); scr(pkg); scr("\n");
    kfree(buf);
    return CMD_RET_OK;
}

static int ensure_dir(const char *path) {
    vfs_node_t *n = vfs_find_node(path);
    if (n && n->is_directory) return 0;
    if (!n) n = vfs_create_node(path, 1);
    return n ? 0 : -1;
}

static int cmd_install(const char *args) {
    if (!args || has_flag(args, 'h')) {
        scr(" Usage: ntua install <package>\n");
        return CMD_RET_OK;
    }
    char pkg[64]; next_arg(args, pkg, sizeof(pkg));
    if (pkg[0] == 0) { scr(" No package specified\n"); return CMD_RET_OK; }
    ensure_dir("/packages");
    char req[128]; strcpy(req, "DOWNLOAD "); strcat(req, pkg); strcat(req, "\n");
    uint8_t *buf = kmalloc(65536);
    if (!buf) { scr(" Out of memory\n"); return CMD_RET_OK; }
    uint32_t len = 0;
    sch(); scr("=== Install "); scr(pkg); scr(" ===\n"); scf();
    if (tcp_download(req, buf, 65536, &len) < 0 || len == 0) {
        scr(" Download failed\n"); kfree(buf); return CMD_RET_OK;
    }
    pkg_hdr_t *hdr = (pkg_hdr_t *)buf;
    char dirpath[128]; strcpy(dirpath, "/packages/"); strcat(dirpath, hdr->name);
    ensure_dir(dirpath);
    uint32_t offset = sizeof(pkg_hdr_t);
    while (offset < len) {
        pkg_hdr_t *fh = (pkg_hdr_t *)(buf + offset);
        if (fh->name[0] == 0 || fh->size == 0) { offset += sizeof(pkg_hdr_t); continue; }
        offset += sizeof(pkg_hdr_t);
        char filepath[256]; strcpy(filepath, dirpath); strcat(filepath, "/"); strcat(filepath, fh->name);
        file_handle_t *f = file_open(filepath, 1);
        if (f) {
            file_write(f, buf + offset, fh->size);
            file_close(f);
            scr("  "); scr(fh->name); scr("\n");
        }
        offset += fh->size;
    }
    char num[16]; int2str((int)(len - sizeof(pkg_hdr_t)), num);
    scr(" Installed "); scr(num); scr(" bytes\n");
    kfree(buf);
    return CMD_RET_OK;
}

static int cmd_list(const char *args) {
    if (args && has_flag(args, 'h')) {
        scr(" Usage: ntua list\n");
        return CMD_RET_OK;
    }
    sch(); scr("=== Installed Packages ===\n"); scf();
    vfs_node_t *dir = vfs_find_node("/packages");
    if (!dir || !dir->is_directory) { scr(" No packages installed\n"); return CMD_RET_OK; }
    vfs_node_t *child = dir->children;
    int count = 0;
    while (child) {
        if (child->is_directory) {
            scr("  "); scr(child->name); scr("\n");
            count++;
        }
        child = child->next;
    }
    if (count == 0) scr(" (empty)\n");
    else { char num[16]; int2str(count, num); scr(" Total: "); scr(num); scr(" packages\n"); }
    return CMD_RET_OK;
}

static int cmd_search(const char *args) {
    if (!args || has_flag(args, 'h')) {
        scr(" Usage: ntua search <term>\n");
        return CMD_RET_OK;
    }
    char term[64]; next_arg(args, term, sizeof(term));
    if (term[0] == 0) { scr(" No search term\n"); return CMD_RET_OK; }
    sch(); scr("=== Search: "); scr(term); scr(" ===\n"); scf();
    uint8_t *buf = kmalloc(65536);
    if (!buf) { scr(" Out of memory\n"); return CMD_RET_OK; }
    uint32_t len = 0;
    if (tcp_download("LIST\n", buf, 65536, &len) < 0 || len == 0) {
        scr(" Could not fetch package list\n"); kfree(buf); return CMD_RET_OK;
    }
    int found = 0;
    uint32_t pos = 0;
    while (pos < len) {
        char line[128]; int li = 0;
        while (pos < len && buf[pos] != '\n' && li < 127) line[li++] = buf[pos++];
        if (pos < len) pos++;
        line[li] = 0;
        if (line[0] && str_contains(line, term)) {
            scr("  "); scr(line); scr("\n"); found++;
        }
    }
    if (!found) scr(" No matching packages\n");
    else { char num[16]; int2str(found, num); scr(" Found "); scr(num); scr(" matches\n"); }
    kfree(buf);
    return CMD_RET_OK;
}

static int cmd_remove(const char *args) {
    if (!args || has_flag(args, 'h')) {
        scr(" Usage: ntua remove <package>\n");
        return CMD_RET_OK;
    }
    char pkg[64]; next_arg(args, pkg, sizeof(pkg));
    if (pkg[0] == 0) { scr(" No package specified\n"); return CMD_RET_OK; }
    char path[128]; strcpy(path, "/packages/"); strcat(path, pkg);
    sch(); scr("=== Remove "); scr(pkg); scr(" ===\n"); scf();
    vfs_node_t *node = vfs_find_node(path);
    if (!node) { scr(" Package not found\n"); return CMD_RET_OK; }
    vfs_node_t *child = node->children;
    while (child) {
        vfs_node_t *next = child->next;
        char child_path[256]; strcpy(child_path, path); strcat(child_path, "/"); strcat(child_path, child->name);
        vfs_delete_node(child_path);
        child = next;
    }
    if (vfs_delete_node(path) == 0) scr(" Removed\n");
    else scr(" Remove failed\n");
    return CMD_RET_OK;
}

static int cmd_update(const char *args) {
    if (args && has_flag(args, 'h')) {
        scr(" Usage: ntua update\n");
        return CMD_RET_OK;
    }
    sch(); scr("=== Update Package List ===\n"); scf();
    uint8_t *buf = kmalloc(65536);
    if (!buf) { scr(" Out of memory\n"); return CMD_RET_OK; }
    uint32_t len = 0;
    if (tcp_download("LIST\n", buf, 65536, &len) < 0 || len == 0) {
        scr(" Update failed - no repo access\n"); kfree(buf); return CMD_RET_OK;
    }
    ensure_dir("/packages");
    file_handle_t *f = file_open("/packages/.repo_list", 1);
    if (f) {
        file_write(f, buf, len);
        file_close(f);
    }
    char num[16]; int2str((int)len, num);
    scr(" Received "); scr(num); scr(" bytes from repo\n");
    scr(" Use 'ntua list' to view local packages\n");
    scr(" Use 'ntua search <term>' to search\n");
    kfree(buf);
    return CMD_RET_OK;
}

static int cmd_repo(const char *args) {
    if (!args || has_flag(args, 'h')) {
        scr(" Usage: ntua repo <host>\n");
        scr(" Current: "); scr(REPO_SERVER_IP); scr("\n");
        return CMD_RET_OK;
    }
    char host[64]; next_arg(args, host, sizeof(host));
    if (host[0] == 0) {
        scr(" Repo: "); scr(REPO_SERVER_IP); scr("\n");
        return CMD_RET_OK;
    }
    strcpy(REPO_SERVER_IP, host);
    sch(); scr("=== Repo Server ===\n"); scf();
    scr(" Set to "); scr(REPO_SERVER_IP); scr("\n");
    return CMD_RET_OK;
}

static int cmd_ntua(const char *args) {
    if (!args || has_flag(args, 'h')) {
        scr(" Usage: ntua <command> [options]\n");
        scr(" Commands:\n");
        scr("  download <pkg>  Download package from repo\n");
        scr("  install  <pkg>  Install package\n");
        scr("  list            List installed packages\n");
        scr("  search   <term> Search packages\n");
        scr("  remove   <pkg>  Remove package\n");
        scr("  update          Refresh package list\n");
        scr("  repo     <host> Set repo server\n");
        scr(" Current repo: "); scr(REPO_SERVER_IP); scr("\n");
        return CMD_RET_OK;
    }
    while (*args == ' ') args++;
    char cmd[32]; int ci = 0;
    while (*args && *args > ' ' && ci < 31) cmd[ci++] = *args++;
    cmd[ci] = 0;
    while (*args == ' ') args++;
    if (strcmp(cmd, "download") == 0) return cmd_download(args);
    if (strcmp(cmd, "install") == 0) return cmd_install(args);
    if (strcmp(cmd, "list") == 0) return cmd_list(args);
    if (strcmp(cmd, "search") == 0) return cmd_search(args);
    if (strcmp(cmd, "remove") == 0) return cmd_remove(args);
    if (strcmp(cmd, "update") == 0) return cmd_update(args);
    if (strcmp(cmd, "repo") == 0) return cmd_repo(args);
    scr(" Unknown subcommand: "); scr(cmd); scr("\n");
    scr(" Use 'ntua -h' for help\n");
    return CMD_RET_OK;
}

void tools_ntua_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("ntua", cmd_ntua, "Package manager - install/remove/search packages",
                 "ntua <download|install|list|search|remove|update|repo> [args]",
                 CMD_CAT_NOCTUA, "1.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 4242
#define PKG_DIR "packages"
#define NAME_LEN 64
#define SIZE_LEN 4
#define BUF_SIZE 4096

static int read_pkg_name(const char *filename, char *name, int maxlen) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", PKG_DIR, filename);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, name, maxlen - 1);
    close(fd);
    if (n <= 0) return -1;
    name[n] = '\0';
    for (int i = 0; i < n; i++) {
        if (name[i] == '\0') { name[i] = '\0'; break; }
        if (name[i] < 32) name[i] = '\0';
    }
    return 0;
}

static void handle_client(int client_fd) {
    char buf[BUF_SIZE];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';

    if (strncmp(buf, "LIST", 4) == 0 && buf[4] == '\0') {
        DIR *dir = opendir(PKG_DIR);
        if (!dir) {
            send(client_fd, "ERR\n", 4, 0);
            close(client_fd);
            return;
        }
        char resp[BUF_SIZE] = {0};
        int off = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char name[NAME_LEN];
            if (read_pkg_name(entry->d_name, name, sizeof(name)) == 0) {
                int len = strlen(name);
                if (off + len + 1 < (int)sizeof(resp)) {
                    memcpy(resp + off, name, len);
                    off += len;
                    resp[off++] = '\n';
                }
            }
        }
        closedir(dir);
        send(client_fd, resp, off, 0);

    } else if (strncmp(buf, "GET ", 4) == 0) {
        const char *pkgname = buf + 4;
        DIR *dir = opendir(PKG_DIR);
        if (!dir) {
            send(client_fd, "ERR\n", 4, 0);
            close(client_fd);
            return;
        }
        int found = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char name[NAME_LEN];
            if (read_pkg_name(entry->d_name, name, sizeof(name)) == 0 &&
                strcmp(name, pkgname) == 0) {
                found = 1;
                char path[256];
                snprintf(path, sizeof(path), "%s/%s", PKG_DIR, entry->d_name);
                int fd = open(path, O_RDONLY);
                if (fd >= 0) {
                    struct stat st;
                    if (fstat(fd, &st) == 0) {
                        int fsize = st.st_size;
                        send(client_fd, (const char*)&fsize, sizeof(fsize), 0);
                        char fbuf[BUF_SIZE];
                        int r;
                        while ((r = read(fd, fbuf, sizeof(fbuf))) > 0) {
                            send(client_fd, fbuf, r, 0);
                        }
                    }
                    close(fd);
                }
                break;
            }
        }
        closedir(dir);
        if (!found)
            send(client_fd, "ERR\n", 4, 0);

    } else if (strncmp(buf, "SEARCH ", 7) == 0) {
        const char *term = buf + 7;
        DIR *dir = opendir(PKG_DIR);
        if (!dir) {
            send(client_fd, "ERR\n", 4, 0);
            close(client_fd);
            return;
        }
        char resp[BUF_SIZE] = {0};
        int off = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char name[NAME_LEN];
            if (read_pkg_name(entry->d_name, name, sizeof(name)) == 0 &&
                strstr(name, term) != NULL) {
                int len = strlen(name);
                if (off + len + 1 < (int)sizeof(resp)) {
                    memcpy(resp + off, name, len);
                    off += len;
                    resp[off++] = '\n';
                }
            }
        }
        closedir(dir);
        send(client_fd, resp, off, 0);

    } else {
        send(client_fd, "ERR\n", 4, 0);
    }

    close(client_fd);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        printf("Noctua OS Package Repo Server\n");
        printf("Usage: %s\n", argv[0]);
        printf("  Listens on port %d, serves packages from '%s/' directory\n", PORT, PKG_DIR);
        printf("  Commands: LIST, GET <name>, SEARCH <term>\n");
        return 0;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("ntua-server: socket failed\n");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("ntua-server: bind failed\n");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 8) < 0) {
        printf("ntua-server: listen failed\n");
        close(server_fd);
        return 1;
    }

    printf("ntua-server: listening on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        printf("ntua-server: connection from %s:%d\n", ip, ntohs(client_addr.sin_port));

        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}

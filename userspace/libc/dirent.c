#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

DIR *opendir(const char *name) {
    int fd = open(name, 0);
    if (fd < 0) return 0;
    DIR *dirp = (DIR *)malloc(sizeof(DIR));
    if (!dirp) { close(fd); return 0; }
    dirp->fd = fd;
    dirp->offset = 0;
    memset(&dirp->cur, 0, sizeof(dirp->cur));
    return dirp;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp) return 0;
    int ret = getdents(dirp->fd, &dirp->cur, sizeof(struct dirent));
    if (ret <= 0) return 0;
    return &dirp->cur;
}

int closedir(DIR *dirp) {
    if (!dirp) return -1;
    close(dirp->fd);
    free(dirp);
    return 0;
}
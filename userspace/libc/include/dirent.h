#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h>

struct dirent {
    ino_t        d_ino;
    off_t        d_off;
    unsigned short d_reclen;
    char         d_name[256];
};

typedef struct DIR {
    int fd;
    struct dirent cur;
    int offset;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif
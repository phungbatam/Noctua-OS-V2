#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/* open flags (POSIX octal) */
#define O_RDONLY     0
#define O_WRONLY     1
#define O_RDWR       2
#define O_CREAT    0100
#define O_EXCL     0200
#define O_NOCTTY   0400
#define O_TRUNC   01000
#define O_APPEND  02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 0200000

/* fcntl commands */
#define F_DUPFD   0
#define F_GETFD   1
#define F_SETFD   2
#define F_GETFL   3
#define F_SETFL   4

/* Standard file descriptors (POSIX names) */
#define FD_CLOEXEC 1

int creat(const char *path, mode_t mode);
int fcntl(int fd, int cmd, ...);

#endif
#include <unistd.h>
#include <stdarg.h>

int open(const char *path, int flags) {
    return (int)syscall(SYS_OPEN, (long)path, (long)flags, 0);
}

ssize_t read(int fd, void *buf, size_t count) {
    return (ssize_t)syscall(SYS_READ, (long)fd, (long)buf, (long)count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return (ssize_t)syscall(SYS_WRITE, (long)fd, (long)buf, (long)count);
}

int close(int fd) {
    return (int)syscall(SYS_CLOSE, (long)fd, 0, 0);
}

pid_t getpid(void) {
    return (pid_t)syscall(SYS_GETPID, 0, 0, 0);
}

void exit(int status) {
    syscall(SYS_EXIT, (long)status, 0, 0);
    for (;;);
}

void *sbrk(intptr_t increment) {
    return (void *)syscall(SYS_SBRK, (long)increment, 0, 0);
}

int pipe(int pipefd[2]) {
    return (int)syscall(SYS_PIPE, (long)pipefd, 0, 0);
}

void sleep(uint32_t ms) {
    syscall(SYS_SLEEP, (long)ms, 0, 0);
}

pid_t fork(void) {
    return (pid_t)syscall(SYS_FORK, 0, 0, 0);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    return (pid_t)syscall(SYS_WAITPID, (long)pid, (long)status, (long)options);
}

pid_t getppid(void) {
    return (pid_t)syscall(SYS_GETPPID, 0, 0, 0);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    return (int)syscall(SYS_EXECVE, (long)path, (long)argv, (long)envp);
}

int stat(const char *path, struct stat *buf) {
    return (int)syscall(SYS_STAT, (long)path, (long)buf, 0);
}

int fstat(int fd, struct stat *buf) {
    return (int)syscall(SYS_FSTAT, (long)fd, (long)buf, 0);
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)syscall(SYS_LSEEK, (long)fd, (long)offset, (long)whence);
}

int dup(int oldfd) {
    return (int)syscall(SYS_DUP, (long)oldfd, 0, 0);
}

int dup2(int oldfd, int newfd) {
    return (int)syscall(SYS_DUP2, (long)oldfd, (long)newfd, 0);
}

int chdir(const char *path) {
    return (int)syscall(SYS_CHDIR, (long)path, 0, 0);
}

int mkdir(const char *path) {
    return (int)syscall(SYS_MKDIR, (long)path, 0, 0);
}

int rmdir(const char *path) {
    return (int)syscall(SYS_RMDIR, (long)path, 0, 0);
}

int uname(void *buf) {
    return (int)syscall(SYS_UNAME, (long)buf, 0, 0);
}

int unlink(const char *path) {
    return (int)syscall(SYS_UNLINK, (long)path, 0, 0);
}

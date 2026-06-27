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
    __builtin_unreachable();
}

void _exit(int status) {
    syscall(SYS_EXIT, (long)status, 0, 0);
    __builtin_unreachable();
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

int wait(int *status) {
    return (int)syscall(SYS_WAITPID, (long)-1, (long)status, 0);
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

int auth_login(const char *user, const char *pass) {
    return (int)syscall(SYS_AUTH_LOGIN, (long)user, (long)pass, 0);
}

unsigned int getuid(void) {
    return (unsigned int)syscall(SYS_GETUID, 0, 0, 0);
}

unsigned int getgid(void) {
    return (unsigned int)syscall(SYS_GETGID, 0, 0, 0);
}

unsigned int geteuid(void) {
    return (unsigned int)syscall(SYS_GETEUID, 0, 0, 0);
}

unsigned int getegid(void) {
    return (unsigned int)syscall(SYS_GETEGID, 0, 0, 0);
}

int getusername(char *buf, unsigned int size) {
    return (int)syscall(SYS_GETUSERNAME, (long)buf, (long)size, 0);
}

int useradd(const char *name, const char *pass, unsigned int uid, unsigned int gid) {
    (void)gid;
    return (int)syscall(SYS_USERADD, (long)name, (long)pass, (long)uid);
}

int userdel(const char *name) {
    return (int)syscall(SYS_USERDEL, (long)name, 0, 0);
}

int setpasswd(const char *name, const char *newpass) {
    return (int)syscall(SYS_SETPASSWD, (long)name, (long)newpass, 0);
}

int getpasswd(const char *name, char *buf, unsigned int size) {
    return (int)syscall(SYS_GETPASSWD, (long)name, (long)buf, (long)size);
}

int usercount(void) {
    return (int)syscall(SYS_USERCOUNT, 0, 0, 0);
}

void debug_con(void) {
    syscall(SYS_DEBUG_CON, 0, 0, 0);
}

int getdents(int fd, void *buf, unsigned int count) {
    return (int)syscall(SYS_GETDENTS, (long)fd, (long)buf, (long)count);
}

int ioctl(int fd, int cmd, void *arg) {
    return (int)syscall(SYS_IOCTL, (long)fd, (long)cmd, (long)arg);
}

int access(const char *path, int mode) {
    return (int)syscall(SYS_ACCESS, (long)path, (long)mode, 0);
}

unsigned int time(unsigned int *tloc) {
    return (unsigned int)syscall(SYS_TIME, (long)tloc, 0, 0);
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    return (int)syscall(SYS_GETTIMEOFDAY, (long)tv, (long)tz, 0);
}

char *getcwd(char *buf, unsigned int size) {
    int ret = (int)syscall(SYS_GETCWD, (long)buf, (long)size, 0);
    if (ret < 0) return 0;
    return buf;
}

int chmod(const char *path, int mode) {
    return (int)syscall(SYS_CHMOD, (long)path, (long)mode, 0);
}

int setuid(unsigned int uid) {
    return (int)syscall(SYS_SETUID, (long)uid, 0, 0);
}

int setgid(unsigned int gid) {
    return (int)syscall(SYS_SETGID, (long)gid, 0, 0);
}

int fsync(int fd) {
    (void)fd;
    return 0;
}

int sync(void) {
    return 0;
}

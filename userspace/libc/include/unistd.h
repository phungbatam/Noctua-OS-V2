#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>
#include <sys/stat.h>

/* Syscall numbers */
#define SYS_OPEN       1
#define SYS_READ       2
#define SYS_WRITE      3
#define SYS_CLOSE      4
#define SYS_GETPID     5
#define SYS_EXIT       6
#define SYS_SBRK       7
#define SYS_PIPE       8
#define SYS_SLEEP      9
#define SYS_GETTICK    10
#define SYS_FORK       11
#define SYS_WAITPID    12
#define SYS_GETPPID    13
#define SYS_GETPGID    14
#define SYS_SETPGID    15
#define SYS_EXECVE     16
#define SYS_STAT       17
#define SYS_FSTAT      18
#define SYS_LSEEK      19
#define SYS_DUP        20
#define SYS_DUP2       21
#define SYS_SHMGET     22
#define SYS_SHMAT      23
#define SYS_SHMDT      24
#define SYS_SHMCTL     25
#define SYS_MSGGET     26
#define SYS_MSGSND     27
#define SYS_MSGRCV     28
#define SYS_MSGCTL     29
#define SYS_SIGACTION  30
#define SYS_SIGPROCMASK 31
#define SYS_KILL       32
#define SYS_SETPRIORITY 33
#define SYS_GETPRIORITY 34
#define SYS_CHDIR      35
#define SYS_MKDIR      36
#define SYS_RMDIR      37
#define SYS_UNAME      38
#define SYS_UNLINK     39

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* lseek whence */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* open flags */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0x40
#define O_TRUNC     0x200
#define O_APPEND    0x400

long syscall(long number, ...);

int open(const char *path, int flags);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
pid_t getpid(void);
void exit(int status);
void *sbrk(intptr_t increment);
int pipe(int pipefd[2]);
void sleep(uint32_t ms);
pid_t fork(void);
pid_t waitpid(pid_t pid, int *status, int options);
pid_t getppid(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
off_t lseek(int fd, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int chdir(const char *path);
int mkdir(const char *path);
int rmdir(const char *path);
int uname(void *buf);
int unlink(const char *path);

#endif

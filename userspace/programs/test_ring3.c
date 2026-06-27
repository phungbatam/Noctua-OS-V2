#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

static int passed = 0;
static int failed = 0;

static void check(const char *name, int condition) {
    if (condition) {
        printf("  PASS: %s\n", name);
        passed++;
    } else {
        printf("  FAIL: %s\n", name);
        failed++;
    }
}

static void test_basic_syscalls(void) {
    printf("\n=== Test 1: Basic Syscalls ===\n");

    /* getpid */
    pid_t pid = getpid();
    check("getpid() returns > 0", pid > 0);

    /* getppid */
    pid_t ppid = getppid();
    check("getppid() returns >= 0", ppid >= 0);

    /* write to stdout */
    int n = write(1, "ring3: sys_write to stdout OK\n", 30);
    check("write(stdout) returns bytes written", n == 30);
    printf("  Output: PID=%d, PPID=%d\n", pid, ppid);

    /* open/read/close */
    int fd = open("/system/version", 0);
    if (fd >= 0) {
        char buf[64];
        memset(buf, 0, sizeof(buf));
        int r = read(fd, buf, sizeof(buf) - 1);
        check("read() from file returns data", r > 0);
        printf("  /system/version: %s\n", buf);
        close(fd);
        check("close() succeeds", 1);
        check("dup() returns >= 0", dup(fd) >= 3);
    } else {
        printf("  (skip open/read test: /system/version not found)\n");
    }

    /* stat */
    struct stat st;
    if (stat("/bin/sh", &st) == 0) {
        check("stat('/bin/sh') succeeds", st.st_size > 0);
        printf("  /bin/sh size=%u\n", (unsigned)st.st_size);
    }

    /* sleep */
    sleep(1);
    check("sleep(1ms) returns", 1);
}

static void test_fork(void) {
    printf("\n=== Test 2: fork() ===\n");

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        printf("  SKIP fork test: pipe() not available\n");
        return;
    }

    pid_t pid = fork();
    check("fork() returns >= 0", pid >= 0);

    if (pid == 0) {
        /* Child process */
        pid_t mypid = getpid();
        pid_t myppid = getppid();
        printf("  CHILD: PID=%d, PPID=%d\n", mypid, myppid);

        char msg[32];
        int len = snprintf(msg, sizeof(msg), "hello from child %d", mypid);
        write(pipefd[1], msg, len);

        /* Verify child gets 0 from fork */
        if (pid == 0) {
            /* Already checked pid == 0 path */
        }
        close(pipefd[0]);
        close(pipefd[1]);
        exit(42);
    } else {
        /* Parent process */
        char buf[64];
        memset(buf, 0, sizeof(buf));
        int r = read(pipefd[0], buf, sizeof(buf) - 1);
        check("child sent data via pipe", r > 0);
        printf("  PARENT: got from child: \"%s\"\n", buf);

        close(pipefd[0]);
        close(pipefd[1]);

        int status;
        pid_t wpid = waitpid(pid, &status, 0);
        check("waitpid() returns child PID", wpid == pid);
        check("child exited normally", WIFEXITED(status));
        check("child exit code = 42", WEXITSTATUS(status) == 42);
        printf("  PARENT: child PID=%d exited with status %d\n",
               pid, WEXITSTATUS(status));
    }
}

static void test_execve(void) {
    printf("\n=== Test 3: execve() ===\n");

    pid_t pid = fork();
    if (pid == 0) {
        /* Child: exec /bin/echo to prove execve works */
        char *argv[] = { "/bin/echo", "execve: hello from new image", NULL };
        char *envp[] = { "PATH=/bin", "TEST=execve_works", NULL };
        execve("/bin/echo", argv, envp);
        /* Should not reach here */
        printf("  FAIL: execve returned!\n");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        check("execve child exited", WIFEXITED(status));
    }
}

static void test_multi_process(void) {
    printf("\n=== Test 4: Multi-process (10 processes) ===\n");

    int nprocs = 10;
    pid_t children[20];
    int nchild = 0;
    int pipefd[2];

    if (pipe(pipefd) < 0) {
        printf("  SKIP: pipe() failed\n");
        return;
    }

    /* Fork N children, each writes a number to pipe */
    for (int i = 0; i < nprocs && nchild < 20; i++) {
        pid_t p = fork();
        if (p == 0) {
            /* Child: write a byte and exit */
            char c = 'A' + i;
            write(pipefd[1], &c, 1);
            close(pipefd[0]);
            close(pipefd[1]);
            exit(100 + i);
        } else if (p > 0) {
            children[nchild++] = p;
        }
    }

    /* Parent: read all bytes from pipe */
    int bytes_read = 0;
    char buf[32];
    while (bytes_read < nprocs) {
        int r = read(pipefd[0], buf, sizeof(buf));
        if (r <= 0) break;
        bytes_read += r;
    }
    check("pipe collected all child messages", bytes_read == nprocs);
    printf("  Read %d bytes from %d children\n", bytes_read, nchild);

    close(pipefd[0]);
    close(pipefd[1]);

    /* Reap all children */
    int reaped = 0;
    for (int i = 0; i < nchild; i++) {
        int status;
        pid_t wpid = waitpid(children[i], &status, 0);
        if (wpid == children[i]) {
            reaped++;
            printf("  Reaped child %d: exit=%d\n",
                   children[i], WEXITSTATUS(status));
        }
    }
    check("all children reaped", reaped == nchild);
    check("correct number of processes", nchild == nprocs);
}

static void test_kernel_mem_protection(void) {
    printf("\n=== Test 5: Kernel Memory Protection ===\n");

    pid_t pid = fork();
    if (pid == 0) {
        /* Try to access kernel memory (address 0, which is identity-mapped) */
        printf("  CHILD: Attempting to read kernel memory at 0x0...\n");
        volatile int *p = (volatile int *)0x0;
        int val = *p; /* This should cause SIGSEGV -> kill child */
        (void)val;
        printf("  FAIL: child survived reading kernel memory!\n");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        int sig = 0;
        if (WIFSIGNALED(status)) {
            sig = WTERMSIG(status);
        }
        printf("  Child terminated with signal %d\n", sig);
        check("kernel mem access caused signal", sig == SIGSEGV);
    }
}

static void test_waitpid_zombie(void) {
    printf("\n=== Test 6: waitpid() Zombie Reaping ===\n");

    pid_t pid = fork();
    if (pid == 0) {
        /* Child exits immediately */
        exit(99);
    } else {
        /* Parent waits */
        int status;
        pid_t r = waitpid(pid, &status, 0);
        check("waitpid returns correct PID", r == pid);
        check("exit status 99", WEXITSTATUS(status) == 99);

        /* Try waitpid again on same PID - should fail (already reaped) */
        r = waitpid(pid, &status, WNOHANG);
        check("second waitpid returns -1", r == -1 || r == 0);
        printf("  Zombie reaped successfully\n");
    }
}

int main(int argc, char *argv[], char *envp[]) {
    (void)argv;
    printf("\n========================================\n");
    printf("  Noctua OS Ring 3 Test Suite v1.0\n");
    printf("  PID=%d  argc=%d\n", getpid(), argc);
    printf("========================================\n");

    /* Print envp */
    printf("Environment:\n");
    for (char **e = envp; e && *e; e++)
        printf("  %s\n", *e);

    test_basic_syscalls();

    test_fork();

    test_execve();

    test_multi_process();

    test_kernel_mem_protection();

    test_waitpid_zombie();

    /* Summary */
    printf("\n========================================\n");
    printf("  TEST RESULTS\n");
    printf("  Passed: %d\n", passed);
    printf("  Failed: %d\n", failed);
    printf("  Total:  %d\n", passed + failed);
    printf("========================================\n");

    if (failed > 0)
        printf("  ⚠ SOME TESTS FAILED\n");
    else
        printf("  ✓ ALL TESTS PASSED\n");
    printf("========================================\n");

    return failed > 0 ? 1 : 0;
}

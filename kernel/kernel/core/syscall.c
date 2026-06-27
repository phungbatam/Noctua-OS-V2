#include "syscall.h"
#include "isr.h"
#include "screen.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "proc/pid.h"
#include "fs/fat32.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "mm/paging.h"
#include "timer/pit.h"
#include "input/keyboard.h"
#include "lib/string.h"
#include "core/elf.h"
#include "ipc/shm.h"
#include "ipc/msg.h"
#include "core/auth.h"
#include "core/debug_con.h"
#include "proc/signal.h"
#include "arch/gdt.h"
#include "timer/rtc.h"

/* Forward declarations from proc/task.c */
extern int task_fork(void);
extern task_t task_pool[];
extern int task_count;

/* ---- Pipe data (shared across processes) ---- */

typedef struct {
    int id;
    int refs;
    uint8_t buf[4096];
    uint32_t head;
    uint32_t tail;
    int readers;
    int writers;
} pipe_t;

#define MAX_PIPES 16
static pipe_t pipes[MAX_PIPES];
static int pipe_count = 0;
static int pipe_next_id = 1;

void syscall_init(void) {
    /* Nothing to do now - per-task fd tables initialized in task_create */
}

static int fd_alloc(void) {
    task_t *t = task_current();
    if (!t) return -1;
    for (int i = 3; i < TASK_FD_MAX; i++) {
        if (!t->fds[i].is_open) {
            t->fds[i].is_open = 1;
            return i;
        }
    }
    return -1;
}

static task_fd_t *fd_get(int fd) {
    task_t *t = task_current();
    if (!t) return 0;
    if (fd < 0 || fd >= TASK_FD_MAX || !t->fds[fd].is_open)
        return 0;
    return &t->fds[fd];
}

/* ---- Pipe internals ---- */

static pipe_t *pipe_create(void) {
    if (pipe_count >= MAX_PIPES) return 0;
    pipe_t *p = &pipes[pipe_count++];
    p->id = pipe_next_id++;
    p->refs = 0;
    p->head = 0;
    p->tail = 0;
    p->readers = 0;
    p->writers = 0;
    return p;
}

static pipe_t *pipe_find(int id) {
    for (int i = 0; i < pipe_count; i++) {
        if (pipes[i].id == id) return &pipes[i];
    }
    return 0;
}

static int pipe_read_bytes(pipe_t *p, uint8_t *buf, uint32_t count) {
    uint32_t read = 0;
    while (read < count && p->tail != p->head) {
        buf[read++] = p->buf[p->tail % 4096];
        p->tail++;
    }
    return read;
}

static int pipe_write_bytes(pipe_t *p, const uint8_t *buf, uint32_t count) {
    uint32_t written = 0;
    while (written < count && (p->head - p->tail) < 4096) {
        p->buf[p->head % 4096] = buf[written++];
        p->head++;
    }
    return written;
}

/* ---- User stack setup for execve ---- */

#define USER_STACK_TOP  0xBFFFFFFC
#define USER_STACK_BOTTOM 0xBFA00000
#define USER_MAX_ARGS 64
#define USER_MAX_ENV  64
#define USER_STR_BUF  4096

int setup_user_stack(task_t *t, uint32_t entry_point,
                     char *const argv[], char *const envp[])
{
    /* Allocate user stack pages */
    for (uint32_t addr = USER_STACK_BOTTOM; addr < USER_STACK_TOP + PAGE_SIZE; addr += PAGE_SIZE) {
        void *phys = pmem_alloc_page();
        if (!phys) return -1;
        paging_map_page((void *)addr, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    /* Build argv/envp strings and pointers on the user stack (top-down) */
    uint32_t sp = USER_STACK_TOP;
    char str_buf[USER_STR_BUF] = {0};
    int str_pos = 0;

    /* Count argv */
    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }

    /* Count envp */
    int envc = 0;
    if (envp) {
        while (envp[envc]) envc++;
    }

    if (argc > USER_MAX_ARGS) argc = USER_MAX_ARGS;
    if (envc > USER_MAX_ENV) envc = USER_MAX_ENV;

    /* Layout on user stack (from high to low):
     *   envp strings
     *   argv strings
     *   envp pointer array (envc+1, NULL terminated)
     *   argv pointer array (argc+1, NULL terminated)
     *   argc
     *   return address (0, _start won't return)
     */

    /* Stack all strings in a temp buffer */
    str_pos = 0;

    /* Push envp strings */
    uint32_t envp_str_ptrs[USER_MAX_ENV];
    for (int i = 0; i < envc; i++) {
        int len = strlen(envp[i]) + 1;
        if (str_pos + len > USER_STR_BUF) break;
        memcpy(str_buf + str_pos, envp[i], len);
        envp_str_ptrs[i] = str_pos;
        str_pos += len;
    }

    /* Push argv strings */
    uint32_t argv_str_ptrs[USER_MAX_ARGS];
    for (int i = 0; i < argc; i++) {
        int len = strlen(argv[i]) + 1;
        if (str_pos + len > USER_STR_BUF) break;
        memcpy(str_buf + str_pos, argv[i], len);
        argv_str_ptrs[i] = str_pos;
        str_pos += len;
    }

    /* Reserve total string space on user stack */
    int str_total = (str_pos + 3) & ~3;
    sp -= str_total;
    uint32_t string_area = sp;

    /* Copy all strings to user stack */
    memcpy((void *)sp, str_buf, str_pos);

    /* Compute absolute addresses for string pointers */
    uint32_t envp_abs_ptrs[USER_MAX_ENV + 1];
    for (int i = 0; i < envc; i++) {
        envp_abs_ptrs[i] = string_area + envp_str_ptrs[i];
    }
    envp_abs_ptrs[envc] = 0;

    uint32_t argv_abs_ptrs[USER_MAX_ARGS + 1];
    for (int i = 0; i < argc; i++) {
        argv_abs_ptrs[i] = string_area + argv_str_ptrs[i];
    }
    argv_abs_ptrs[argc] = 0;

    /* Push envp[] array */
    sp -= (envc + 1) * sizeof(uint32_t);
    sp &= ~3;
    memcpy((void *)sp, envp_abs_ptrs, (envc + 1) * sizeof(uint32_t));
    uint32_t envp_ptr = sp;

    /* Push argv[] array */
    sp -= (argc + 1) * sizeof(uint32_t);
    sp &= ~3;
    memcpy((void *)sp, argv_abs_ptrs, (argc + 1) * sizeof(uint32_t));
    uint32_t argv_ptr = sp;

    /* Push argc (must be at top of stack for crt0.S _start) */
    sp -= sizeof(uint32_t);
    sp &= ~3;
    *(uint32_t *)sp = (uint32_t)argc;

    /* Set task context for user mode */
    t->context.eip = entry_point;
    t->context.useresp = sp;
    t->context.esp = sp;  /* also set esp for initial switch */
    t->context.cs = GDT_USER_CODE;  /* 0x1B, ring 3 */
    t->context.ss = GDT_USER_DATA;  /* 0x23, ring 3 */
    t->context.ds = GDT_USER_DATA;
    t->context.es = GDT_USER_DATA;
    t->context.fs = GDT_USER_DATA;
    t->context.gs = GDT_USER_DATA;
    t->context.eflags = 0x202;
    t->context.eflags = 0x202;

    (void)argv_ptr;
    (void)envp_ptr;

    return 0;
}

/* ---- Syscall dispatch ---- */

void syscall_handler(struct registers *r) {
    uint32_t n = r->eax;
    uint32_t a = r->ebx;
    uint32_t b = r->ecx;
    uint32_t c = r->edx;

    switch (n) {
    case SYS_OPEN:    r->eax = sys_open((const char *)a, (int)b);      break;
    case SYS_READ:    r->eax = sys_read((int)a, (void *)b, c);         break;
    case SYS_WRITE:   r->eax = sys_write((int)a, (const void *)b, c);  break;
    case SYS_CLOSE:   r->eax = (uint32_t)sys_close((int)a);            break;
    case SYS_GETPID:  r->eax = (uint32_t)sys_getpid();                 break;
    case SYS_EXIT:    sys_exit((int)a);                                 break;
    case SYS_SBRK:    r->eax = (uint32_t)sys_sbrk((intptr_t)a);        break;
    case SYS_PIPE:    r->eax = (uint32_t)sys_pipe((int *)a);           break;
    case SYS_SLEEP:   sys_sleep(a);                                     break;
    case SYS_GETTICK: r->eax = sys_get_tick();                         break;
    case SYS_FORK:
        /* Save caller context (syscall registers) to current task */
        current_task->context.eax = r->eax;
        current_task->context.ebx = r->ebx;
        current_task->context.ecx = r->ecx;
        current_task->context.edx = r->edx;
        current_task->context.esi = r->esi;
        current_task->context.edi = r->edi;
        current_task->context.esp = r->esp;
        current_task->context.ebp = r->ebp;
        current_task->context.eip = r->eip;
        current_task->context.eflags = r->eflags;
        current_task->context.useresp = r->useresp;
        current_task->context.cs = r->cs;
        current_task->context.ss = r->ss;
        current_task->context.ds = r->ds;
        current_task->context.es = r->es;
        current_task->context.fs = r->fs;
        current_task->context.gs = r->gs;
        r->eax = (uint32_t)sys_fork();
        break;
    case SYS_WAITPID: r->eax = (uint32_t)sys_waitpid((int)a, (int *)b, (int)c); break;
    case SYS_GETPPID: r->eax = (uint32_t)sys_getppid();                 break;
    case SYS_GETPGID: r->eax = (uint32_t)sys_getpgid((int)a);          break;
    case SYS_SETPGID: r->eax = (uint32_t)sys_setpgid((int)a, (int)b);  break;
    case SYS_EXECVE:
        /* Save caller context first, then execve will overwrite it */
        current_task->context.eax = r->eax;
        current_task->context.ebx = r->ebx;
        current_task->context.ecx = r->ecx;
        current_task->context.edx = r->edx;
        current_task->context.esi = r->esi;
        current_task->context.edi = r->edi;
        current_task->context.esp = r->esp;
        current_task->context.ebp = r->ebp;
        current_task->context.eip = r->eip;
        current_task->context.eflags = r->eflags;
        current_task->context.useresp = r->useresp;
        current_task->context.cs = r->cs;
        current_task->context.ss = r->ss;
        current_task->context.ds = r->ds;
        current_task->context.es = r->es;
        current_task->context.fs = r->fs;
        current_task->context.gs = r->gs;
        r->eax = (uint32_t)sys_execve((const char *)a, (char *const *)b, (char *const *)c);
        /* On success, modify the iret frame directly to jump to user mode */
        if ((int32_t)r->eax >= 0) {
            /* execve succeeded - we need to modify the interrupt frame
             * that will be restored by iret. The frame has:
             * [r->eip, r->cs, r->eflags, r->useresp, r->ss]
             * These have been set by sys_execve() via current_task->context
             */
            context_to_regs(&current_task->context, r);
        }
        break;
    case SYS_STAT:    r->eax = (uint32_t)sys_stat((const char *)a, (void *)b); break;
    case SYS_FSTAT:   r->eax = (uint32_t)sys_fstat((int)a, (void *)b); break;
    case SYS_LSEEK:   r->eax = sys_lseek((int)a, b, (int)c); break;
    case SYS_DUP:     r->eax = (uint32_t)sys_dup((int)a); break;
    case SYS_DUP2:    r->eax = (uint32_t)sys_dup2((int)a, (int)b); break;
    case SYS_SHMGET:  r->eax = (uint32_t)sys_shmget((int)a, b, (int)c); break;
    case SYS_SHMAT:   r->eax = (uint32_t)sys_shmat((int)a, (const void *)b, (int)c); break;
    case SYS_SHMDT:   r->eax = (uint32_t)sys_shmdt((const void *)a); break;
    case SYS_SHMCTL:  r->eax = (uint32_t)sys_shmctl((int)a, (int)b, (void *)c); break;
    case SYS_MSGGET:  r->eax = (uint32_t)sys_msgget((int)a, (int)b); break;
    case SYS_MSGSND:  r->eax = (uint32_t)sys_msgsnd((int)a, (const void *)b, c, 0); break;
    case SYS_MSGRCV:  r->eax = (uint32_t)sys_msgrcv((int)a, (void *)b, c, 0, 0); break;
    case SYS_MSGCTL:  r->eax = (uint32_t)sys_msgctl((int)a, (int)b, (void *)c); break;
    case SYS_SIGACTION: r->eax = (uint32_t)sys_sigaction((int)a, (const void *)b, (void *)c); break;
    case SYS_SIGPROCMASK: r->eax = (uint32_t)sys_sigprocmask((int)a, (const void *)b, (void *)c); break;
    case SYS_KILL:    r->eax = (uint32_t)sys_kill((int)a, (int)b); break;
    case SYS_SETPRIORITY: r->eax = (uint32_t)sys_setpriority((int)a, (int)b, (int)c); break;
    case SYS_GETPRIORITY: r->eax = (uint32_t)sys_getpriority((int)a, (int)b); break;
    case SYS_CHDIR:    r->eax = (uint32_t)sys_chdir((const char *)a);   break;
    case SYS_MKDIR:    r->eax = (uint32_t)sys_mkdir((const char *)a);   break;
    case SYS_RMDIR:    r->eax = (uint32_t)sys_rmdir((const char *)a);   break;
    case SYS_UNAME:    r->eax = (uint32_t)sys_uname((void *)a);         break;
    case SYS_UNLINK:   r->eax = (uint32_t)sys_unlink((const char *)a);  break;
    case SYS_AUTH_LOGIN: r->eax = (uint32_t)sys_auth_login((const char *)a, (const char *)b); break;
    case SYS_GETUID:   r->eax = sys_getuid();                           break;
    case SYS_GETGID:   r->eax = sys_getgid();                           break;
    case SYS_GETEUID:  r->eax = sys_geteuid();                          break;
    case SYS_GETEGID:  r->eax = sys_getegid();                          break;
    case SYS_GETUSERNAME: r->eax = (uint32_t)sys_getusername((char *)a, b); break;
    case SYS_USERADD:  r->eax = (uint32_t)sys_useradd((const char *)a, (const char *)b, c, 0); break;
    case SYS_USERDEL:  r->eax = (uint32_t)sys_userdel((const char *)a); break;
    case SYS_SETPASSWD: r->eax = (uint32_t)sys_setpasswd((const char *)a, (const char *)b); break;
    case SYS_GETPASSWD: r->eax = (uint32_t)sys_getpasswd((const char *)a, (char *)b, c); break;
    case SYS_USERCOUNT: r->eax = (uint32_t)sys_usercount();             break;
    case SYS_DEBUG_CON: sys_debug_con(); r->eax = 0;                    break;
    case SYS_GETDENTS:  r->eax = (uint32_t)sys_getdents((int)a, (void *)b, c); break;
    case SYS_IOCTL:     r->eax = (uint32_t)sys_ioctl((int)a, (int)b, (void *)c); break;
    case SYS_ACCESS:    r->eax = (uint32_t)sys_access((const char *)a, (int)b); break;
    case SYS_TIME:      r->eax = sys_time((uint32_t *)a);               break;
    case SYS_GETTIMEOFDAY: r->eax = (uint32_t)sys_gettimeofday((void *)a, (void *)b); break;
    case SYS_GETCWD:    r->eax = (uint32_t)sys_getcwd((char *)a, b);   break;
    case SYS_CHMOD:     r->eax = (uint32_t)sys_chmod((const char *)a, (int)b); break;
    case SYS_SETUID:    r->eax = (uint32_t)sys_setuid(a);               break;
    case SYS_SETGID:    r->eax = (uint32_t)sys_setgid(a);               break;
    default:          r->eax = 0xFFFFFFFF;                              break;
    }
    /* Check pending signals before returning to user mode */
    signal_check();
}

/* ---- Syscall implementations ---- */

uint32_t sys_open(const char *path, int flags) {
    if (!path) return 0xFFFFFFFF;

    int fd = fd_alloc();
    if (fd < 0) return 0xFFFFFFFF;

    task_t *t = task_current();
    if (!t) return 0xFFFFFFFF;

    vfs_node_t *node = vfs_lookup(path);
    if (!node) {
        t->fds[fd].is_open = 0;
        return 0xFFFFFFFF;
    }

    t->fds[fd].vnode = node;
    t->fds[fd].is_pipe = 0;
    t->fds[fd].pos = 0;
    t->fds[fd].flags = flags;

    return (uint32_t)fd;
}

uint32_t sys_read(int fd, void *buf, uint32_t count) {
    task_fd_t *f = fd_get(fd);
    if (!f || !buf || !count) return 0xFFFFFFFF;

    if (f->is_pipe) {
        pipe_t *p = pipe_find(f->pipe_id);
        if (!p || f->is_pipe_write) return 0xFFFFFFFF;
        return (uint32_t)pipe_read_bytes(p, (uint8_t *)buf, count);
    }

    if (fd == 0) {
        uint8_t *b = (uint8_t *)buf;
        for (uint32_t i = 0; i < count; i++) {
            int c = keyboard_getchar();
            if (c < 0) break;
            b[i] = (uint8_t)c;
        }
        return count;
    }

    if (!f->vnode) return 0xFFFFFFFF;

    vfs_node_t *node = (vfs_node_t *)f->vnode;
    if (node->f_op && node->f_op->read) {
        int ret = node->f_op->read(node, f->pos, count, buf);
        if (ret > 0) f->pos += ret;
        return ret;
    }

    return 0xFFFFFFFF;
}

uint32_t sys_write(int fd, const void *buf, uint32_t count) {
    task_fd_t *f = fd_get(fd);
    if (!f || !buf || !count) return 0xFFFFFFFF;

    if (f->is_pipe) {
        pipe_t *p = pipe_find(f->pipe_id);
        if (!p || !f->is_pipe_write) return 0xFFFFFFFF;
        return (uint32_t)pipe_write_bytes(p, (const uint8_t *)buf, count);
    }

    if (f->vnode) {
        vfs_node_t *node = (vfs_node_t *)f->vnode;
        if (node->f_op && node->f_op->write) {
            int ret = node->f_op->write(node, f->pos, count, buf);
            if (ret > 0) f->pos += ret;
            return ret;
        }
    }

    if (fd == 1 || fd == 2) {
        screen_term_write_buf((const char *)buf, count);
        return count;
    }

    return 0xFFFFFFFF;
}

int sys_close(int fd) {
    task_fd_t *f = fd_get(fd);
    if (!f) return -1;

    if (f->is_pipe) {
        pipe_t *p = pipe_find(f->pipe_id);
        if (p) {
            p->refs--;
            if (f->is_pipe_write) p->writers--;
            else p->readers--;
        }
    }

    f->is_open = 0;
    return 0;
}

int sys_getpid(void) {
    return task_current() ? task_current()->pid : 0;
}

void sys_exit(int status) {
    task_t *t = task_current();
    if (t) {
        t->state = TASK_ZOMBIE;
        t->exit_code = status;
    }
    task_yield();
    for (;;);
}

void *sys_sbrk(intptr_t increment) {
    (void)increment;
    return 0;
}

int sys_pipe(int *pipefd) {
    if (!pipefd) return -1;

    task_t *t = task_current();
    if (!t) return -1;

    pipe_t *p = pipe_create();
    if (!p) return -1;

    int rfd = fd_alloc();
    int wfd = fd_alloc();
    if (rfd < 0 || wfd < 0) return -1;

    p->refs = 2;
    p->readers = 1;
    p->writers = 1;

    t->fds[rfd].is_pipe = 1;
    t->fds[rfd].pipe_id = p->id;
    t->fds[rfd].is_pipe_write = 0;

    t->fds[wfd].is_pipe = 1;
    t->fds[wfd].pipe_id = p->id;
    t->fds[wfd].is_pipe_write = 1;

    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
}

void sys_sleep(uint32_t ms) {
    pit_sleep(ms);
}

uint32_t sys_get_tick(void) {
    return pit_get_ticks();
}

int sys_fork(void) {
    return task_fork();
}

int sys_waitpid(int pid, int *status, int options) {
    task_t *curr = task_current();
    if (!curr) return -1;

    for (;;) {
        task_t *child = curr->children;
        while (child) {
            if (pid <= 0 || child->pid == pid) {
                if (child->state == TASK_ZOMBIE) {
                    int exit_code = child->exit_code;
                    if (status) {
                        /* Encode exit status in POSIX format:
                         * exit_code >= 128 → killed by signal (exit_code - 128)
                         * exit_code < 128  → normal exit, code in high byte */
                        if (exit_code >= 128)
                            *status = (exit_code - 128) & 0x7f;
                        else
                            *status = (exit_code & 0xff) << 8;
                    }
                    int cpid = child->pid;
                    /* Reclaim PID and clean up */
                    pid_free(child->pid);
                    child->state = TASK_READY; /* mark as dead */
                    return cpid;
                }
            }
            child = child->sibling;
        }

        if (pid > 0) {
            task_t *target = task_find_by_pid(pid);
            if (!target || target->parent != curr) {
                return -1;
            }
        }

        /* Wait for any child if pid=0 or pid=-1 */
        int any_alive = 0;
        child = curr->children;
        while (child) {
            if (pid <= 0 || child->pid == pid) {
                if (child->state != TASK_ZOMBIE) {
                    any_alive = 1;
                    break;
                }
            }
            child = child->sibling;
        }

        if (!any_alive) {
            /* No matching children at all */
            if (pid > 0) return -1;
            return -1;
        }

        if (options & 1) return 0; /* WNOHANG */

        curr->state = TASK_WAITING;
        task_yield();
    }
}

int sys_getppid(void) {
    task_t *curr = task_current();
    if (!curr || !curr->parent) return 0;
    return curr->parent->pid;
}

int sys_getpgid(int pid) {
    if (pid == 0) {
        task_t *curr = task_current();
        return curr ? curr->pgid : 0;
    }
    task_t *t = task_find_by_pid(pid);
    return t ? t->pgid : -1;
}

int sys_setpgid(int pid, int pgid) {
    if (pid == 0) pid = task_current()->pid;
    if (pgid == 0) pgid = pid;
    task_set_pgid(pid, pgid);
    return 0;
}

/* ---- Stat structure ---- */
typedef struct {
    uint32_t st_dev;
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
} stat_t;

#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFMT   0170000

int sys_execve(const char *path, char *const argv[], char *const envp[]) {
    if (!path) return -1;

    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;

    elf_load_result_t result = elf_load(node);
    if (!result.success) return -1;

    task_t *t = task_current();
    if (!t) return -1;

    /* Clean up old user mappings (simplified - just map new ones) */
    /* Set up user stack with argv/envp */
    if (setup_user_stack(t, result.entry_point, argv, envp) < 0) {
        return -1;
    }

    screen_set_content_color(C_INFO);
    screen_term_write("ELF: Exec '");
    screen_term_write(path);
    screen_term_write("' entry=0x");
    char hex[16];
    int2str((int)result.entry_point, hex);
    screen_term_write(hex);
    screen_term_write("\n");

    return 0;
}

int sys_stat(const char *path, void *statbuf) {
    if (!path || !statbuf) return -1;
    
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;
    
    stat_t *st = (stat_t *)statbuf;
    memset(st, 0, sizeof(stat_t));
    
    st->st_size = node->size;
    st->st_mode = node->is_directory ? S_IFDIR : S_IFREG;
    st->st_ino = (uint32_t)node;
    
    return 0;
}

int sys_fstat(int fd, void *statbuf) {
    if (!statbuf) return -1;
    
    task_fd_t *f = fd_get(fd);
    if (!f) return -1;
    
    stat_t *st = (stat_t *)statbuf;
    memset(st, 0, sizeof(stat_t));
    
    if (f->vnode) {
        vfs_node_t *node = (vfs_node_t *)f->vnode;
        st->st_size = node->size;
        st->st_mode = node->is_directory ? S_IFDIR : S_IFREG;
        st->st_ino = (uint32_t)node;
    }
    
    return 0;
}

uint32_t sys_lseek(int fd, uint32_t offset, int whence) {
    task_fd_t *f = fd_get(fd);
    if (!f) return 0xFFFFFFFF;
    
    uint32_t new_pos;
    switch (whence) {
        case 0: new_pos = offset; break;
        case 1: new_pos = f->pos + offset; break;
        case 2:
            if (f->vnode) {
                vfs_node_t *node = (vfs_node_t *)f->vnode;
                new_pos = node->size + offset;
            } else {
                new_pos = offset;
            }
            break;
        default:
            return 0xFFFFFFFF;
    }
    
    f->pos = new_pos;
    return new_pos;
}

int sys_dup(int oldfd) {
    return sys_dup2(oldfd, -1);
}

int sys_dup2(int oldfd, int newfd) {
    task_t *t = task_current();
    if (!t) return -1;

    if (oldfd < 0 || oldfd >= TASK_FD_MAX || !t->fds[oldfd].is_open) return -1;
    
    if (newfd < 0) {
        for (int i = 3; i < TASK_FD_MAX; i++) {
            if (!t->fds[i].is_open) {
                newfd = i;
                break;
            }
        }
        if (newfd < 0) return -1;
    } else {
        if (newfd >= TASK_FD_MAX) return -1;
        if (t->fds[newfd].is_open) {
            sys_close(newfd);
        }
    }
    
    t->fds[newfd] = t->fds[oldfd];
    t->fds[newfd].is_open = 1;
    
    return newfd;
}

/* ---- Shared memory syscalls ---- */

int sys_shmget(int key, uint32_t size, int flags) {
    (void)flags;
    return shm_create(key, size);
}

void *sys_shmat(int shmid, const void *shmaddr, int shmflg) {
    (void)shmaddr;
    (void)shmflg;
    return shm_attach(shmid);
}

int sys_shmdt(const void *shmaddr) {
    return shm_detach((void *)shmaddr);
}

int sys_shmctl(int shmid, int cmd, void *buf) {
    (void)buf;
    switch (cmd) {
        case 1: return shm_delete(shmid);
        default: return -1;
    }
}

/* ---- Message queue syscalls ---- */

int sys_msgget(int key, int msgflg) {
    return msg_get(key, msgflg);
}

int sys_msgsnd(int msqid, const void *msgp, uint32_t msgsz, int msgflg) {
    return msg_send(msqid, msgp, msgsz, msgflg);
}

int sys_msgrcv(int msqid, void *msgp, uint32_t msgsz, int msgtyp, int msgflg) {
    (void)msgtyp;
    return msg_recv(msqid, msgp, msgsz, msgflg);
}

int sys_msgctl(int msqid, int cmd, void *buf) {
    return msg_ctl(msqid, cmd, buf);
}

/* ---- Signal syscalls ---- */

typedef struct {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, void *, void *);
    uint32_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
} sigaction_t;

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

int sys_sigaction(int signum, const void *act, void *oldact) {
    task_t *t = task_current();
    if (!t) return -1;
    if (signum < 1 || signum >= NSIG) return -1;
    
    if (oldact) {
        memcpy(oldact, &t->sig_handlers[signum], sizeof(sigaction_t));
    }
    if (act) {
        memcpy(&t->sig_handlers[signum], act, sizeof(sigaction_t));
    }
    return 0;
}

int sys_sigprocmask(int how, const void *set, void *oldset) {
    task_t *t = task_current();
    if (!t) return -1;
    
    if (oldset) {
        *(uint32_t *)oldset = t->sig_blocked;
    }
    if (set) {
        uint32_t mask = *(uint32_t *)set;
        switch (how) {
            case SIG_BLOCK:    t->sig_blocked |= mask; break;
            case SIG_UNBLOCK:  t->sig_blocked &= ~mask; break;
            case SIG_SETMASK:  t->sig_blocked = mask; break;
            default: return -1;
        }
    }
    return 0;
}

int sys_kill(int pid, int sig) {
    if (sig < 1 || sig >= NSIG) return -1;
    task_t *target = task_find_by_pid(pid);
    if (!target) return -1;
    target->sig_pending |= (1 << (sig - 1));
    return 0;
}

int sys_setpriority(int which, int who, int prio) {
    (void)which;
    (void)who;
    task_t *t = task_current();
    if (!t) return -1;
    if (prio < -20) prio = -20;
    if (prio > 19) prio = 19;
    t->nice = prio;
    return 0;
}

int sys_getpriority(int which, int who) {
    (void)which;
    (void)who;
    task_t *t = task_current();
    if (!t) return -1;
    return t->nice;
}

/* ---- chdir, mkdir, rmdir, uname ---- */

typedef struct {
    char sysname[64];
    char nodename[64];
    char release[64];
    char version[64];
    char machine[64];
} utsname_t;

int sys_chdir(const char *path) {
    if (!path) return -1;
    task_t *t = task_current();
    if (!t) return -1;

    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;
    if (!node->is_directory) return -1;

    strncpy(t->cwd, path, sizeof(t->cwd) - 1);
    return 0;
}

int sys_mkdir(const char *path) {
    if (!path) return -1;
    vfs_node_t *node = vfs_create_node(path, 1);
    if (!node) return -1;
    return 0;
}

int sys_rmdir(const char *path) {
    if (!path) return -1;
    return vfs_delete_node(path);
}

int sys_uname(void *buf) {
    if (!buf) return -1;
    utsname_t *u = (utsname_t *)buf;
    strcpy(u->sysname, "Noctua");
    strcpy(u->nodename, "noctua");
    strcpy(u->release, "1.0.0");
    strcpy(u->version, "Noctua OS x86 32-bit");
    strcpy(u->machine, "i386");
    return 0;
}

int sys_unlink(const char *path) {
    if (!path) return -1;
    return vfs_delete_node(path);
}

int sys_auth_login(const char *user, const char *pass) {
    return auth_login(user, pass);
}

uint32_t sys_getuid(void) {
    task_t *t = task_current();
    return t ? t->uid : 0;
}

uint32_t sys_getgid(void) {
    task_t *t = task_current();
    return t ? t->gid : 0;
}

uint32_t sys_geteuid(void) {
    task_t *t = task_current();
    return t ? t->euid : 0;
}

uint32_t sys_getegid(void) {
    task_t *t = task_current();
    return t ? t->egid : 0;
}

int sys_getusername(char *buf, uint32_t size) {
    if (!buf || size == 0) return -1;
    const char *name = auth_get_username();
    strncpy(buf, name, size - 1);
    buf[size - 1] = 0;
    return 0;
}

int sys_useradd(const char *name, const char *pass, uint32_t uid, uint32_t gid) {
    return user_add(name, pass, uid, gid);
}

int sys_userdel(const char *name) {
    return user_del(name);
}

int sys_setpasswd(const char *name, const char *newpass) {
    return user_set_password(name, newpass);
}

int sys_getpasswd(const char *name, char *buf, uint32_t size) {
    if (!name || !buf || size == 0) return -1;
    user_t *u = user_find(name);
    if (!u) return -1;
    strncpy(buf, u->pass_hash, size - 1);
    buf[size - 1] = 0;
    return 0;
}

int sys_usercount(void) {
    return user_count();
}

int sys_getdents(int fd, void *buf, uint32_t count) {
    task_fd_t *f = fd_get(fd);
    if (!f || !buf || !count) return -1;

    if (!f->vnode) return -1;
    vfs_node_t *node = (vfs_node_t *)f->vnode;
    if (!node->is_directory) return -1;

    /* Simple dirent structure matching Linux getdents */
    typedef struct {
        uint32_t d_ino;
        uint32_t d_off;
        uint16_t d_reclen;
        char     d_name[256];
    } dirent_t;

    uint32_t written = 0;
    vfs_node_t *child = node->children;

    while (child && written + sizeof(dirent_t) <= count) {
        dirent_t *d = (dirent_t *)((uint8_t *)buf + written);
        memset(d, 0, sizeof(dirent_t));
        d->d_ino = (uint32_t)child;
        d->d_off = sizeof(dirent_t);
        int nlen = strlen(child->name);
        if (nlen > 255) nlen = 255;
        memcpy(d->d_name, child->name, nlen);
        d->d_name[nlen] = 0;
        d->d_reclen = sizeof(dirent_t);

        written += d->d_reclen;
        child = child->next;
    }

    f->pos += written;
    return written;
}

int sys_ioctl(int fd, int cmd, void *arg) {
    (void)fd;
    (void)cmd;
    (void)arg;
    /* Minimal: TCGETS would return terminal attributes */
    if (cmd == 0x5401) { /* TCGETS */
        /* Return a basic termios structure */
        uint8_t *termios = (uint8_t *)arg;
        if (termios) {
            memset(termios, 0, 128);
            termios[12] = 0x0d; /* c_cc[VMIN] = 13 */
        }
        return 0;
    }
    /* TCSETS, TCSETSW, TCSETSF */
    if (cmd == 0x5402 || cmd == 0x5403 || cmd == 0x5404) {
        return 0;
    }
    /* TIOCGWINSZ - window size */
    if (cmd == 0x5413) {
        typedef struct { uint16_t row; uint16_t col; uint16_t xpix; uint16_t ypix; } winsize_t;
        winsize_t *ws = (winsize_t *)arg;
        if (ws) {
            ws->row = 25;
            ws->col = 80;
            ws->xpix = 0;
            ws->ypix = 0;
        }
        return 0;
    }
    return -1;
}

int sys_access(const char *path, int mode) {
    (void)mode;
    if (!path) return -1;
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;
    /* For now, files are always accessible if they exist */
    return 0;
}

uint32_t sys_time(uint32_t *tloc) {
    uint32_t t = rtc_get_timestamp();
    if (tloc) *tloc = t;
    return t;
}

int sys_gettimeofday(void *tv, void *tz) {
    (void)tz;
    if (!tv) return -1;
    typedef struct { uint32_t tv_sec; uint32_t tv_usec; } timeval_t;
    timeval_t *t = (timeval_t *)tv;
    t->tv_sec = rtc_get_timestamp();
    t->tv_usec = 0;
    return 0;
}

int sys_getcwd(char *buf, uint32_t size) {
    if (!buf || size == 0) return -1;
    task_t *t = task_current();
    if (!t) return -1;
    strncpy(buf, t->cwd, size - 1);
    buf[size - 1] = 0;
    return 0;
}

int sys_chmod(const char *path, int mode) {
    (void)mode;
    if (!path) return -1;
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;
    /* Permissions stored in node->permissions; stub */
    node->permissions = (uint16_t)mode;
    return 0;
}

int sys_setuid(uint32_t uid) {
    task_t *t = task_current();
    if (!t) return -1;
    t->uid = uid;
    t->euid = uid;
    return 0;
}

int sys_setgid(uint32_t gid) {
    task_t *t = task_current();
    if (!t) return -1;
    t->gid = gid;
    t->egid = gid;
    return 0;
}

void sys_debug_con(void) {
    debug_con_enter();
}



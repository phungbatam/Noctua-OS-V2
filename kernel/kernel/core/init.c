#include "init.h"
#include "screen.h"
#include "string.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "core/elf.h"
#include "core/syscall.h"
#include "fs/fat32.h"
#include "arch/gdt.h"
#include "mm/page.h"
#include "mm/paging.h"

/* enter_user_mode from hw.asm: void enter_user_mode(eip, esp, stack_seg, code_seg) */
extern void enter_user_mode(uint32_t eip, uint32_t esp,
                            uint32_t stack_seg, uint32_t code_seg);

/* Embedded userspace binaries (from objcopy -I binary) */
extern char _binary_kernel_embed_init_bin_start[];
extern char _binary_kernel_embed_init_bin_end[];
extern unsigned int _binary_kernel_embed_init_bin_size;

extern char _binary_kernel_embed_sh_bin_start[];
extern char _binary_kernel_embed_sh_bin_end[];
extern unsigned int _binary_kernel_embed_sh_bin_size;

extern char _binary_kernel_embed_test_ring3_bin_start[];
extern char _binary_kernel_embed_test_ring3_bin_end[];
extern unsigned int _binary_kernel_embed_test_ring3_bin_size;

/* Init process: the first user-space process */
static void init_process_entry(void) {
    screen_term_write("INIT: Starting user-space...\n");

    /* Try /bin/init first, then /bin/sh */
    vfs_node_t *prog = vfs_find_node("/bin/init");
    const char *prog_name = "/bin/init";
    if (!prog) {
        prog = vfs_find_node("/bin/sh");
        prog_name = "/bin/sh";
    }

    if (!prog) {
        screen_term_write("INIT: No user programs found, staying in kernel mode\n");
        for (;;) task_yield();
        return;
    }

    screen_term_write("INIT: Loading ");
    screen_term_write(prog_name);
    screen_term_write("\n");

    elf_load_result_t result = elf_load(prog);
    if (!result.success) {
        screen_term_write("INIT: ELF load failed\n");
        for (;;) task_yield();
        return;
    }

    /* Build argv and envp for the init program */
    char *argv[] = { (char *)prog_name, NULL };
    char *envp[] = {
        "PATH=/bin:/system",
        "HOME=/",
        "SHELL=sh",
        "TERM=noctua",
        NULL
    };

    /* Set up user stack and user-mode context */
    task_t *self = task_current();
    if (setup_user_stack(self, result.entry_point, argv, envp) < 0) {
        screen_term_write("INIT: Stack setup failed\n");
        for (;;) task_yield();
        return;
    }

    screen_term_write("INIT: Entering user mode at 0x");
    char hex[16];
    int2str((int)result.entry_point, hex);
    screen_term_write(hex);
    screen_term_write("\n");

    /* Switch to user mode. This function never returns. */
    enter_user_mode(result.entry_point, self->context.useresp, GDT_USER_DATA, GDT_USER_CODE);

    /* Never reached */
    for (;;) task_yield();
}

void init_start(void) {
    /* Register embedded programs in virtual filesystem */
    vfs_node_t *init_node = vfs_find_node("/bin/init");
    if (!init_node) {
        init_node = vfs_create_node("/bin/init", 0);
    }
    if (init_node) {
        init_node->size = _binary_kernel_embed_init_bin_size;
        vfs_register_embedded(init_node, _binary_kernel_embed_init_bin_start, _binary_kernel_embed_init_bin_size);
        screen_term_write("INIT: Registered embedded /bin/init (");
        char buf[8];
        int2str(_binary_kernel_embed_init_bin_size, buf);
        screen_term_write(buf);
        screen_term_write(" bytes)\n");
    }

    vfs_node_t *sh_node = vfs_find_node("/bin/sh");
    if (!sh_node) {
        sh_node = vfs_create_node("/bin/sh", 0);
    }
    if (sh_node) {
        sh_node->size = _binary_kernel_embed_sh_bin_size;
        vfs_register_embedded(sh_node, _binary_kernel_embed_sh_bin_start, _binary_kernel_embed_sh_bin_size);
    }

    vfs_node_t *test_node = vfs_find_node("/bin/test_ring3");
    if (!test_node) {
        test_node = vfs_create_node("/bin/test_ring3", 0);
    }
    if (test_node) {
        test_node->size = _binary_kernel_embed_test_ring3_bin_size;
        vfs_register_embedded(test_node, _binary_kernel_embed_test_ring3_bin_start, _binary_kernel_embed_test_ring3_bin_size);
    }

    int pid = task_create("init", init_process_entry, 5);
    if (pid < 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("INIT: Failed to create init process\n");
    } else {
        screen_set_content_color(C_INFO);
        screen_term_write("INIT: Created init process (PID ");
        char buf[8];
        int2str(pid, buf);
        screen_term_write(buf);
        screen_term_write(")\n");
    }
}

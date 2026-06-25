#include "debug_con.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "printk.h"
#include "vsprintf.h"
#include "proc/task.h"
#include "mm/page.h"
#include "arch/ports.h"

#define MAX_BREAKPOINTS 8
#define MAX_HISTORY 16

typedef struct {
    uint32_t addr;
    int active;
} breakpoint_t;

static int active = 0;
static char cmd_buf[128];
static int cmd_pos = 0;
static char history[MAX_HISTORY][128];
static int hist_count = 0;
static int hist_idx = 0;
static breakpoint_t breakpoints[MAX_BREAKPOINTS];

static const char * const help_text[] = {
    "Noctua OS Kernel Debug Console",
    "--------------------------------",
    "Commands:",
    "  h, help     - show this help",
    "  r, regs     - show CPU registers (current task)",
    "  m, mem <addr> [count] - show memory at address",
    "  s, stack [depth] - show stack trace",
    "  t, tasks    - list all tasks",
    "  b, boot     - show boot log",
    "  w, write <addr> <val> - write 32-bit value to memory",
    "  c, cont     - continue execution",
    "  q, quit     - exit debug console",
    "  bp, break <addr> - set breakpoint",
    "  bc, breakclear <idx> - clear breakpoint",
    "  bl, breaklist - list breakpoints",
    "  dmesg       - show kernel log",
    "  cls         - clear screen",
    0
};

void debug_con_init(void) {
    memset(breakpoints, 0, sizeof(breakpoints));
    memset(history, 0, sizeof(history));
    active = 0;
    printk("DEBUG: Debug console initialized");
}

void debug_con_enter(void) {
    screen_set_content_color(C_WIN_TITLE);
    screen_term_write("\n*** KERNEL DEBUG CONSOLE ***\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Type 'h' for help, 'q' to quit\n\n");

    active = 1;
    cmd_pos = 0;
    memset(cmd_buf, 0, sizeof(cmd_buf));

    while (active) {
        screen_set_content_color(C_HEADER);
        screen_term_write("debug> ");
        screen_set_content_color(C_INPUT);

        cmd_pos = 0;
        memset(cmd_buf, 0, sizeof(cmd_buf));

        while (1) {
            int k = keyboard_getchar();
            if (k == '\n') {
                screen_term_write("\n");
                break;
            }
            if (k == '\b' && cmd_pos > 0) {
                cmd_pos--;
                screen_term_write("\b \b");
                continue;
            }
            if (k == K_UP) {
                if (hist_count > 0) {
                    hist_idx = (hist_idx > 0) ? hist_idx - 1 : hist_count - 1;
                    strcpy(cmd_buf, history[hist_idx]);
                    cmd_pos = strlen(cmd_buf);
                    screen_term_write("\rdebug> ");
                    screen_term_write(cmd_buf);
                    for (int i = cmd_pos; i < 120; i++) screen_term_write(" ");
                    screen_term_write("\rdebug> ");
                    screen_term_write(cmd_buf);
                }
                continue;
            }
            if (k == K_DOWN) {
                if (hist_count > 0) {
                    hist_idx = (hist_idx < hist_count - 1) ? hist_idx + 1 : 0;
                    strcpy(cmd_buf, history[hist_idx]);
                    cmd_pos = strlen(cmd_buf);
                    screen_term_write("\rdebug> ");
                    screen_term_write(cmd_buf);
                    for (int i = cmd_pos; i < 120; i++) screen_term_write(" ");
                    screen_term_write("\rdebug> ");
                    screen_term_write(cmd_buf);
                }
                continue;
            }
            if (k >= 0x20 && k < 0x80 && cmd_pos < 120) {
                cmd_buf[cmd_pos++] = k;
                char s[2] = {(char)k, 0};
                screen_term_write(s);
            }
        }

        if (cmd_pos > 0) {
            strcpy(history[hist_count % MAX_HISTORY], cmd_buf);
            hist_count++;
            hist_idx = hist_count % MAX_HISTORY;
        }

        char cmd[64];
        char arg[64];
        cmd[0] = 0;
        arg[0] = 0;

        int i = 0;
        while (cmd_buf[i] == ' ') i++;
        int j = 0;
        while (cmd_buf[i] && cmd_buf[i] != ' ' && j < 63) cmd[j++] = cmd_buf[i++];
        cmd[j] = 0;
        while (cmd_buf[i] == ' ') i++;
        j = 0;
        while (cmd_buf[i] && j < 63) arg[j++] = cmd_buf[i++];
        arg[j] = 0;

        screen_set_content_color(C_INFO);

        if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
            debug_con_show_help();
        } else if (strcmp(cmd, "r") == 0 || strcmp(cmd, "regs") == 0) {
            debug_con_show_regs();
        } else if (strcmp(cmd, "m") == 0 || strcmp(cmd, "mem") == 0) {
            uint32_t addr = 0;
            int count = 64;
            int n = 0;
            if (arg[0]) {
                while (arg[n] >= '0' && arg[n] <= '9') {
                    addr = addr * 16 + (arg[n] >= 'a' ? arg[n] - 'a' + 10 : arg[n] >= 'A' ? arg[n] - 'A' + 10 : arg[n] - '0');
                    n++;
                }
                if (arg[n] == ' ') {
                    n++;
                    count = 0;
                    while (arg[n] >= '0' && arg[n] <= '9') {
                        count = count * 10 + (arg[n] - '0');
                        n++;
                    }
                }
            }
            debug_con_show_memory(addr, count > 256 ? 256 : count);
        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "stack") == 0) {
            int depth = 16;
            if (arg[0]) {
                depth = 0;
                int n = 0;
                while (arg[n] >= '0' && arg[n] <= '9') depth = depth * 10 + (arg[n++] - '0');
            }
            debug_con_show_stack(depth);
        } else if (strcmp(cmd, "t") == 0 || strcmp(cmd, "tasks") == 0) {
            debug_con_show_tasks();
        } else if (strcmp(cmd, "b") == 0 || strcmp(cmd, "boot") == 0) {
            debug_con_show_boot_log();
        } else if (strcmp(cmd, "w") == 0 || strcmp(cmd, "write") == 0) {
            uint32_t addr = 0;
            uint32_t val = 0;
            int n = 0;
            if (arg[0]) {
                while (arg[n] >= '0' && arg[n] <= '9') {
                    addr = addr * 16 + (arg[n] >= 'a' ? arg[n] - 'a' + 10 : arg[n] >= 'A' ? arg[n] - 'A' + 10 : arg[n] - '0');
                    n++;
                }
                if (arg[n] == ' ') {
                    n++;
                    val = 0;
                    while (arg[n] >= '0' && arg[n] <= '9') {
                        val = val * 16 + (arg[n] >= 'a' ? arg[n] - 'a' + 10 : arg[n] >= 'A' ? arg[n] - 'A' + 10 : arg[n] - '0');
                        n++;
                    }
                }
            }
            debug_con_write_mem(addr, val);
        } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "cont") == 0 || strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            screen_set_content_color(C_WIN_TITLE);
            screen_term_write("Leaving debug console...\n");
            screen_set_content_color(C_INFO);
            active = 0;
        } else if (strcmp(cmd, "bp") == 0 || strcmp(cmd, "break") == 0) {
            uint32_t addr = 0;
            int n = 0;
            while (arg[n] >= '0' && arg[n] <= '9') {
                addr = addr * 16 + (arg[n] >= 'a' ? arg[n] - 'a' + 10 : arg[n] >= 'A' ? arg[n] - 'A' + 10 : arg[n] - '0');
                n++;
            }
            debug_con_set_breakpoint(addr);
        } else if (strcmp(cmd, "bc") == 0 || strcmp(cmd, "breakclear") == 0) {
            int idx = 0;
            int n = 0;
            while (arg[n] >= '0' && arg[n] <= '9') idx = idx * 10 + (arg[n++] - '0');
            debug_con_clear_breakpoint(idx);
        } else if (strcmp(cmd, "bl") == 0 || strcmp(cmd, "breaklist") == 0) {
            debug_con_show_breakpoints();
        } else if (strcmp(cmd, "dmesg") == 0) {
            extern void klog_dump(void);
            klog_dump();
        } else if (strcmp(cmd, "cls") == 0) {
            screen_clear_content();
        } else if (cmd[0] == 0) {
        } else {
            screen_set_content_color(C_ERROR);
            screen_term_write("Unknown command: ");
            screen_term_write(cmd);
            screen_term_write("\n");
        }
    }
}

int debug_con_active(void) {
    return active;
}

void debug_con_show_help(void) {
    for (int i = 0; help_text[i]; i++) {
        screen_term_write(help_text[i]);
        screen_term_write("\n");
    }
}

void debug_con_show_regs(void) {
    task_t *t = task_current();
    if (!t) {
        screen_term_write("No current task\n");
        return;
    }
    char buf[16];
    screen_term_write("Task: ");
    screen_term_write(t->name);
    screen_term_write(" (PID: ");
    int2str(t->pid, buf);
    screen_term_write(buf);
    screen_term_write(")\n");

    screen_term_write("EAX: 0x");
    int2str_hex(t->context.eax, buf);
    screen_term_write(buf);
    screen_term_write("  EBX: 0x");
    int2str_hex(t->context.ebx, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write("ECX: 0x");
    int2str_hex(t->context.ecx, buf);
    screen_term_write(buf);
    screen_term_write("  EDX: 0x");
    int2str_hex(t->context.edx, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write("ESI: 0x");
    int2str_hex(t->context.esi, buf);
    screen_term_write(buf);
    screen_term_write("  EDI: 0x");
    int2str_hex(t->context.edi, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write("ESP: 0x");
    int2str_hex(t->context.esp, buf);
    screen_term_write(buf);
    screen_term_write("  EBP: 0x");
    int2str_hex(t->context.ebp, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write("EIP: 0x");
    int2str_hex(t->context.eip, buf);
    screen_term_write(buf);
    screen_term_write("  EFLAGS: 0x");
    int2str_hex(t->context.eflags, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write("CS: 0x");
    int2str_hex(t->context.cs, buf);
    screen_term_write(buf);
    screen_term_write("  DS: 0x");
    int2str_hex(t->context.ds, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write("State: ");
    switch (t->state) {
        case TASK_READY: screen_term_write("READY"); break;
        case TASK_RUNNING: screen_term_write("RUNNING"); break;
        case TASK_BLOCKED: screen_term_write("BLOCKED"); break;
        case TASK_SLEEPING: screen_term_write("SLEEPING"); break;
        case TASK_WAITING: screen_term_write("WAITING"); break;
        case TASK_ZOMBIE: screen_term_write("ZOMBIE"); break;
        default: screen_term_write("UNKNOWN"); break;
    }
    screen_term_write("\n");
}

void debug_con_show_memory(uint32_t addr, int count) {
    char buf[16];
    count = (count + 15) & ~15;
    for (int i = 0; i < count; i += 16) {
        screen_term_write("0x");
        int2str_hex(addr + i, buf);
        screen_term_write(buf);
        screen_term_write(": ");
        for (int j = 0; j < 16; j++) {
            if (i + j < count) {
                uint8_t byte = *(volatile uint8_t *)(addr + i + j);
                int2str_hex(byte, buf);
                if (strlen(buf) == 1) screen_term_write("0");
                screen_term_write(buf);
                screen_term_write(" ");
            } else {
                screen_term_write("   ");
            }
        }
        screen_term_write(" |");
        for (int j = 0; j < 16; j++) {
            if (i + j < count) {
                char c = *(volatile char *)(addr + i + j);
                if (c >= 0x20 && c < 0x7F) {
                    char s[2] = {c, 0};
                    screen_term_write(s);
                } else {
                    screen_term_write(".");
                }
            }
        }
        screen_term_write("|\n");
    }
}

void debug_con_show_stack(int depth) {
    task_t *t = task_current();
    if (!t) {
        screen_term_write("No current task\n");
        return;
    }
    uint32_t *stack = (uint32_t *)t->context.esp;
    char buf[16];

    screen_term_write("Stack (ESP=0x");
    int2str_hex(t->context.esp, buf);
    screen_term_write(buf);
    screen_term_write("):\n");

    for (int i = 0; i < depth; i++) {
        screen_term_write("  0x");
        int2str_hex((uint32_t)&stack[i], buf);
        screen_term_write(buf);
        screen_term_write(": 0x");
        int2str_hex(stack[i], buf);
        screen_term_write(buf);
        screen_term_write("\n");
    }
}

void debug_con_show_tasks(void) {
    char buf[16];
    screen_term_write(" PID  NAME            STATE      PRIORITY\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = task_get(i);
        if (!t) continue;
        int2str(t->pid, buf);
        screen_term_write(" ");
        screen_term_write(buf);
        if (t->pid < 10) screen_term_write(" ");
        screen_term_write("  ");
        screen_term_write(t->name);
        for (int j = strlen(t->name); j < 14; j++) screen_term_write(" ");
        const char *s = "ready";
        if (t->state == TASK_RUNNING) s = "running";
        else if (t->state == TASK_BLOCKED) s = "blocked";
        else if (t->state == TASK_SLEEPING) s = "sleeping";
        else if (t->state == TASK_ZOMBIE) s = "zombie";
        screen_term_write(s);
        for (int j = strlen(s); j < 10; j++) screen_term_write(" ");
        int2str(t->priority, buf);
        screen_term_write(buf);
        screen_term_write("\n");
    }
}

void debug_con_show_boot_log(void) {
    extern void klog_dump(void);
    klog_dump();
}

void debug_con_write_mem(uint32_t addr, uint32_t value) {
    if (addr == 0) {
        screen_term_write("Cannot write to address 0\n");
        return;
    }
    *(volatile uint32_t *)addr = value;
    char buf[16];
    screen_term_write("Wrote 0x");
    int2str_hex(value, buf);
    screen_term_write(buf);
    screen_term_write(" to 0x");
    int2str_hex(addr, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void debug_con_set_breakpoint(uint32_t addr) {
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (!breakpoints[i].active) {
            breakpoints[i].addr = addr;
            breakpoints[i].active = 1;
            char buf[16];
            screen_term_write("Breakpoint set at 0x");
            int2str_hex(addr, buf);
            screen_term_write(buf);
            screen_term_write("\n");
            return;
        }
    }
    screen_term_write("No free breakpoint slots\n");
}

void debug_con_clear_breakpoint(int index) {
    if (index < 0 || index >= MAX_BREAKPOINTS || !breakpoints[index].active) {
        screen_term_write("Invalid breakpoint index\n");
        return;
    }
    breakpoints[index].active = 0;
    char buf[16];
    screen_term_write("Breakpoint ");
    int2str(index, buf);
    screen_term_write(buf);
    screen_term_write(" cleared\n");
}

void debug_con_show_breakpoints(void) {
    int found = 0;
    char buf[16];
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (breakpoints[i].active) {
            screen_term_write(" ");
            int2str(i, buf);
            screen_term_write(buf);
            screen_term_write(": 0x");
            int2str_hex(breakpoints[i].addr, buf);
            screen_term_write(buf);
            screen_term_write("\n");
            found = 1;
        }
    }
    if (!found) screen_term_write("No breakpoints set\n");
}

#include "emu/monitor.h"
#include "core/commands.h"
#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"
#include "keyboard.h"
#include "serial.h"
#include "debug_con.h"

/* Classic shell command dispatcher */
extern void execute(const char *cmd);

/* ============================================================
   VM Shell Bytecode Program
   Runs INSIDE the virtual CPU as a mini OS kernel.
   Single-key interactive shell.
   ============================================================ */

/*
   Memory layout:
     0x0000: program
     0x0080: string table
     
   String addresses:
     0x0080: "VM OS v1.0\n\0"
     0x0090: "> \0"
     0x00A0: "Help: [h]elp [1]cnt [2]fib [3]calc [e]xit\n\0"
     0x00D0: "Hello from VM!\n\0"
     0x00E0: "?\n\0"
     0x0100: "Running...\n\0"
     
   Program flow:
     1. Print welcome string
     2. Print prompt "> "
     3. Read key (IN R0, 1) non-blocking poll
     4. Check key:
        'h' (0x68): print help
        '1' (0x31): print 1-5 counter
        '2' (0x32): print "fib demo"
        '3' (0x33): print "5*3=15"
        'e' (0x65): HLT
        else: print "?"
     5. Loop back to prompt
*/

const uint8_t vm_shell_bin[] = {
    /* 0x0000: PRINTSTR 0x0080   welcome message */
    0x21, 0x80, 0x00,

    /* 0x0003: print prompt "> " */
    0x21, 0x90, 0x00,

    /* 0x0006: read loop - IN R0, 1 */
    0x1B, 0x00, 0x01,

    /* 0x0009: XOR R1, R1, R1 → R1 = 0 */
    0x0B, 0x01, 0x01, 0x01,

    /* 0x000D: CMP R0, R1  (check if R0 == 0) */
    0x0D, 0x00, 0x01,

    /* 0x0010: JZ 0x0006 (if no key, poll again) */
    0x13, 0x06, 0x00,

    /* 0x0013: OUT 0, R0   echo char to screen */
    0x1C, 0x00, 0x00,

    /* Compare with 'e' (0x65) for exit */
    /* 0x0016: MVI R1, 0x65 */
    0x02, 0x01, 0x65,

    /* 0x0019: CMP R0, R1 */
    0x0D, 0x00, 0x01,

    /* 0x001C: JZ 0x0050 (exit handler) */
    0x13, 0x50, 0x00,

    /* Compare with 'h' (0x68) for help */
    0x02, 0x01, 0x68,
    0x0D, 0x00, 0x01,
    0x13, 0x30, 0x00,    /* JZ help handler at 0x0030 */

    /* Compare with '1' (0x31) for counter */
    0x02, 0x01, 0x31,
    0x0D, 0x00, 0x01,
    0x13, 0x40, 0x00,    /* JZ counter handler at 0x0040 */

    /* Compare with '2' (0x32) for fib */
    0x02, 0x01, 0x32,
    0x0D, 0x00, 0x01,
    0x13, 0x60, 0x00,    /* JZ fib handler at 0x0060 */

    /* Compare with '3' (0x33) for calc */
    0x02, 0x01, 0x33,
    0x0D, 0x00, 0x01,
    0x13, 0x70, 0x00,    /* JZ calc handler at 0x0070 */

    /* Unknown key: newline + prompt */
    0x21, 0xE0, 0x00,    /* PRINTSTR 0x00E0 "\n" */
    0x12, 0x03, 0x00,    /* JMP 0x0003 (prompt) */

    /* ===== 0x0030: help handler ===== */
    0x21, 0xA0, 0x00,    /* PRINTSTR 0x00A0 help text */
    0x12, 0x03, 0x00,    /* JMP 0x0003 (back to prompt) */

    /* ===== 0x0040: counter handler ===== */
    /* MVI R0, 1       start = 1 */
    0x02, 0x00, 0x01,
    /* MVI R1, 6       limit = 6 */
    0x02, 0x01, 0x06,
    /* 0x0046: loop: INT 1 (print R0 as decimal) */
    0x1D, 0x01,
    /* INT 0 (newline) */
    0x1D, 0x00,
    /* INC R0 */
    0x0E, 0x00,
    /* CMP R0, R1 */
    0x0D, 0x00, 0x01,
    /* JNZ 0x0046 (loop if R0 != 6) */
    0x14, 0x46, 0x00,
    0x12, 0x03, 0x00,    /* JMP 0x0003 */

    /* ===== 0x0050: exit handler ===== */
    /* Print "Running..." and return to monitor */
    0x21, 0x00, 0x01,    /* PRINTSTR 0x0100 */
    0x1E,                /* HLT */

    /* ===== 0x0060: fib handler ===== */
    /* Simple: just print "fib demo" text */
    0x21, 0xB0, 0x00,    /* PRINTSTR at 0x00B0 (will be "Fib: 1 1 2 3 5\n") */
    0x12, 0x03, 0x00,

    /* ===== 0x0070: calc handler ===== */
    /* Print "5*3=15" */
    0x21, 0xC0, 0x00,
    0x12, 0x03, 0x00,

    /* ===== String table at 0x0080 ===== */
    /* Filled in via MVI/PRINTSTR - stored as separate data below */
};

/* Strings loaded into memory separately */
static void load_shell_strings(cpu_t *cpu) {
    const char *s;
    uint16_t addr;

    addr = 0x0080; s = "VM OS v1.0\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x0090; s = "> "; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x00A0; s = "Help: [h]elp [1]cnt [2]fib [3]calc [e]xit\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x00B0; s = "Fib: 1 1 2 3 5\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x00C0; s = "5*3=15\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x00D0; s = "Hello!\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x00E0; s = "\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
    addr = 0x0100; s = "Exiting VM...\n"; while (*s) cpu->memory[addr++] = *s++; cpu->memory[addr] = 0;
}

/* ============================================================
   Monitor implementation
   ============================================================ */

#define MAX_PROGRAMS 16
#define MAX_INPUT 128

static monitor_t *g_mon = 0;

void monitor_init(monitor_t *m) {
    cpu_init(&m->cpu);
    m->running = 1;
    m->auto_run = 0;
    m->boot_prog = 0;
    m->load_addr = 0x0000;
    m->step_mode = 0;
    m->bp_count = 0;
    g_mon = m;
}

void monitor_register_program(monitor_t *m, const vm_program_t *prog) {
    (void)m;
    (void)prog;
}

void monitor_reset_vm(monitor_t *m) {
    cpu_init(&m->cpu);
}

void monitor_print_regs(monitor_t *m) {
    cpu_dump(&m->cpu);
}

void monitor_print_mem(monitor_t *m, uint16_t addr, uint16_t len) {
    cpu_print_program(&m->cpu, addr, len);
}

void monitor_print_status(monitor_t *m) {
    char buf[16];
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM Status ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" State: ");
    screen_term_write(m->cpu.halted ? "HALTED" : m->cpu.running ? "RUNNING" : "STOPPED");
    screen_term_write("\n PC: 0x");
    int2str_hex(m->cpu.pc, buf);
    screen_term_write(buf);
    screen_term_write("  SP: 0x");
    int2str_hex(m->cpu.sp, buf);
    screen_term_write(buf);
    screen_term_write("  Cycles: ");
    int2str(m->cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("  Step mode: ");
    screen_term_write(m->step_mode ? "ON" : "OFF");
    screen_term_write("\n");
}

void monitor_run_vm(monitor_t *m) {
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[VM] Running...\n");
    screen_set_content_color(C_INFO);
    cpu_run(&m->cpu);
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[VM] Halted. Cycles: ");
    char buf[16];
    int2str(m->cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void monitor_step_vm(monitor_t *m) {
    int ret = cpu_step(&m->cpu);
    if (ret < 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Error: Invalid instruction at 0x");
        char buf[16];
        int2str_hex(m->cpu.pc, buf);
        screen_term_write(buf);
        screen_term_write("\n");
        m->cpu.halted = 1;
    } else if (m->cpu.halted) {
        screen_set_content_color(C_WIN_TEXT);
        screen_term_write("Program halted.\n");
    }
}

int monitor_load(monitor_t *m, const char *name) {
    if (strcmp(name, "hello") == 0) {
        cpu_load_program(&m->cpu, demo_hello, 0, demo_hello_size);
        return 0;
    }
    if (strcmp(name, "fib") == 0) {
        cpu_load_program(&m->cpu, demo_fib, 0, demo_fib_size);
        return 0;
    }
    if (strcmp(name, "counter") == 0) {
        cpu_load_program(&m->cpu, demo_counter, 0, demo_counter_size);
        return 0;
    }
    if (strcmp(name, "calc") == 0) {
        cpu_load_program(&m->cpu, demo_calc, 0, demo_calc_size);
        return 0;
    }
    if (strcmp(name, "mul") == 0) {
        cpu_load_program(&m->cpu, demo_mul, 0, demo_mul_size);
        return 0;
    }
    if (strcmp(name, "shell") == 0 || strcmp(name, "boot") == 0) {
        cpu_load_program(&m->cpu, vm_shell_bin, 0, sizeof(vm_shell_bin));
        load_shell_strings(&m->cpu);
        return 0;
    }
    return -1;
}

static void cmd_help_list(void) {
    screen_term_write("Monitor commands:\n");
    screen_term_write("  run           Run VM program\n");
    screen_term_write("  step [n]      Single-step n instructions\n");
    screen_term_write("  regs          Show registers\n");
    screen_term_write("  regs <r> <v>  Set register (e.g., regs R0 42)\n");
    screen_term_write("  mem <addr>    Show memory at address\n");
    screen_term_write("  dump <a> <l>  Hex dump memory\n");
    screen_term_write("  load <prog>   Load program (hello/fib/counter/calc/mul/shell)\n");
    screen_term_write("  reset         Reset VM\n");
    screen_term_write("  boot [prog]   Load and run program (default: shell)\n");
    screen_term_write("  status        VM state info\n");
    screen_term_write("  asm           Assembly reference\n");
    screen_term_write("  cls           Clear screen\n");
    screen_term_write("  help          This help\n");
    screen_term_write("  shell         Switch to classic shell\n");
    screen_term_write("  (All classic shell commands also work here)\n");
}

int monitor_exec(monitor_t *m, const char *cmd_line) {
    if (!cmd_line || cmd_line[0] == 0) return MON_RET_OK;

    const char *orig_cmd = cmd_line;
    char cmd[32];
    int ci = 0;
    while (*cmd_line == ' ') cmd_line++;
    while (*cmd_line && *cmd_line > ' ' && ci < 31) cmd[ci++] = *cmd_line++;
    cmd[ci] = 0;
    while (*cmd_line == ' ') cmd_line++;
    const char *args = cmd_line;

    if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
        if (m->cpu.halted) { screen_set_content_color(C_ERROR); screen_term_write("Program halted. Use 'reset' first.\n"); return MON_RET_OK; }
        monitor_run_vm(m);
        return MON_RET_OK;
    }
    if (strcmp(cmd, "step") == 0) {
        int n = 1;
        if (args[0]) {
            n = 0;
            while (*args >= '0' && *args <= '9') { n = n * 10 + (*args - '0'); args++; }
        }
        if (n > 1000) n = 1000;
        for (int i = 0; i < n && !m->cpu.halted; i++) {
            monitor_step_vm(m);
        }
        return MON_RET_OK;
    }
    if (strcmp(cmd, "regs") == 0) {
        if (args[0] && args[1] == ' ') {
            int rn = args[1] - '0';
            if (args[0] == 'R' && rn >= 0 && rn <= 15) {
                const char *v = args + 3;
                int val = 0;
                while (*v >= '0' && *v <= '9') { val = val * 10 + (*v - '0'); v++; }
                m->cpu.regs[rn] = (uint16_t)val;
                char buf[16];
                int2str(val, buf);
                screen_term_write("R"); screen_term_write(args + 1);
                screen_term_write(" = "); screen_term_write(buf); screen_term_write("\n");
                return MON_RET_OK;
            }
        }
        monitor_print_regs(m);
        return MON_RET_OK;
    }
    if (strcmp(cmd, "mem") == 0) {
        uint16_t addr = 0;
        while (*args >= '0' && *args <= '9') { addr = addr * 10 + (*args - '0'); args++; }
        monitor_print_mem(m, addr, 64);
        return MON_RET_OK;
    }
    if (strcmp(cmd, "dump") == 0) {
        uint16_t addr = 0, len = 128;
        while (*args >= '0' && *args <= '9') { addr = addr * 10 + (*args - '0'); args++; }
        while (*args == ' ') args++;
        if (*args) { len = 0; while (*args >= '0' && *args <= '9') { len = len * 10 + (*args - '0'); args++; } }
        monitor_print_mem(m, addr, len);
        return MON_RET_OK;
    }
    if (strcmp(cmd, "load") == 0) {
        if (monitor_load(m, args) == 0) {
            screen_set_content_color(C_WIN_TEXT);
            screen_term_write("Loaded program: "); screen_term_write(args); screen_term_write("\n");
            char buf[16];
            int2str(m->cpu.pc, buf);
            screen_term_write(" PC = 0x"); int2str_hex(m->cpu.pc, buf); screen_term_write(buf); screen_term_write("\n");
        } else {
            screen_set_content_color(C_ERROR);
            screen_term_write("Unknown program: "); screen_term_write(args); screen_term_write("\n");
            screen_term_write("Available: hello, fib, counter, calc, mul, shell\n");
        }
        return MON_RET_OK;
    }
    if (strcmp(cmd, "reset") == 0) {
        monitor_reset_vm(m);
        screen_set_content_color(C_WIN_TEXT);
        screen_term_write("VM reset.\n");
        return MON_RET_OK;
    }
    if (strcmp(cmd, "boot") == 0) {
        const char *prog = args[0] ? args : "shell";
        monitor_reset_vm(m);
        if (monitor_load(m, prog) == 0) {
            screen_set_content_color(C_WIN_TEXT);
            screen_term_write("Booting: "); screen_term_write(prog); screen_term_write("\n");
            monitor_run_vm(m);
        } else {
            screen_set_content_color(C_ERROR);
            screen_term_write("Cannot boot: "); screen_term_write(prog); screen_term_write("\n");
        }
        return MON_RET_OK;
    }
    if (strcmp(cmd, "status") == 0) {
        monitor_print_status(m);
        return MON_RET_OK;
    }
    if (strcmp(cmd, "asm") == 0) {
        cpu_demo_asm();
        return MON_RET_OK;
    }
    if (strcmp(cmd, "cls") == 0 || strcmp(cmd, "clear") == 0) {
        screen_clear_content();
        return MON_RET_OK;
    }
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        cmd_help_list();
        return MON_RET_OK;
    }
    if (strcmp(cmd, "shell") == 0) {
        return MON_RET_EXIT;
    }

    /* Delegate to classic shell for unknown commands */
    execute(orig_cmd);
    return MON_RET_OK;
}

void monitor_prompt(monitor_t *m) {
    char buf[16];
    screen_set_content_color(0x70);
    screen_term_write(" MC ");
    screen_set_content_color(C_HEADER);
    screen_term_write("[");
    screen_set_content_color(C_INFO);
    int2str_hex(m->cpu.pc, buf);
    screen_term_write(buf);
    screen_term_write("]");
    screen_set_content_color(C_PROMPT);
    screen_term_write(" > ");
    screen_set_content_color(C_INPUT);
}

void monitor_loop(monitor_t *m) {
    screen_set_content_color(C_WIN_TITLE);
    screen_term_write("\n=== Noctua OS Machine Code Monitor v1.0 ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Type 'help' for commands, 'shell' for classic shell.\n");
    screen_term_write("Type 'boot shell' to run the VM shell demo.\n\n");

    if (m->auto_run) {
        screen_set_content_color(C_HEADER);
        screen_term_write("[Boot] Loading VM shell...\n");
        monitor_reset_vm(m);
        monitor_load(m, "shell");
        screen_set_content_color(C_HEADER);
        screen_term_write("[Boot] Starting VM...\n\n");
        screen_set_content_color(C_INFO);
        cpu_run(&m->cpu);
        screen_set_content_color(C_WIN_TEXT);
        char buf[16];
        screen_term_write("\n[VM] Program finished. Cycles: ");
        int2str(m->cpu.cycles, buf);
        screen_term_write(buf);
        screen_term_write("\n\n");
        m->auto_run = 0;
    }

    char buf[128];
    int len = 0;

    monitor_prompt(m);

    while (m->running) {
        int k = keyboard_getchar_nb();
        if (k == 0) {
            k = serial_read_char_nb(COM1_PORT);
            if (k == 0) continue;
            if (k == '\r') k = '\n';
        }

        if (k == K_PGUP) {
            screen_scroll_up();
            continue;
        }
        if (k == K_PGDN) {
            screen_scroll_down();
            continue;
        }
        if (k == K_CTRL_C) {
            continue;
        }
        if (k == K_F12) {
            debug_con_enter();
            monitor_prompt(m);
            len = 0;
            continue;
        }

        if (k == '\n') {
            buf[len] = 0;
            screen_term_write("\n");
            if (len > 0) {
                int ret = monitor_exec(m, buf);
                if (ret == MON_RET_EXIT) break;
            }
            len = 0;
            monitor_prompt(m);
        } else if (k == '\b' || k == 0x7F) {
            if (len > 0) {
                len--;
                screen_term_write("\b \b");
            }
        } else if (k >= 0x20 && k < 0x80 && len < 126) {
            buf[len++] = k;
            screen_term_putchar(k);
        }
    }
}

/* External demo program declarations */
extern const uint8_t demo_hello[];
extern const uint8_t demo_fib[];
extern const uint8_t demo_counter[];
extern const uint8_t demo_calc[];
extern const uint8_t demo_mul[];

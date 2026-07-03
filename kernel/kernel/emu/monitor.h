#ifndef MONITOR_H
#define MONITOR_H

#include "cpu.h"

/* Monitor commands return values */
#define MON_RET_OK    0
#define MON_RET_FAIL -1
#define MON_RET_EXIT -2

/* Program metadata for embedded VM programs */
typedef struct {
    const char *name;
    const char *desc;
    const uint8_t *data;
    uint16_t addr;
    uint16_t size;
} vm_program_t;

/* Monitor state */
typedef struct {
    cpu_t cpu;
    int running;
    int auto_run;           /* auto-run loaded program on boot */
    const vm_program_t *boot_prog;  /* program to load on boot */
    uint16_t load_addr;     /* default load address */
    int step_mode;          /* single-step mode */
    uint8_t breakpoints[16]; /* simple breakpoint addresses (low byte) */
    int bp_count;
} monitor_t;

/* Initialize monitor with a persistent VM instance */
void monitor_init(monitor_t *m);

/* Register a program that can be loaded by name */
void monitor_register_program(monitor_t *m, const vm_program_t *prog);

/* Load a program by name into the VM */
int monitor_load(monitor_t *m, const char *name);

/* Run the main monitor REPL loop */
void monitor_loop(monitor_t *m);

/* Execute a single monitor command string */
int monitor_exec(monitor_t *m, const char *cmd);

/* Display the monitor prompt */
void monitor_prompt(monitor_t *m);

/* Run the current VM program (blocking) */
void monitor_run_vm(monitor_t *m);

/* Single-step the VM */
void monitor_step_vm(monitor_t *m);

/* Reset the VM */
void monitor_reset_vm(monitor_t *m);

/* Print register state */
void monitor_print_regs(monitor_t *m);

/* Print memory dump */
void monitor_print_mem(monitor_t *m, uint16_t addr, uint16_t len);

/* Show status */
void monitor_print_status(monitor_t *m);

/* Default boot program for auto-start */
extern const uint8_t vm_shell_bin[];
extern const uint16_t vm_shell_bin_size;

#endif /* MONITOR_H */

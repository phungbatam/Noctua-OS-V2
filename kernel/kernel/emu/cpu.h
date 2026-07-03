#ifndef NOCTUA_CPU_H
#define NOCTUA_CPU_H

#include <stdint.h>

#define NUM_REGS     16
#define MEM_SIZE     65536
#define STACK_BASE   0xFFFE

#define FLAG_ZF  (1 << 0)
#define FLAG_CF  (1 << 1)
#define FLAG_OF  (1 << 2)
#define FLAG_NF  (1 << 3)

#define PORT_PUTCHAR  0x00
#define PORT_GETCHAR  0x01
#define PORT_EXIT     0x02
#define PORT_PRINTSTR 0x03

typedef struct {
    uint16_t regs[NUM_REGS];
    uint16_t pc;
    uint16_t sp;
    uint8_t  flags;
    uint8_t  memory[MEM_SIZE];
    uint8_t  running;
    uint8_t  halted;
    uint32_t cycles;
} cpu_t;

void cpu_init(cpu_t *cpu);
int  cpu_load_program(cpu_t *cpu, const uint8_t *program, uint16_t addr, uint16_t size);
int  cpu_step(cpu_t *cpu);
void cpu_run(cpu_t *cpu);
void cpu_run_cycles(cpu_t *cpu, uint32_t max_cycles);
void cpu_dump(cpu_t *cpu);
void cpu_print_program(cpu_t *cpu, uint16_t addr, uint16_t len);

void cpu_demo_hello(void);
void cpu_demo_fib(void);
void cpu_demo_counter(void);
void cpu_demo_calc(void);
void cpu_demo_mul(void);
void cpu_demo_asm(void);

extern int cpu_vm_command(const char *args);

/* Demo program bytecode arrays (for monitor direct loading) */
extern const uint8_t demo_hello[];
extern const uint8_t demo_fib[];
extern const uint8_t demo_counter[];
extern const uint8_t demo_calc[];
extern const uint8_t demo_mul[];
extern const uint16_t demo_hello_size;
extern const uint16_t demo_fib_size;
extern const uint16_t demo_counter_size;
extern const uint16_t demo_calc_size;
extern const uint16_t demo_mul_size;

#endif

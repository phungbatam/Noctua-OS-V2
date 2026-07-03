#include "cpu.h"
#include "screen.h"
#include "string.h"
#include "keyboard.h"
#include "core/commands.h"
#include "core/cmd/cmd.h"
#include "core/initcall.h"
#include "vsprintf.h"

static void set_flag(cpu_t *cpu, uint8_t flag, int cond) {
    if (cond) cpu->flags |= flag; else cpu->flags &= ~flag;
}

void cpu_init(cpu_t *cpu) {
    memset(cpu, 0, sizeof(cpu_t));
    cpu->sp = STACK_BASE;
    cpu->running = 1;
}

int cpu_load_program(cpu_t *cpu, const uint8_t *program, uint16_t addr, uint16_t size) {
    if (addr + size > MEM_SIZE) return -1;
    memcpy(cpu->memory + addr, program, size);
    return 0;
}

static uint16_t fetch16(cpu_t *cpu) {
    uint16_t val = cpu->memory[cpu->pc] | (cpu->memory[cpu->pc + 1] << 8);
    cpu->pc += 2;
    return val;
}

static uint8_t fetch8(cpu_t *cpu) {
    return cpu->memory[cpu->pc++];
}

static void push16(cpu_t *cpu, uint16_t val) {
    cpu->sp -= 2;
    cpu->memory[cpu->sp] = val & 0xFF;
    cpu->memory[cpu->sp + 1] = (val >> 8) & 0xFF;
}

static uint16_t pop16(cpu_t *cpu) {
    uint16_t val = cpu->memory[cpu->sp] | (cpu->memory[cpu->sp + 1] << 8);
    cpu->sp += 2;
    return val;
}

static uint16_t mem_read16(cpu_t *cpu, uint16_t addr) {
    return cpu->memory[addr] | (cpu->memory[addr + 1] << 8);
}

static void mem_write16(cpu_t *cpu, uint16_t addr, uint16_t val) {
    cpu->memory[addr] = val & 0xFF;
    cpu->memory[addr + 1] = (val >> 8) & 0xFF;
}

static void io_write(cpu_t *cpu, uint8_t port, uint8_t val) {
    (void)cpu;
    switch (port) {
        case PORT_PUTCHAR:
            screen_term_putchar(val);
            break;
        case PORT_EXIT:
            cpu->running = 0;
            cpu->halted = 1;
            break;
    }
}

static uint8_t io_read(cpu_t *cpu, uint8_t port) {
    (void)cpu;
    switch (port) {
        case PORT_GETCHAR:
            return (uint8_t)keyboard_getchar_nb();
        default:
            return 0;
    }
}

static void print_str(cpu_t *cpu, uint16_t addr) {
    while (cpu->memory[addr]) {
        screen_term_putchar(cpu->memory[addr++]);
    }
}

enum {
    OP_NOP  = 0x00,
    OP_MOV  = 0x01, OP_MVI  = 0x02, OP_MVA  = 0x03, OP_MAV  = 0x04,
    OP_ADD  = 0x05, OP_SUB  = 0x06, OP_MUL  = 0x07, OP_DIV  = 0x08,
    OP_AND  = 0x09, OP_OR   = 0x0A, OP_XOR  = 0x0B, OP_NOT  = 0x0C,
    OP_CMP  = 0x0D, OP_INC  = 0x0E, OP_DEC  = 0x0F,
    OP_SHL  = 0x10, OP_SHR  = 0x11,
    OP_JMP  = 0x12, OP_JZ   = 0x13, OP_JNZ  = 0x14, OP_JC   = 0x15,
    OP_JNC  = 0x16, OP_CALL = 0x17, OP_RET  = 0x18,
    OP_PUSH = 0x19, OP_POP  = 0x1A,
    OP_IN   = 0x1B, OP_OUT  = 0x1C, OP_INT  = 0x1D, OP_HLT  = 0x1E,
    OP_ADDI = 0x1F, OP_MVI16 = 0x20, OP_PRINTSTR = 0x21,
    OP_XCHG = 0x22, OP_NEG  = 0x23,
};

int cpu_step(cpu_t *cpu) {
    if (!cpu->running || cpu->halted) return 0;

    cpu->cycles++;

    uint8_t op = fetch8(cpu);
    uint8_t r1, r2, r3, imm8;
    uint16_t addr, imm16;

    switch (op) {
        case OP_NOP:
            break;

        case OP_MOV:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            cpu->regs[r1] = cpu->regs[r2];
            break;

        case OP_MVI:
            r1 = fetch8(cpu); imm8 = fetch8(cpu);
            cpu->regs[r1] = imm8;
            break;

        case OP_MVI16:
            r1 = fetch8(cpu); imm16 = fetch16(cpu);
            cpu->regs[r1] = imm16;
            break;

        case OP_MVA:
            r1 = fetch8(cpu); addr = fetch16(cpu);
            cpu->regs[r1] = mem_read16(cpu, addr);
            break;

        case OP_MAV:
            addr = fetch16(cpu); r1 = fetch8(cpu);
            mem_write16(cpu, addr, cpu->regs[r1]);
            break;

        case OP_ADD:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            { uint32_t res = (uint32_t)cpu->regs[r2] + cpu->regs[r3];
              cpu->regs[r1] = (uint16_t)res;
              set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
              set_flag(cpu, FLAG_CF, res > 0xFFFF);
              set_flag(cpu, FLAG_OF, (res & 0x8000) != ((uint32_t)cpu->regs[r1] & 0x8000)); }
            break;

        case OP_SUB:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            { int32_t res = (int32_t)cpu->regs[r2] - cpu->regs[r3];
              cpu->regs[r1] = (uint16_t)res;
              set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
              set_flag(cpu, FLAG_CF, cpu->regs[r2] < cpu->regs[r3]);
              set_flag(cpu, FLAG_NF, res < 0); }
            break;

        case OP_MUL:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            cpu->regs[r1] = (uint16_t)((uint32_t)cpu->regs[r2] * cpu->regs[r3]);
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_DIV:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            if (cpu->regs[r3] == 0) { cpu->halted = 1; return -1; }
            cpu->regs[r1] = cpu->regs[r2] / cpu->regs[r3];
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_AND:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            cpu->regs[r1] = cpu->regs[r2] & cpu->regs[r3];
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_OR:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            cpu->regs[r1] = cpu->regs[r2] | cpu->regs[r3];
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_XOR:
            r1 = fetch8(cpu); r2 = fetch8(cpu); r3 = fetch8(cpu);
            cpu->regs[r1] = cpu->regs[r2] ^ cpu->regs[r3];
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_NOT:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            cpu->regs[r1] = ~cpu->regs[r2];
            break;

        case OP_NEG:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            cpu->regs[r1] = -cpu->regs[r2];
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            set_flag(cpu, FLAG_NF, (int16_t)cpu->regs[r1] < 0);
            break;

        case OP_CMP:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            { int32_t res = (int32_t)cpu->regs[r1] - cpu->regs[r2];
              set_flag(cpu, FLAG_ZF, res == 0);
              set_flag(cpu, FLAG_CF, cpu->regs[r1] < cpu->regs[r2]);
              set_flag(cpu, FLAG_NF, res < 0); }
            break;

        case OP_INC:
            r1 = fetch8(cpu);
            cpu->regs[r1]++;
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_DEC:
            r1 = fetch8(cpu);
            cpu->regs[r1]--;
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_SHL:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            cpu->regs[r1] <<= (cpu->regs[r2] & 0x0F);
            break;

        case OP_SHR:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            cpu->regs[r1] >>= (cpu->regs[r2] & 0x0F);
            break;

        case OP_ADDI:
            r1 = fetch8(cpu); imm8 = fetch8(cpu);
            cpu->regs[r1] += imm8;
            set_flag(cpu, FLAG_ZF, cpu->regs[r1] == 0);
            break;

        case OP_XCHG:
            r1 = fetch8(cpu); r2 = fetch8(cpu);
            { uint16_t t = cpu->regs[r1]; cpu->regs[r1] = cpu->regs[r2]; cpu->regs[r2] = t; }
            break;

        case OP_JMP:
            addr = fetch16(cpu);
            cpu->pc = addr;
            break;

        case OP_JZ:
            addr = fetch16(cpu);
            if (cpu->flags & FLAG_ZF) cpu->pc = addr;
            break;

        case OP_JNZ:
            addr = fetch16(cpu);
            if (!(cpu->flags & FLAG_ZF)) cpu->pc = addr;
            break;

        case OP_JC:
            addr = fetch16(cpu);
            if (cpu->flags & FLAG_CF) cpu->pc = addr;
            break;

        case OP_JNC:
            addr = fetch16(cpu);
            if (!(cpu->flags & FLAG_CF)) cpu->pc = addr;
            break;

        case OP_CALL:
            addr = fetch16(cpu);
            push16(cpu, cpu->pc);
            cpu->pc = addr;
            break;

        case OP_RET:
            cpu->pc = pop16(cpu);
            break;

        case OP_PUSH:
            r1 = fetch8(cpu);
            push16(cpu, cpu->regs[r1]);
            break;

        case OP_POP:
            r1 = fetch8(cpu);
            cpu->regs[r1] = pop16(cpu);
            break;

        case OP_IN:
            r1 = fetch8(cpu); imm8 = fetch8(cpu);
            cpu->regs[r1] = io_read(cpu, imm8);
            break;

        case OP_OUT:
            imm8 = fetch8(cpu); r1 = fetch8(cpu);
            io_write(cpu, imm8, (uint8_t)(cpu->regs[r1] & 0xFF));
            break;

        case OP_PRINTSTR:
            addr = fetch16(cpu);
            print_str(cpu, addr);
            break;

        case OP_INT:
            imm8 = fetch8(cpu);
            switch (imm8) {
                case 0:
                    screen_term_write("\n");
                    break;
                case 1: {
                    char buf[16];
                    int2str(cpu->regs[0], buf);
                    screen_term_write(buf);
                    break;
                }
                case 2: {
                    char buf[16];
                    int2str_hex(cpu->regs[0], buf);
                    screen_term_write(buf);
                    break;
                }
            }
            break;

        case OP_HLT:
            cpu->running = 0;
            cpu->halted = 1;
            break;

        default:
            return -1;
    }
    return 0;
}

void cpu_run(cpu_t *cpu) {
    cpu->running = 1;
    cpu->halted = 0;
    while (cpu->running && !cpu->halted) {
        if (cpu_step(cpu) < 0) break;
    }
}

void cpu_run_cycles(cpu_t *cpu, uint32_t max_cycles) {
    cpu->running = 1;
    cpu->halted = 0;
    uint32_t start = cpu->cycles;
    while (cpu->running && !cpu->halted && (cpu->cycles - start) < max_cycles) {
        if (cpu_step(cpu) < 0) break;
    }
}

void cpu_dump(cpu_t *cpu) {
    char buf[16];
    screen_set_content_color(C_HEADER);
    screen_term_write("=== CPU State ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" PC: 0x");
    int2str_hex(cpu->pc, buf);
    screen_term_write(buf);
    screen_term_write("  SP: 0x");
    int2str_hex(cpu->sp, buf);
    screen_term_write(buf);
    screen_term_write("  Cycles: ");
    int2str(cpu->cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n Flags: ");
    screen_term_write(cpu->flags & FLAG_ZF ? "Z" : ".");
    screen_term_write(cpu->flags & FLAG_CF ? "C" : ".");
    screen_term_write(cpu->flags & FLAG_OF ? "O" : ".");
    screen_term_write(cpu->flags & FLAG_NF ? "N" : ".");
    screen_term_write("  State: ");
    screen_term_write(cpu->halted ? "HALTED" : cpu->running ? "RUNNING" : "STOPPED");
    screen_term_write("\n\n Registers:\n");
    for (int i = 0; i < NUM_REGS; i++) {
        screen_term_write("  R");
        if (i < 10) screen_term_write(" ");
        int2str(i, buf);
        screen_term_write(buf);
        screen_term_write(" = 0x");
        int2str_hex(cpu->regs[i], buf);
        screen_term_write(buf);
        screen_term_write(" (");
        int2str(cpu->regs[i], buf);
        screen_term_write(buf);
        screen_term_write(")");
        if (i % 4 == 3) screen_term_write("\n");
        else screen_term_write("  ");
    }
    screen_term_write("\n");
}

void cpu_print_program(cpu_t *cpu, uint16_t addr, uint16_t len) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Memory Dump ===\n");
    screen_set_content_color(C_INFO);
    char buf[16];
    for (uint16_t i = 0; i < len; i += 16) {
        int2str_hex(addr + i, buf);
        screen_term_write(" 0x");
        screen_term_write(buf);
        screen_term_write(": ");
        for (int j = 0; j < 16 && i + j < len; j++) {
            int2str_hex(cpu->memory[addr + i + j], buf);
            if (cpu->memory[addr + i + j] < 0x10) screen_term_write("0");
            screen_term_write(buf);
            screen_term_write(" ");
        }
        screen_term_write("\n");
    }
}

/* ============================================================
   Demo Programs (pre-compiled bytecode)
   ============================================================ */

const uint8_t demo_hello[] = {
    /* MVI R0..R13 with "Hello Noctua!\n" */
    0x02, 0x00, 0x48, 0x02, 0x01, 0x65, 0x02, 0x02, 0x6C, 0x02, 0x03, 0x6C,
    0x02, 0x04, 0x6F, 0x02, 0x05, 0x20, 0x02, 0x06, 0x4E, 0x02, 0x07, 0x6F,
    0x02, 0x08, 0x63, 0x02, 0x09, 0x74, 0x02, 0x0A, 0x75, 0x02, 0x0B, 0x61,
    0x02, 0x0C, 0x21, 0x02, 0x0D, 0x0A,
    /* OUT 0,R0 .. OUT 0,R13 */
    0x1C, 0x00, 0x00, 0x1C, 0x00, 0x01, 0x1C, 0x00, 0x02, 0x1C, 0x00, 0x03,
    0x1C, 0x00, 0x04, 0x1C, 0x00, 0x05, 0x1C, 0x00, 0x06, 0x1C, 0x00, 0x07,
    0x1C, 0x00, 0x08, 0x1C, 0x00, 0x09, 0x1C, 0x00, 0x0A, 0x1C, 0x00, 0x0B,
    0x1C, 0x00, 0x0C, 0x1C, 0x00, 0x0D,
    /* HLT */
    0x1E
};
const uint16_t demo_hello_size = sizeof(demo_hello);

/* Compute and print first 10 Fibonacci numbers using INT 1 (print R0) + INT 0 (newline) */
const uint8_t demo_fib[] = {
    0x02, 0x00, 0x00,       /* 00: MVI R0, 0       a=0    */
    0x02, 0x01, 0x01,       /* 03: MVI R1, 1       b=1    */
    0x02, 0x02, 0x0A,       /* 06: MVI R2, 10      count=10 */
    /* loop: */
    0x1D, 0x01,             /* 09: INT 1           print R0 */
    0x1D, 0x00,             /* 0B: INT 0           newline */
    0x05, 0x03, 0x00, 0x01, /* 0D: ADD R3,R0,R1    next=a+b */
    0x01, 0x00, 0x01,       /* 11: MOV R0,R1       a=b    */
    0x01, 0x01, 0x03,       /* 14: MOV R1,R3       b=next */
    0x0F, 0x02,             /* 17: DEC R2          count-- */
    0x14, 0x09, 0x00,       /* 19: JNZ loop        if count!=0 */
    0x1E                    /* 1C: HLT */
};
const uint16_t demo_fib_size = sizeof(demo_fib);

/* Count 1 to 10 using INT 1 + INT 0 */
const uint8_t demo_counter[] = {
    0x02, 0x00, 0x01,       /* 00: MVI R0, 1       start=1 */
    0x02, 0x01, 0x0B,       /* 03: MVI R1, 11      limit=11 */
    /* loop: */
    0x1D, 0x01,             /* 06: INT 1           print R0 */
    0x1D, 0x00,             /* 08: INT 0           newline */
    0x0E, 0x00,             /* 0A: INC R0          R0++ */
    0x0D, 0x00, 0x01,       /* 0C: CMP R0,R1       compare */
    0x14, 0x06, 0x00,       /* 0F: JNZ loop        if R0!=11 */
    0x1E                    /* 12: HLT */
};
const uint16_t demo_counter_size = sizeof(demo_counter);

/* Compute 5 * 3 = 15, print result */
const uint8_t demo_calc[] = {
    0x02, 0x00, 0x05,       /* 00: MVI R0, 5       a=5 */
    0x02, 0x01, 0x03,       /* 03: MVI R1, 3       b=3 */
    0x07, 0x02, 0x00, 0x01, /* 06: MUL R2,R0,R1    res=5*3 */
    0x01, 0x00, 0x02,       /* 0A: MOV R0,R2       R0=res */
    0x1D, 0x01,             /* 0D: INT 1           print res */
    0x1D, 0x00,             /* 0F: INT 0           newline */
    0x1E                    /* 11: HLT */
};
const uint16_t demo_calc_size = sizeof(demo_calc);

/* Compute 7 * 6 = 42, print result */
const uint8_t demo_mul[] = {
    0x02, 0x00, 0x07,       /* 00: MVI R0, 7 */
    0x02, 0x01, 0x06,       /* 03: MVI R1, 6 */
    0x07, 0x02, 0x00, 0x01, /* 06: MUL R2,R0,R1    R2=7*6 */
    0x01, 0x00, 0x02,       /* 0A: MOV R0,R2       R0=42 */
    0x1D, 0x01,             /* 0D: INT 1           print 42 */
    0x1D, 0x00,             /* 0F: INT 0           newline */
    0x1E                    /* 11: HLT */
};
const uint16_t demo_mul_size = sizeof(demo_mul);

void cpu_demo_hello(void) {
    cpu_t cpu;
    cpu_init(&cpu);
    cpu_load_program(&cpu, demo_hello, 0, sizeof(demo_hello));
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM: Hello World ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Running...\n\n");
    cpu_run(&cpu);
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[  OK  ] Program halted. Cycles: ");
    char buf[16];
    int2str(cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void cpu_demo_fib(void) {
    cpu_t cpu;
    cpu_init(&cpu);
    cpu_load_program(&cpu, demo_fib, 0, sizeof(demo_fib));
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM: Fibonacci ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("First 10 Fibonacci numbers:\n\n");
    cpu_run(&cpu);
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[  OK  ] Program halted. Cycles: ");
    char buf[16];
    int2str(cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void cpu_demo_counter(void) {
    cpu_t cpu;
    cpu_init(&cpu);
    cpu_load_program(&cpu, demo_counter, 0, sizeof(demo_counter));
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM: Counter ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Counting 10 to 1:\n\n");
    cpu_run(&cpu);
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[  OK  ] Program halted. Cycles: ");
    char buf[16];
    int2str(cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void cpu_demo_calc(void) {
    cpu_t cpu;
    cpu_init(&cpu);
    cpu_load_program(&cpu, demo_calc, 0, sizeof(demo_calc));
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM: Calculator ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("5 * 3 = ?\n\n");
    cpu_run(&cpu);
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[  OK  ] Program halted. Cycles: ");
    char buf[16];
    int2str(cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void cpu_demo_mul(void) {
    cpu_t cpu;
    cpu_init(&cpu);
    cpu_load_program(&cpu, demo_mul, 0, sizeof(demo_mul));
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM: Multiplication ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("7 * 6 = ");
    cpu_run(&cpu);
    screen_set_content_color(C_WIN_TEXT);
    screen_term_write("\n[  OK  ] Cycles: ");
    char buf[16];
    int2str(cpu.cycles, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

void cpu_demo_asm(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== VM Assembler Guide ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Noctua VM Assembly Syntax:\n\n");
    screen_term_write("  Registers: R0-R15 (16-bit)\n");
    screen_term_write("  Memory: 64KB (0x0000-0xFFFF)\n");
    screen_term_write("  Stack: grows down from 0xFFFF\n\n");
    screen_term_write("Instructions:\n");
    screen_term_write("  NOP         - No operation\n");
    screen_term_write("  MOV Rd,Rs   - Rd = Rs\n");
    screen_term_write("  MVI Rd,imm  - Rd = 8-bit immediate\n");
    screen_term_write("  MVI16 Rd,im - Rd = 16-bit immediate\n");
    screen_term_write("  MVA Rd,addr - Rd = memory[addr]\n");
    screen_term_write("  MAV addr,Rd - memory[addr] = Rd\n");
    screen_term_write("  ADD Rd,Rs,Rt - Rd = Rs + Rt\n");
    screen_term_write("  SUB Rd,Rs,Rt - Rd = Rs - Rt\n");
    screen_term_write("  MUL Rd,Rs,Rt - Rd = Rs * Rt\n");
    screen_term_write("  DIV Rd,Rs,Rt - Rd = Rs / Rt\n");
    screen_term_write("  AND/OR/XOR   - Bitwise operations\n");
    screen_term_write("  NOT/NEG      - Bitwise/arithmetic negate\n");
    screen_term_write("  CMP Rs,Rt    - Compare (sets flags)\n");
    screen_term_write("  INC/DEC Rd   - Increment/Decrement\n");
    screen_term_write("  SHL/SHR Rd,Rs - Shift left/right\n");
    screen_term_write("  JMP addr     - Unconditional jump\n");
    screen_term_write("  JZ/JNZ addr  - Conditional jump\n");
    screen_term_write("  JC/JNC addr  - Jump on carry/not carry\n");
    screen_term_write("  CALL addr    - Subroutine call\n");
    screen_term_write("  RET          - Return from subroutine\n");
    screen_term_write("  PUSH Rd      - Push register\n");
    screen_term_write("  POP Rd       - Pop register\n");
    screen_term_write("  IN Rd,port   - Read I/O port\n");
    screen_term_write("  OUT port,Rd  - Write I/O port\n");
    screen_term_write("  INT n        - Software interrupt\n");
    screen_term_write("  HLT          - Halt execution\n\n");
    screen_term_write("I/O Ports:\n");
    screen_term_write("  0x00: putchar (write byte to screen)\n");
    screen_term_write("  0x01: getchar (read key, non-blocking)\n");
    screen_term_write("  0x02: exit with code\n");
    screen_term_write("  0x03: print string at [R0]\n\n");
    screen_term_write("INT calls:\n");
    screen_term_write("  0: print newline\n");
    screen_term_write("  1: print R0 as decimal\n");
    screen_term_write("  2: print R0 as hex\n");
}

/* ============================================================
   Command Interface
   ============================================================ */

static int cmd_emu(const char *args) {
    if (!args || args[0] == 0) {
        screen_set_content_color(C_HEADER);
        screen_term_write("=== Noctua VM ===\n");
        screen_set_content_color(C_INFO);
        screen_term_write("Usage: vm <command>\n\n");
        screen_term_write("Commands:\n");
        screen_term_write("  hello     - Hello World demo\n");
        screen_term_write("  fib       - Fibonacci sequence\n");
        screen_term_write("  counter   - Countdown from 10\n");
        screen_term_write("  calc      - 5 * 3 calculator\n");
        screen_term_write("  mul       - 7 * 6 multiplication\n");
        screen_term_write("  asm       - Assembly reference\n");
        screen_term_write("  help      - This help\n");
        return CMD_RET_OK;
    }

    if (strcmp(args, "hello") == 0) { cpu_demo_hello(); return CMD_RET_OK; }
    if (strcmp(args, "fib") == 0) { cpu_demo_fib(); return CMD_RET_OK; }
    if (strcmp(args, "counter") == 0) { cpu_demo_counter(); return CMD_RET_OK; }
    if (strcmp(args, "calc") == 0) { cpu_demo_calc(); return CMD_RET_OK; }
    if (strcmp(args, "mul") == 0) { cpu_demo_mul(); return CMD_RET_OK; }
    if (strcmp(args, "asm") == 0) { cpu_demo_asm(); return CMD_RET_OK; }
    if (strcmp(args, "help") == 0 || strcmp(args, "-h") == 0 || strcmp(args, "--help") == 0) {
        screen_set_content_color(C_HEADER);
        screen_term_write("=== VM Help ===\n");
        screen_set_content_color(C_INFO);
        screen_term_write("Commands: hello, fib, counter, calc, mul, asm, help\n");
        return CMD_RET_OK;
    }

    screen_set_content_color(C_ERROR);
    screen_term_write("Unknown VM command: ");
    screen_term_write(args);
    screen_term_write("\n");
    screen_term_write("Use 'vm help' for available commands.\n");
    return CMD_RET_FAIL;
}

int cpu_vm_command(const char *args) {
    return cmd_emu(args);
}

static int emu_init(void) {
    static command_t emu_cmd = CMD_FLAG(
        "vm",
        cmd_emu,
        "Noctua Virtual Machine - CPU emulator with demos",
        "vm <hello|fib|counter|calc|mul|asm|help>",
        CMD_CAT_NOCTUA,
        "1.0"
    );
    cmd_register(&emu_cmd);
    return 0;
}

late_initcall(emu_init);

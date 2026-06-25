#ifndef TVN_DEBUG_CON_H
#define TVN_DEBUG_CON_H

#include <stdint.h>

void debug_con_init(void);
void debug_con_enter(void);
int debug_con_active(void);
void debug_con_printf(const char *fmt, ...);

void debug_con_show_regs(void);
void debug_con_show_memory(uint32_t addr, int count);
void debug_con_show_stack(int depth);
void debug_con_show_tasks(void);
void debug_con_show_boot_log(void);
void debug_con_show_help(void);
void debug_con_write_mem(uint32_t addr, uint32_t value);
void debug_con_step(void);
void debug_con_continue(void);
void debug_con_set_breakpoint(uint32_t addr);
void debug_con_clear_breakpoint(int index);
void debug_con_show_breakpoints(void);

#endif

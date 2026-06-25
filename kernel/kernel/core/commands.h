#ifndef COMMANDS_H
#define COMMANDS_H

/* Dispatch a command line through the OOP command registry.
 * Returns 0 if command was found and executed, -1 if not found. */
int cmd_dispatch(const char *cmd_line);

/* Display available commands by category */
void cmd_show_noctua_commands(void);

/* Init functions: called during kernel boot to register commands */
void cmd_noctua_init(void);
void cmd_linux_init(void);
void cmd_dev_init(void);

#endif

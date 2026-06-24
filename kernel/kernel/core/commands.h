#ifndef COMMANDS_H
#define COMMANDS_H

/* Dispatch a command line through the 200-entry command table.
 * Returns 0 if command was found and executed, -1 if not found. */
int cmd_dispatch(const char *cmd_line);

/* Display available Noctua-OS unique commands */
void cmd_show_noctua_commands(void);

#endif

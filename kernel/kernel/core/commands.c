#include "commands.h"
#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"

void cmd_show_noctua_commands(void) {
    cmd_show_category(CMD_CAT_NOCTUA);
}

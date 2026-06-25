#ifndef CMD_OOP_H
#define CMD_OOP_H

#define CMD_NAME_MAX 32
#define CMD_DESC_MAX 64

typedef enum {
    CMD_CAT_LINUX   = 0,
    CMD_CAT_NOCTUA  = 1,
    CMD_CAT_DEV     = 2,
    CMD_CAT_SYSTEM  = 3,
    CMD_CAT_MAX
} cmd_category_t;

typedef struct command {
    const char *name;
    int (*handler)(const char *args);
    const char *description;
    const char *usage;
    cmd_category_t category;
    const char *version;
    struct command *next;
} command_t;

int cmd_dispatch(const char *cmd_line);
void cmd_register(command_t *cmd);
void cmd_show_category(cmd_category_t cat);
void cmd_help_all(void);
const command_t *cmd_find(const char *name);

const char *cmd_cat_name(cmd_category_t cat);
int cmd_has_flag(const char *args, char flag);

#define CMD_RET_OK    0
#define CMD_RET_FAIL  -1

#define CMD_FLAG(name, handler, desc, usage, cat, ver) \
    {name, handler, desc, usage, cat, ver, NULL}

#endif

#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"
#include "printk.h"

static command_t *cmd_list = 0;

void cmd_register(command_t *cmd) {
    if (!cmd) return;
    cmd->next = cmd_list;
    cmd_list = cmd;
}

const command_t *cmd_find(const char *name) {
    for (command_t *c = cmd_list; c; c = c->next)
        if (strcmp(c->name, name) == 0)
            return c;
    return 0;
}

int cmd_dispatch(const char *cmd_line) {
    if (!cmd_line || cmd_line[0] == 0) return CMD_RET_FAIL;
    char cmd_name[CMD_NAME_MAX];
    const char *args;
    int i = 0;
    while (cmd_line[i] && cmd_line[i] > ' ' && i < CMD_NAME_MAX - 1) {
        cmd_name[i] = cmd_line[i];
        i++;
    }
    cmd_name[i] = 0;
    args = cmd_line + i;
    while (*args == ' ' || *args == '\t') args++;
    const command_t *c = cmd_find(cmd_name);
    if (c && c->handler) return c->handler(args);
    return CMD_RET_FAIL;
}

const char *cmd_cat_name(cmd_category_t cat) {
    static const char *names[] = {
        "Linux Compat", "Noctua Native", "Development", "System"
    };
    if (cat < CMD_CAT_MAX) return names[cat];
    return "Unknown";
}

void cmd_show_category(cmd_category_t cat) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== ");
    screen_term_write(cmd_cat_name(cat));
    screen_term_write(" Commands ===\n");
    screen_set_content_color(C_INFO);
    int count = 0;
    for (command_t *c = cmd_list; c; c = c->next) {
        if (c->category != cat) continue;
        screen_term_write(" ");
        screen_term_write(c->name);
        int pad = 16;
        const char *p = c->name;
        while (*p) { pad--; p++; }
        while (pad > 0) { screen_term_write(" "); pad--; }
        screen_term_write("- ");
        screen_term_write(c->description);
        if (c->version) { screen_term_write(" ["); screen_term_write(c->version); screen_term_write("]"); }
        screen_term_write("\n");
        count++;
    }
    char buf[16];
    int2str(count, buf);
    screen_term_write(" Total: "); screen_term_write(buf); screen_term_write("\n");
    screen_set_content_color(C_INFO);
}

void cmd_help_all(void) {
    for (int cat = 0; cat < CMD_CAT_MAX; cat++)
        cmd_show_category((cmd_category_t)cat);
}

int cmd_has_flag(const char *args, char flag) {
    if (!args) return 0;
    for (const char *p = args; *p; p++) {
        if (*p == '-' && p[1] == flag) return 1;
        if (p[0] == '-' && p[1] == '-' && p[2]) {
            const char *longopts[] = {
                "verbose","v","quiet","q","help","h","force","f",
                "recursive","r","all","a","list","l","count","c",
                "sort","s","invert","i","pretty","p",
                0
            };
            for (int o = 0; longopts[o]; o += 2) {
                const char *lp = p + 2;
                int match = 1;
                for (const char *a = longopts[o], *b = lp; *a && *b && *b != '='; a++, b++) {
                    if (*a != *b) { match = 0; break; }
                }
                if (match && longopts[o + 1][0] == flag) return 1;
            }
        }
    }
    return 0;
}

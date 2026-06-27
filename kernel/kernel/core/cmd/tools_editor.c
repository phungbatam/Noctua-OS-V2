#include "cmd/cmd.h"
#include "screen.h"
#include "string.h"
#include "editor.h"
#include "fs/fat32.h"
#include "heap.h"

static void scr(const char *s) { screen_term_write(s); }
static void scf(void) { screen_set_content_color(C_INFO); }
static void sch(void) { screen_set_content_color(C_HEADER); }

/* ---- wc: word/line/char count ---- */
static int cmd_wc(const char *args) {
    if (!args) { scr(" Usage: wc <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    int lines = 0, words = 0, chars = 0, in_word = 0;
    char buf[512]; int n;
    while ((n = file_read(f, buf, 512)) > 0) {
        for (int i = 0; i < n; i++) {
            chars++;
            if (buf[i] == '\n') lines++;
            if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t') { in_word = 0; }
            else if (!in_word) { in_word = 1; words++; }
        }
    }
    file_close(f);
    char num[16];
    scr("  "); int2str(lines, num); scr(num);
    scr(" ");  int2str(words, num); scr(num);
    scr(" ");  int2str(chars, num); scr(num);
    scr(" "); scr(path); scr("\n");
    return CMD_RET_OK;
}

/* ---- head: show first N lines of file ---- */
static int cmd_head(const char *args) {
    int n = 10;
    if (!args) { scr(" Usage: head [-n <count>] <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    if (cmd_has_flag(args, 'n')) {
        const char *p = args;
        while (*p && *p != 'n') p++;
        if (*p == 'n') { p++; while (*p == ' ' || *p == '=') p++; n = 0;
            while (*p >= '0' && *p <= '9') { n = n*10 + (*p-'0'); p++; } }
    }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ' && *args != '-') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[512]; int lines = 0;
    int total, offset = 0;
    while (lines < n && (total = file_read(f, buf, 512)) > 0) {
        for (int i = 0; i < total && lines < n; i++) {
            if (buf[i] == '\n') lines++;
            char s[2] = {buf[i], 0}; scr(s);
        }
    }
    if (lines < n && total > 0) scr("\n");
    file_close(f);
    return CMD_RET_OK;
}

/* ---- tail: show last N lines of file ---- */
static int cmd_tail(const char *args) {
    int n = 10;
    if (!args) { scr(" Usage: tail [-n <count>] <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    if (cmd_has_flag(args, 'n')) {
        const char *p = args;
        while (*p && *p != 'n') p++;
        if (*p == 'n') { p++; while (*p == ' ' || *p == '=') p++; n = 0;
            while (*p >= '0' && *p <= '9') { n = n*10 + (*p-'0'); p++; } }
    }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ' && *args != '-') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096];
    int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    int lf = 0;
    for (int i = 0; i < total; i++) if (buf[i] == '\n') lf++;
    int start_line = lf - n + 1;
    if (start_line < 0) start_line = 0;
    int lc = 0, printing = 0;
    for (int i = 0; i < total; i++) {
        if (lc >= start_line) printing = 1;
        if (buf[i] == '\n') lc++;
        if (printing) { char s[2] = {buf[i], 0}; scr(s); }
    }
    if (printing && total > 0 && buf[total-1] != '\n') scr("\n");
    return CMD_RET_OK;
}

/* ---- nl: number lines of file ---- */
static int cmd_nl(const char *args) {
    if (!args) { scr(" Usage: nl <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    int lnum = 1, col = 0;
    char num[16]; char line[256]; int li = 0; char buf[512]; int n;
    while ((n = file_read(f, buf, 512)) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                line[li] = 0;
                int2str(lnum, num); scr("     "); int len = strlen(num);
                for (int j = 0; j < 5 - len; j++) scr(" ");
                scr(num); scr("  "); scr(line); scr("\n");
                lnum++; li = 0; col = 0;
            } else if (li < 255) { line[li++] = buf[i]; }
        }
    }
    if (li > 0) { line[li] = 0; char num[16]; int2str(lnum, num); scr("     ");
        int len = strlen(num); for (int j = 0; j < 5 - len; j++) scr(" "); scr(num); scr("  "); scr(line); scr("\n"); }
    file_close(f);
    return CMD_RET_OK;
}

/* ---- tac: reverse lines of file ---- */
static int cmd_tac(const char *args) {
    if (!args) { scr(" Usage: tac <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    int ends[2048]; int ec = 0;
    for (int i = 0; i < total; i++) if (buf[i] == '\n') ends[ec++] = i;
    int prev = total;
    for (int i = ec - 1; i >= 0; i--) {
        int start = ends[i] + 1;
        for (int j = start; j < prev; j++) { char s[2] = {buf[j], 0}; scr(s); }
        scr("\n");
        prev = ends[i];
    }
    for (int j = 0; j < prev; j++) { char s[2] = {buf[j], 0}; scr(s); }
    return CMD_RET_OK;
}

/* ---- sort: sort lines of file ---- */
static int cmd_sort(const char *args) {
    int reverse = 0;
    if (!args) { scr(" Usage: sort [-r] <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    reverse = cmd_has_flag(args, 'r');
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ' && *args != '-') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    char *lines[1024]; int lc = 0;
    lines[0] = buf; lc = 1;
    for (int i = 0; i < total; i++) {
        if (buf[i] == '\n') {
            buf[i] = 0;
            if (i + 1 < total) { lines[lc++] = &buf[i+1]; if (lc >= 1024) break; }
        }
    }
    for (int i = 0; i < lc - 1; i++) {
        for (int j = i + 1; j < lc; j++) {
            int cmp = strcmp(lines[i], lines[j]);
            if (reverse) cmp = -cmp;
            if (cmp > 0) { char *tmp = lines[i]; lines[i] = lines[j]; lines[j] = tmp; }
        }
    }
    for (int i = 0; i < lc; i++) { scr(lines[i]); scr("\n"); }
    return CMD_RET_OK;
}

/* ---- uniq: remove duplicate lines ---- */
static int cmd_uniq(const char *args) {
    if (!args) { scr(" Usage: uniq <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    int lines_idx[1024]; int lc = 0; lines_idx[lc++] = 0;
    for (int i = 0; i < total; i++) {
        if (buf[i] == '\n') { buf[i] = 0; if (i + 1 < total) lines_idx[lc++] = i + 1; if (lc >= 1024) break; }
    }
    scr(lines_idx[0] + buf); scr("\n");
    for (int i = 1; i < lc; i++) {
        if (strcmp(buf + lines_idx[i], buf + lines_idx[i-1]) != 0) {
            scr(buf + lines_idx[i]); scr("\n");
        }
    }
    return CMD_RET_OK;
}

/* ---- cut: cut out selected fields ---- */
static int cmd_cut(const char *args) {
    if (!args) { scr(" Usage: cut -d<delim> -f<field> <file>\n"); return CMD_RET_OK; }
    char delim = '\t'; int field = 1;
    const char *p = args;
    while (*p) {
        if (*p == '-' && *(p+1) == 'd') { p += 2; delim = *p; if (delim) p++; }
        else if (*p == '-' && *(p+1) == 'f') { p += 2; field = 0;
            while (*p >= '0' && *p <= '9') { field = field * 10 + (*p - '0'); p++; } }
        else p++;
    }
    while (*p == ' ') p--;
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    const char *q = args;
    while (*q && *q > ' ' && *q != '-') { pi = 0; while (*q && *q > ' ' && *q != '-') path[pi++] = *q++; path[pi] = 0; }
    if (path[0] == 0) { 
        while (*args && *args > ' ') args++;
        while (*args == ' ') args++;
        pi = 0; while (*args && *args > ' ') path[pi++] = *args++; path[pi] = 0;
    }
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    char line[256]; int li = 0;
    for (int i = 0; i <= total; i++) {
        if (i == total || buf[i] == '\n') {
            line[li] = 0;
            int fcount = 1, pos = 0;
            for (int j = 0; j <= li; j++) {
                if (line[j] == delim || line[j] == 0) {
                    if (fcount == field) {
                        for (int k = pos; k < j; k++) { char s[2] = {line[k], 0}; scr(s); }
                        scr("\n"); break;
                    }
                    fcount++; pos = j + 1;
                }
            }
            li = 0;
        } else if (li < 255) { line[li++] = buf[i]; }
    }
    return CMD_RET_OK;
}

/* ---- tr: translate/delete characters ---- */
static int cmd_tr(const char *args) {
    if (!args) { scr(" Usage: tr <set1> <set2> <file>\n"); return CMD_RET_OK; }
    char set1[256] = {0}, set2[256] = {0}, path[256] = {0};
    int mode = 0, pi = 0, si = 0;
    while (*args == ' ') args++;
    for (const char *p = args; *p; p++) {
        if (*p == ' ') { if (mode == 0) { set1[si] = 0; mode = 1; si = 0; }
            else if (mode == 1) { set2[si] = 0; mode = 2; si = 0; } }
        else if (mode < 2) { set1[si++] = *p; }
        else { path[pi++] = *p; }
    }
    if (mode == 1) { set2[si] = 0; }
    if (path[0] == 0) { for (int i = 0; args[i] && args[i] > ' '; i++) path[i] = args[i]; }
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    for (int i = 0; i < total; i++) {
        unsigned char c = (unsigned char)buf[i];
        char replaced = 0;
        for (int j = 0; set1[j]; j++) {
            if ((unsigned char)set1[j] == c) {
                if (j < (int)strlen(set2)) { char s[2] = {set2[j], 0}; scr(s); }
                replaced = 1; break;
            }
        }
        if (!replaced) { char s[2] = {buf[i], 0}; scr(s); }
    }
    return CMD_RET_OK;
}

/* ---- diff: simple file comparison ---- */
static int cmd_diff(const char *args) {
    if (!args) { scr(" Usage: diff <file1> <file2>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char f1[256], f2[256]; int pi = 0;
    while (*args && *args > ' ') { f1[pi++] = *args++; } f1[pi] = 0;
    while (*args == ' ') args++; pi = 0;
    while (*args && *args > ' ') { f2[pi++] = *args++; } f2[pi] = 0;
    if (f1[0] == 0 || f2[0] == 0) { scr(" Need two files\n"); return CMD_RET_OK; }
    file_handle_t *fa = file_open(f1, 0);
    file_handle_t *fb = file_open(f2, 0);
    if (!fa || !fb) { scr(" File not found\n"); if (fa) file_close(fa); if (fb) file_close(fb); return CMD_RET_OK; }
    char buf1[4096], buf2[4096];
    int n1 = file_read(fa, buf1, 4096);
    int n2 = file_read(fb, buf2, 4096);
    file_close(fa); file_close(fb);
    if (n1 == n2 && memcmp(buf1, buf2, n1) == 0) { scr(" Files are identical\n"); return CMD_RET_OK; }
    sch(); scr("=== Diff: "); scr(f1); scr(" vs "); scr(f2); scr(" ===\n"); scf();
    int l1 = 1, l2 = 1, i1 = 0, i2 = 0;
    char line1[256], line2[256];
    int li1 = 0, li2 = 0;
    while (i1 < n1 || i2 < n2) {
        while (i1 < n1 && buf1[i1] != '\n' && li1 < 255) line1[li1++] = buf1[i1++];
        if (i1 < n1 && buf1[i1] == '\n') { line1[li1] = 0; i1++; }
        else if (i1 < n1) line1[li1] = 0;
        while (i2 < n2 && buf2[i2] != '\n' && li2 < 255) line2[li2++] = buf2[i2++];
        if (i2 < n2 && buf2[i2] == '\n') { line2[li2] = 0; i2++; }
        else if (i2 < n2) line2[li2] = 0;
        if (li1 == 0 && li2 == 0) break;
        if (strcmp(line1, line2) != 0) {
            char num[16];
            if (li1 > 0) { scr(" < "); int2str(l1, num); scr(num); scr(": "); scr(line1); scr("\n"); }
            if (li2 > 0) { scr(" > "); int2str(l2, num); scr(num); scr(": "); scr(line2); scr("\n"); }
        }
        l1++; l2++; li1 = 0; li2 = 0;
    }
    return CMD_RET_OK;
}

/* ---- sed: stream editor find/replace ---- */
static int cmd_sed(const char *args) {
    if (!args) { scr(" Usage: sed 's/old/new/g' <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char pattern[256] = {0}, replace[256] = {0}, path[256] = {0};
    int global = 0, mode = 0, pi = 0, ri = 0, pp = 0;
    if (*args == 's' && *(args+1) == '/') {
        args += 2;
        while (*args && *args != '/' && pi < 255) pattern[pi++] = *args++;
        if (*args == '/') args++;
        while (*args && *args != '/' && *args != ' ' && ri < 255) replace[ri++] = *args++;
        if (*args == '/') { args++; while (*args == 'g') { global = 1; args++; } }
        while (*args == ' ') args++;
        pi = 0; while (*args && *args > ' ' && pi < 255) path[pi++] = *args++; path[pi] = 0;
    } else {
        while (*args && *args > ' ' && pi < 255) path[pi++] = *args++; path[pi] = 0;
        scr(" Pattern must be s/old/new/[g]\n"); return CMD_RET_OK;
    }
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    char line[256]; int li = 0; int plen = strlen(pattern);
    for (int i = 0; i <= total; i++) {
        if (i == total || buf[i] == '\n') {
            line[li] = 0;
            int match_count = 0;
            for (int j = 0; j <= li - plen; ) {
                int found = 1;
                for (int k = 0; k < plen; k++) { if (line[j + k] != pattern[k]) { found = 0; break; } }
                if (found) {
                    for (int k = 0; k < j; k++) { char s[2] = {line[k], 0}; scr(s); }
                    scr(replace);
                    j += plen;
                    match_count++;
                    if (!global) {
                        for (int k = j; k <= li; k++) { char s[2] = {line[k], 0}; scr(s); }
                        goto next_line_sed;
                    }
                } else j++;
            }
            if (match_count == 0) scr(line);
            next_line_sed: scr("\n"); li = 0;
        } else if (li < 255) line[li++] = buf[i];
    }
    return CMD_RET_OK;
}

/* ---- expand: convert tabs to spaces ---- */
static int cmd_expand(const char *args) {
    if (!args) { scr(" Usage: expand <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    int col = 0;
    for (int i = 0; i < total; i++) {
        if (buf[i] == '\t') {
            int spaces = 4 - (col % 4);
            for (int j = 0; j < spaces; j++) { scr(" "); col++; }
        } else {
            char s[2] = {buf[i], 0}; scr(s);
            if (buf[i] == '\n') col = 0; else col++;
        }
    }
    return CMD_RET_OK;
}

/* ---- nano: launch editor with nano-style display ---- */
static int cmd_nano(const char *args) {
    if (!args) { scr(" Usage: nano <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    sch(); scr("=== Nano Editor Mode ===\n"); scf();
    scr(" Ctrl+O: Save  Ctrl+X: Exit  Ctrl+K: Cut\n");
    scr(" Ctrl+W: Search  Ctrl+A: Home  Ctrl+E: End\n");
    scr(" Opening: "); scr(args); scr("\n");
    editor_open(args);
    return CMD_RET_OK;
}

/* ---- vi: alias to edit with vi-style display ---- */
static int cmd_vi(const char *args) {
    if (!args) { scr(" Usage: vi <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    sch(); scr("=== Vi Editor Mode ===\n"); scf();
    scr(" i: insert  ESC: normal  :w save  :q quit\n");
    scr(" h/j/k/l: move  x: delete  yy: copy\n");
    editor_open(args);
    return CMD_RET_OK;
}

/* ---- fold: wrap long lines ---- */
static int cmd_fold(const char *args) {
    int width = 80;
    if (!args) { scr(" Usage: fold [-w <width>] <file>\n"); return CMD_RET_OK; }
    while (*args == ' ') args++;
    if (cmd_has_flag(args, 'w')) {
        const char *p = args;
        while (*p && *p != 'w') p++;
        if (*p == 'w') { p++; while (*p == ' ' || *p == '=') p++; width = 0;
            while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; } }
    }
    while (*args == ' ') args++;
    char path[256]; int pi = 0;
    while (*args && *args > ' ' && *args != '-') { path[pi++] = *args++; } path[pi] = 0;
    if (path[0] == 0) { scr(" No file\n"); return CMD_RET_OK; }
    file_handle_t *f = file_open(path, 0);
    if (!f) { scr(" File not found\n"); return CMD_RET_OK; }
    char buf[4096]; int total = file_read(f, buf, 4096);
    file_close(f);
    if (total <= 0) return CMD_RET_OK;
    int col = 0;
    for (int i = 0; i < total; i++) {
        if (buf[i] == '\n') { scr("\n"); col = 0; }
        else if (col >= width) { scr("\n"); col = 0; i--; }
        else { char s[2] = {buf[i], 0}; scr(s); col++; }
    }
    if (col > 0) scr("\n");
    return CMD_RET_OK;
}

void cmd_editor_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("wc",     cmd_wc,     "Word/line/char count", "wc <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("head",   cmd_head,   "Show first lines of file", "head [-n <N>] <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("tail",   cmd_tail,   "Show last lines of file", "tail [-n <N>] <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("nl",     cmd_nl,     "Number lines of file", "nl <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("tac",    cmd_tac,    "Reverse lines of file", "tac <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("sort",   cmd_sort,   "Sort lines of file", "sort [-r] <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("uniq",   cmd_uniq,   "Remove duplicate lines", "uniq <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("cut",    cmd_cut,    "Cut selected fields", "cut -d<delim> -f<N> <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("tr",     cmd_tr,     "Translate/delete chars", "tr <set1> <set2> <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("diff",   cmd_diff,   "Compare two files", "diff <file1> <file2>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("sed",    cmd_sed,    "Stream editor find/replace", "sed 's/old/new/g' <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("expand", cmd_expand, "Convert tabs to spaces", "expand <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("nano",   cmd_nano,   "Nano-style editor", "nano <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("vi",     cmd_vi,     "Vi-style editor", "vi <file>", CMD_CAT_LINUX, "1.0"),
        CMD_FLAG("fold",   cmd_fold,   "Wrap long lines", "fold [-w <width>] <file>", CMD_CAT_LINUX, "1.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

#include "cmd/cmd.h"
#include "screen.h"
#include "printk.h"

static int cmd_gcc(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== GCC (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" gcc (Noctua-OS) 14.2.0\n");
    screen_term_write(" Copyright (C) 2025 Free Software Foundation, Inc.\n");
    screen_term_write(" This is free software; see the source for copying conditions.\n");
    screen_term_write(" Noctua-OS: x86_64-pc-noctua-gcc\n");
    screen_term_write(" Thread model: posix\n");
    screen_term_write(" Supported LTO compression algorithms: zlib zstd\n");
    screen_term_write(" gcc version 14.2.0 (Noctua-OS 1.0) \n");
    if (cmd_has_flag(args, 'v')) {
        screen_term_write(" COLLECT_GCC=gcc\n");
        screen_term_write(" COLLECT_LTO_WRAPPER=/usr/libexec/gcc/x86_64-pc-noctua/14.2.0/lto-wrapper\n");
        screen_term_write(" Target: x86_64-pc-noctua\n");
        screen_term_write(" Configured with: ./configure --target=x86_64-pc-noctua --prefix=/usr\n");
        screen_term_write("  --enable-languages=c,c++ --disable-multilib\n");
    }
    return CMD_RET_OK;
}

static int cmd_gxx(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== G++ (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" g++ (Noctua-OS) 14.2.0\n");
    screen_term_write(" Copyright (C) 2025 Free Software Foundation, Inc.\n");
    screen_term_write(" This is free software; see the source for copying conditions.\n");
    screen_term_write(" Noctua-OS: x86_64-pc-noctua-g++\n");
    screen_term_write(" Thread model: posix\n");
    screen_term_write(" gcc version 14.2.0 (Noctua-OS 1.0) \n");
    if (cmd_has_flag(args, 'v')) {
        screen_term_write(" COLLECT_GCC=g++\n");
        screen_term_write(" Target: x86_64-pc-noctua\n");
        screen_term_write(" Supported C++ standards: c++11 c++14 c++17 c++20 c++23\n");
    }
    return CMD_RET_OK;
}

static int cmd_clang(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Clang/LLVM (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" clang version 18.1.8 (Noctua-OS)\n");
    screen_term_write(" Target: x86_64-pc-noctua-elf\n");
    screen_term_write(" Thread model: posix\n");
    screen_term_write(" InstalledDir: /usr/bin\n");
    return CMD_RET_OK;
}

static int cmd_make(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== GNU Make (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" GNU Make 4.4.1\n");
    screen_term_write(" Built for x86_64-pc-noctua\n");
    screen_term_write(" Copyright (C) 1988-2024 Free Software Foundation, Inc.\n");
    return CMD_RET_OK;
}

static int cmd_python(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Python (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" Python 3.12.7 (Noctua-OS, 2025-01-15)\n");
    screen_term_write(" [GCC 14.2.0] on linux\n");
    screen_term_write(" Type 'python -h' for help\n");
    return CMD_RET_OK;
}

static int cmd_perl(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Perl (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" This is perl 5, version 40, subversion 0 (v5.40.0)\n");
    screen_term_write(" Built for x86_64-pc-noctua\n");
    return CMD_RET_OK;
}

static int cmd_rustc(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== rustc (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" rustc 1.83.0 (Noctua-OS, 2025-02-01)\n");
    screen_term_write(" Host: x86_64-unknown-noctua\n");
    screen_term_write(" LLVM version: 18.1.8\n");
    return CMD_RET_OK;
}

static int cmd_node(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Node.js (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" v22.12.0\n");
    return CMD_RET_OK;
}

static int cmd_git(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== git (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" git version 2.47.1\n");
    return CMD_RET_OK;
}

static int cmd_docker(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Docker (Noctua-OS Edition) ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" Docker version 27.4.0, build bde2b89\n");
    screen_term_write(" Docker Desktop is not available in this environment.\n");
    return CMD_RET_OK;
}

void cmd_dev_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("gcc",     cmd_gcc,     "GNU C Compiler (version display)", "gcc [-v]", CMD_CAT_DEV, "14.2.0"),
        CMD_FLAG("g++",     cmd_gxx,     "GNU C++ Compiler (version display)", "g++ [-v]", CMD_CAT_DEV, "14.2.0"),
        CMD_FLAG("clang",   cmd_clang,   "Clang/LLVM compiler (version display)", "clang [-v]", CMD_CAT_DEV, "18.1.8"),
        CMD_FLAG("make",    cmd_make,    "GNU Make (version display)", "make", CMD_CAT_DEV, "4.4.1"),
        CMD_FLAG("python",  cmd_python,  "Python interpreter (version display)", "python", CMD_CAT_DEV, "3.12.7"),
        CMD_FLAG("python3", cmd_python,  "Python 3 interpreter (version display)", "python3", CMD_CAT_DEV, "3.12.7"),
        CMD_FLAG("perl",    cmd_perl,    "Perl interpreter (version display)", "perl", CMD_CAT_DEV, "5.40.0"),
        CMD_FLAG("rustc",   cmd_rustc,   "Rust compiler (version display)", "rustc", CMD_CAT_DEV, "1.83.0"),
        CMD_FLAG("node",    cmd_node,    "Node.js (version display)", "node", CMD_CAT_DEV, "22.12.0"),
        CMD_FLAG("git",     cmd_git,     "Git version control (version display)", "git", CMD_CAT_DEV, "2.47.1"),
        CMD_FLAG("docker",  cmd_docker,  "Docker container (version display)", "docker", CMD_CAT_DEV, "27.4.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

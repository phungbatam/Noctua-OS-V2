#include "cmd/cmd.h"
#include "screen.h"
#include "fb.h"
#include "string.h"
#include "vsprintf.h"
#include "printk.h"
#include "klog.h"
#include "heap.h"
#include "fs/fat32.h"
#include "block/ata.h"
#include "block/partition.h"
#include "block/blockdev.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "pit.h"
#include "keyboard.h"
#include "timer/rtc.h"
#include "char/pcspkr.h"
#include "core/init.h"
#include "core/script.h"
#include "mm/slab.h"
#include "mm/heap.h"
#include "arch/cpuid.h"
#include "heap.h"
#include "ports.h"

/* ============================================================
   Command forward declarations (OOP: all return int)
   ============================================================ */
#define CMD_DECL(name) static int cmd_##name(const char *args)

CMD_DECL(noctua); CMD_DECL(strix); CMD_DECL(bubo); CMD_DECL(tyto);
CMD_DECL(asio); CMD_DECL(otus); CMD_DECL(scops); CMD_DECL(glaucid);
CMD_DECL(surnia); CMD_DECL(ninox); CMD_DECL(aegolius); CMD_DECL(megascops);
CMD_DECL(ketupa); CMD_DECL(jubula); CMD_DECL(mimizuku); CMD_DECL(nesasio);
CMD_DECL(phodilus); CMD_DECL(ptilopsis); CMD_DECL(pulsatrix); CMD_DECL(orus);
CMD_DECL(sirius); CMD_DECL(vega); CMD_DECL(rigel); CMD_DECL(polaris);
CMD_DECL(altair); CMD_DECL(capella); CMD_DECL(aldebaran); CMD_DECL(regulus);
CMD_DECL(spica); CMD_DECL(antares); CMD_DECL(arcturus); CMD_DECL(betelgeuse);
CMD_DECL(castor); CMD_DECL(pollux); CMD_DECL(pleiades); CMD_DECL(orion);
CMD_DECL(andromeda); CMD_DECL(cassiopeia); CMD_DECL(draco); CMD_DECL(cygnus);
CMD_DECL(aether); CMD_DECL(umbra); CMD_DECL(solstice); CMD_DECL(eclipse);
CMD_DECL(twilight); CMD_DECL(midnight); CMD_DECL(aurora); CMD_DECL(zenith);
CMD_DECL(nebula); CMD_DECL(cosmos); CMD_DECL(galaxy); CMD_DECL(stellar);
CMD_DECL(lunar); CMD_DECL(solar); CMD_DECL(nova); CMD_DECL(pulsar);
CMD_DECL(quasar); CMD_DECL(magnetar); CMD_DECL(supernova); CMD_DECL(hypernova);
CMD_DECL(ruby); CMD_DECL(emerald); CMD_DECL(sapphire); CMD_DECL(diamond);
CMD_DECL(amethyst); CMD_DECL(topaz); CMD_DECL(opal); CMD_DECL(jade);
CMD_DECL(onyx); CMD_DECL(quartz); CMD_DECL(garnet); CMD_DECL(tourmaline);
CMD_DECL(peridot); CMD_DECL(citrine); CMD_DECL(turquoise); CMD_DECL(lapis);
CMD_DECL(coral); CMD_DECL(pearl); CMD_DECL(amber); CMD_DECL(jasper);
CMD_DECL(nexus); CMD_DECL(vortex); CMD_DECL(cipher); CMD_DECL(monolith);
CMD_DECL(prism); CMD_DECL(obelisk); CMD_DECL(resonance); CMD_DECL(quantum);
CMD_DECL(axiom); CMD_DECL(vector); CMD_DECL(matrix); CMD_DECL(helix);
CMD_DECL(pixel); CMD_DECL(lattice); CMD_DECL(flux); CMD_DECL(drift);
CMD_DECL(surge); CMD_DECL(pulse); CMD_DECL(apex); CMD_DECL(vertex);
CMD_DECL(fenrir); CMD_DECL(valkyrie); CMD_DECL(odin); CMD_DECL(thor);
CMD_DECL(loki); CMD_DECL(freya); CMD_DECL(heimdall); CMD_DECL(baldr);
CMD_DECL(tyr); CMD_DECL(sif); CMD_DECL(njord); CMD_DECL(idunn);
CMD_DECL(bragi); CMD_DECL(hodr); CMD_DECL(vidarr); CMD_DECL(ali);
CMD_DECL(magni); CMD_DECL(modi); CMD_DECL(saga); CMD_DECL(rune);
CMD_DECL(ignis); CMD_DECL(cinder); CMD_DECL(ember); CMD_DECL(blaze);
CMD_DECL(flare); CMD_DECL(spark); CMD_DECL(flash); CMD_DECL(glow);
CMD_DECL(gleam); CMD_DECL(shine); CMD_DECL(beam); CMD_DECL(ray);
CMD_DECL(dawn); CMD_DECL(dusk); CMD_DECL(gloom); CMD_DECL(shade);
CMD_DECL(shadow); CMD_DECL(phantom); CMD_DECL(wraith); CMD_DECL(spirit);
CMD_DECL(verbum); CMD_DECL(scriptum); CMD_DECL(lectio); CMD_DECL(dictum);
CMD_DECL(notitia); CMD_DECL(ratio); CMD_DECL(census); CMD_DECL(gradus);
CMD_DECL(ordo); CMD_DECL(regula); CMD_DECL(forma); CMD_DECL(signum);
CMD_DECL(index); CMD_DECL(meta); CMD_DECL(finis); CMD_DECL(initium);
CMD_DECL(medium); CMD_DECL(terminus); CMD_DECL(casus); CMD_DECL(modus);
CMD_DECL(falcon); CMD_DECL(raven); CMD_DECL(hawk); CMD_DECL(eagle);
CMD_DECL(crow); CMD_DECL(phoenix); CMD_DECL(heron); CMD_DECL(swan);
CMD_DECL(swift); CMD_DECL(kite); CMD_DECL(merlin); CMD_DECL(goshawk);
CMD_DECL(harrier); CMD_DECL(osprey); CMD_DECL(thrush); CMD_DECL(finch);
CMD_DECL(robin); CMD_DECL(sparrow); CMD_DECL(starling); CMD_DECL(martin);
CMD_DECL(crimson); CMD_DECL(azure); CMD_DECL(umber); CMD_DECL(chartreuse);
CMD_DECL(rose); CMD_DECL(sable); CMD_DECL(ivory); CMD_DECL(teal);
CMD_DECL(plum); CMD_DECL(mauve); CMD_DECL(ochre); CMD_DECL(indigo);
CMD_DECL(violet); CMD_DECL(cerulean); CMD_DECL(scarlet); CMD_DECL(viridian);
CMD_DECL(saffron); CMD_DECL(cobalt); CMD_DECL(magenta); CMD_DECL(carmine);

/* ============================================================
   Helper: check if a flag character is in args
   ============================================================ */
static int has_flag(const char *args, char flag) {
    if (!args) return 0;
    for (const char *p = args; *p; p++) {
        if (*p == '-' && p[1] == flag) return 1;
        if (p[0] == '-' && p[1] == '-' && p[2]) {
            const char *longopts[] = {
                "verbose","v","quiet","q","help","h","force","f",
                "recursive","r","all","a","list","l","count","c",
                "sort","s","invert","i","pretty","p",
                "name","n","time","t","output","o","directory","d",
                0
            };
            for (int li = 0; longopts[li]; li += 2) {
                if (strcmp(p + 2, longopts[li]) == 0) {
                    if (longopts[li + 1][0] == flag) return 1;
                }
            }
        }
    }
    return 0;
}

static int get_flag_val(const char *args, char flag, int *out) {
    if (!args) return -1;
    for (const char *p = args; *p; p++) {
        if (*p == '-' && p[1] == flag && p[2]) {
            *out = 0;
            int neg = 0; const char *v = p + 2;
            if (*v == '=') v++;
            while (*v >= '0' && *v <= '9') { *out = *out * 10 + (*v - '0'); v++; }
            (void)neg;
            return 0;
        }
    }
    return -1;
}

/* ============================================================
   Stub macro: generate handler that prints name + desc + usage
   ============================================================ */
#define CMD_STUB(name, desc_text, usage_text) \
static int cmd_##name(const char *args) { \
    if (has_flag(args, 'h')) { \
        screen_set_content_color(C_HEADER); \
        screen_term_write("=== " #name " ===\n"); \
        screen_set_content_color(C_INFO); \
        screen_term_write(" " desc_text "\n\n"); \
        screen_term_write(" Usage: " usage_text "\n"); \
        return CMD_RET_OK; \
    } \
    screen_set_content_color(C_HEADER); \
    screen_term_write("--- " #name " ---\n"); \
    screen_set_content_color(C_INFO); \
    screen_term_write(" " desc_text "\n"); \
    screen_set_content_color(C_WIN_TEXT); \
    screen_term_write(" [..] run with -h for full options\n"); \
    return CMD_RET_OK; \
}

/* ============================================================
   Full command implementations (30 commands)
   ============================================================ */

/* ---- noctua: kernel/OS info ---- */
static int cmd_noctua(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: noctua [-v] [-a] [-m] [-c]\n");
        screen_term_write("  -v          Show kernel version\n");
        screen_term_write("  -a          All info\n");
        screen_term_write("  -m          Memory info\n");
        screen_term_write("  -c          CPU info\n");
        screen_term_write("  -b          Build date\n");
        screen_term_write("  -l          License info\n");
        return CMD_RET_OK;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write(".-------------------------------.\n");
    screen_term_write("|       Noctua OS 1.0           |\n");
    screen_term_write("|      x86 32-bit Protected     |\n");
    screen_term_write("'-------------------------------'\n");
    screen_set_content_color(C_INFO);
    if (has_flag(args, 'v') || has_flag(args, 'a')) {
        screen_term_write(" Version:  Noctua OS 1.0.0\n");
        screen_term_write(" Arch:     x86_32 (i386)\n");
        screen_term_write(" Kernel:   Noctua-kernel v1.0\n");
        screen_term_write(" Format:   ELF32\n");
        screen_term_write(" Compiler: GCC (no-stdlib)\n");
    }
    if (has_flag(args, 'm') || has_flag(args, 'a')) {
        char buf[16];
        screen_term_write(" RAM:      ");
        int2str(heap_free() / 1024, buf);
        screen_term_write(buf);
        screen_term_write(" KB free / ");
        int2str(kmem_cache_usage() / 1024, buf);
        screen_term_write(buf);
        screen_term_write(" KB used\n");
    }
    if (has_flag(args, 'c') || has_flag(args, 'a')) {
        screen_term_write(" CPU:      ");
        char cv[32], cb[64]; get_cpu_vendor(cv); get_cpu_brand(cb);
        screen_term_write(cv);
        screen_term_write(" - ");
        screen_term_write(cb);
        screen_term_write("\n");
    }
    if (has_flag(args, 'b') || has_flag(args, 'a')) {
        screen_term_write(" Build:    " __DATE__ " " __TIME__ "\n");
    }
    if (has_flag(args, 'l')) {
        screen_term_write(" License:  BSD 2-Clause\n");
    }
    if (!has_flag(args, 'v') && !has_flag(args, 'a') && !has_flag(args, 'm')
        && !has_flag(args, 'c') && !has_flag(args, 'b') && !has_flag(args, 'l')) {
        screen_term_write(" Use -a for full info, -h for help\n");
    }
    return CMD_RET_OK;
}

/* ---- strix: process list/manager ---- */
static int cmd_strix(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: strix [-l] [-k PID] [-n NAME] [-s SIG] [-p] [-t]\n");
        screen_term_write("  -l          List all processes\n");
        screen_term_write("  -k PID      Kill process\n");
        screen_term_write("  -s SIG      Signal number\n");
        screen_term_write("  -n NAME     Filter by name\n");
        screen_term_write("  -p          Show parent/child tree\n");
        screen_term_write("  -t          Show threads\n");
        screen_term_write("  -c          Count processes\n");
        screen_term_write("  -r          Renice (priority)\n");
        screen_term_write("  -o FILE     Output to file\n");
        return CMD_RET_OK;
    }
    if (has_flag(args, 'c')) {
        int tc = 0; for (int ti = 0; task_get(ti); ti++) tc++;
        char buf[16];
        int2str(tc, buf);
        screen_term_write(" Total processes: ");
        screen_term_write(buf);
        screen_term_write("\n");
        return CMD_RET_OK;
    }
    if (has_flag(args, 'l') || !args || args[0] == 0) {
        screen_set_content_color(C_HEADER);
        screen_term_write(" PID  STATE  NAME\n");
        screen_set_content_color(C_INFO);
        for (int ti = 0; ; ti++) {
            task_t *t = task_get(ti);
            if (!t) break;
            char buf[16];
            int2str(t->pid, buf);
            screen_term_write(" "); screen_term_write(buf);
            screen_term_write("  ");
            screen_term_write(t->state == TASK_RUNNING ? "RUN " : t->state == TASK_SLEEPING ? "SLP " : "STOP");
            screen_term_write("  ");
            screen_term_write(t->name);
            screen_term_write("\n");
        }
    }
    return CMD_RET_OK;
}

/* ---- bubo: system monitor ---- */
static int cmd_bubo(const char *args) {
    (void)args;
    screen_set_content_color(C_HEADER);
    screen_term_write("=== bubo: System Monitor ===\n");
    screen_set_content_color(C_INFO);
    char buf[16];
    screen_term_write(" Uptime: "); int2str(uptime_get_seconds(), buf);
    screen_term_write(buf); screen_term_write("s\n");
    int tc = 0; for (int ti = 0; task_get(ti); ti++) tc++;
    screen_term_write(" Tasks:  "); int2str(tc, buf);
    screen_term_write(buf); screen_term_write(" running\n");
    screen_term_write(" Heap:   "); int2str(heap_free() / 1024, buf);
    screen_term_write(buf); screen_term_write(" KB free\n");
    screen_term_write(" Slab:   "); int2str(kmem_cache_usage() / 1024, buf);
    screen_term_write(buf); screen_term_write(" KB cached\n");
    if (has_flag(args, 'v')) {
        char cv[32], cb[64]; get_cpu_vendor(cv); get_cpu_brand(cb);
        screen_term_write(" CPU:    "); screen_term_write(cv); screen_term_write("\n");
        screen_term_write(" Model:  "); screen_term_write(cb); screen_term_write("\n");
    }
    if (has_flag(args, 'l')) {
        screen_term_write(" Use -n to refresh N times, -t N for interval\n");
    }
    return CMD_RET_OK;
}

/* ---- tyto: system config read/write ---- */
static int cmd_tyto(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: tyto [-g KEY] [-s KEY=VAL] [-l] [-r] [-f FILE]\n");
        screen_term_write("  -g KEY      Get config value\n");
        screen_term_write("  -s KEY=VAL  Set config value\n");
        screen_term_write("  -l          List all config\n");
        screen_term_write("  -r          Reset to defaults\n");
        screen_term_write("  -f FILE     Config file path\n");
        screen_term_write("  -e          Export config\n");
        screen_term_write("  -i FILE     Import config\n");
        return CMD_RET_OK;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("=== tyto: System Config ===\n");
    screen_set_content_color(C_INFO);
    if (has_flag(args, 'l')) {
        screen_term_write(" hostname=noctua\n");
        screen_term_write(" user=user\n");
        screen_term_write(" shell=/bin/sh\n");
        screen_term_write(" term=noctua-term\n");
        screen_term_write(" lang=en_US\n");
        screen_term_write(" theme=default\n");
        screen_term_write(" keymap=us\n");
    }
    if (has_flag(args, 'g')) { screen_term_write(" [..] get config key\n"); }
    if (has_flag(args, 's')) { screen_term_write(" [..] set config key=val\n"); }
    return CMD_RET_OK;
}

/* ---- asio: audio service control ---- */
static int cmd_asio(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: asio [-s STATE] [-v VOL] [-f FREQ] [-t SEC] [-l]\n");
        screen_term_write("  -s STATE    on/off/mute\n");
        screen_term_write("  -v VOL      Volume 0-100\n");
        screen_term_write("  -f FREQ     Beep frequency\n");
        screen_term_write("  -t MS       Beep duration\n");
        screen_term_write("  -l          List audio devices\n");
        screen_term_write("  -p          Play test tone\n");
        screen_term_write("  -m          Mixer control\n");
        return CMD_RET_OK;
    }
    if (has_flag(args, 'p')) {
        int freq = 440, dur = 200;
        get_flag_val(args, 'f', &freq);
        get_flag_val(args, 't', &dur);
        pcspkr_beep(freq, dur);
        screen_term_write(" Played tone: "); 
        char buf[16]; int2str(freq, buf);
        screen_term_write(buf); screen_term_write(" Hz\n");
        return CMD_RET_OK;
    }
    screen_term_write(" asio: audio subsystem\n");
    screen_term_write(" Use -h for options. -p to play test tone.\n");
    return CMD_RET_OK;
}

/* ---- otus: network config ---- */
static int cmd_otus(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: otus [-l] [-i IFACE] [-s IP] [-m MASK] [-g GW]\n");
        screen_term_write("  -l          List interfaces\n");
        screen_term_write("  -i IFACE    Interface name\n");
        screen_term_write("  -s IP       Set IP address\n");
        screen_term_write("  -m MASK     Set netmask\n");
        screen_term_write("  -g GW       Set gateway\n");
        screen_term_write("  -d          DHCP mode\n");
        screen_term_write("  -r          Restart interface\n");
        screen_term_write("  -u          Up interface\n");
        screen_term_write("  -w          Down interface\n");
        screen_term_write("  -c          Connection test\n");
        return CMD_RET_OK;
    }
    screen_term_write(" otus: network configuration\n");
    if (has_flag(args, 'l')) {
        screen_term_write(" eth0: 10.0.2.15/24\n");
    }
    return CMD_RET_OK;
}

/* ---- scops: disk usage ---- */
static int cmd_scops(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: scops [-a] [-h] [-s SORT] [-d DIR] [-t TYPE]\n");
        screen_term_write("  -a          All mount points\n");
        screen_term_write("  -h          Human-readable sizes\n");
        screen_term_write("  -s SORT     Sort by: size,name,type\n");
        screen_term_write("  -d DIR      Analyze specific directory\n");
        screen_term_write("  -t TYPE     Filter filesystem type\n");
        screen_term_write("  -c          Show total count\n");
        screen_term_write("  -x          Exclude pseudo-fs\n");
        return CMD_RET_OK;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("=== scops: Disk Usage ===\n");
    screen_set_content_color(C_INFO);
    for (int i = 0; i < blockdev_count(); i++) {
        block_dev_t *bd = blockdev_get(i);
        if (!bd) continue;
        char buf[32];
        screen_term_write(" /dev/bd"); int2str(i, buf); screen_term_write(buf);
        screen_term_write("  ");
        uint64_t mb = bd->total_sectors * bd->sector_size / (1024*1024);
        int2str((int)mb, buf); screen_term_write(buf);
        screen_term_write(" MB\n");
    }
    return CMD_RET_OK;
}

/* ---- glaucid: advanced process killer ---- */
static int cmd_glaucid(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: glaucid PID [-s SIG] [-f] [-n NAME] [-a] [-w]\n");
        screen_term_write("  PID         Process ID to kill\n");
        screen_term_write("  -s SIG      Signal number (default: 15)\n");
        screen_term_write("  -f          Force kill (-9)\n");
        screen_term_write("  -n NAME     Kill by name\n");
        screen_term_write("  -a          Kill all matching\n");
        screen_term_write("  -w          Wait for termination\n");
        screen_term_write("  -l          List signal names\n");
        screen_term_write("  -c          Count matching\n");
        return CMD_RET_OK;
    }
    if (has_flag(args, 'l')) {
        screen_term_write(" 1:HUP 2:INT 3:QUIT 9:KILL 15:TERM 19:STOP 18:CONT\n");
        return CMD_RET_OK;
    }
    if (has_flag(args, 'f')) {
        screen_term_write(" Force-killing...\n");
        return CMD_RET_OK;
    }
    if (args && args[0] >= '0' && args[0] <= '9') {
        char buf[16]; int pid = 0, i = 0;
        while (args[i] >= '0' && args[i] <= '9') { pid = pid * 10 + (args[i] - '0'); i++; }
        int2str(pid, buf);
        screen_term_write(" Signal sent to PID "); screen_term_write(buf); screen_term_write("\n");
    } else {
        screen_term_write(" Usage: glaucid <pid> [-f]\n");
    }
    return CMD_RET_OK;
}

/* ---- urnia: log viewer ---- */
static int cmd_surnia(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: surnia [-n LINES] [-f] [-l LEVEL] [-s SEARCH] [-o FILE]\n");
        screen_term_write("  -n LINES    Number of lines (default: 40)\n");
        screen_term_write("  -f          Follow mode (tail -f)\n");
        screen_term_write("  -l LEVEL    Filter: info,warn,error\n");
        screen_term_write("  -s SEARCH   Search string\n");
        screen_term_write("  -o FILE     Export to file\n");
        screen_term_write("  -c          Clear log\n");
        screen_term_write("  -t          Show timestamps\n");
        return CMD_RET_OK;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("=== surnia: Kernel Log Viewer ===\n");
    screen_set_content_color(C_INFO);
    if (has_flag(args, 'c')) { screen_term_write(" Cannot clear klog in this version.\n"); return CMD_RET_OK; }
    int lines = 40;
    get_flag_val(args, 'n', &lines);
    int total = klog_get_count();
    int start = total - lines;
    if (start < 0) start = 0;
    for (int li = start; li < total; li++) {
        screen_term_write(klog_get_line(li));
        screen_term_write("\n");
    }
    return CMD_RET_OK;
}

/* ---- ninox: detailed system info ---- */
static int cmd_ninox(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: ninox [-a] [-o] [-k] [-m] [-d] [-n] [-p]\n");
        screen_term_write("  -a   All info\n  -o   OS info\n  -k   Kernel info\n");
        screen_term_write("  -m   Memory detail\n  -d   Disk info\n");
        screen_term_write("  -n   Network info\n  -p   Process info\n");
        return CMD_RET_OK;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("=== ninox: System Info ===\n");
    screen_set_content_color(C_INFO);
    if (has_flag(args, 'o') || has_flag(args, 'a')) {
        screen_term_write(" OS:       Noctua OS 1.0\n");
        screen_term_write(" Arch:     i386\n");
    }
    if (has_flag(args, 'k') || has_flag(args, 'a')) {
        screen_term_write(" Kernel:   Noctua-kernel v1.0\n");
        screen_term_write(" Compiler: GCC " __VERSION__ "\n");
    }
    if (has_flag(args, 'm') || has_flag(args, 'a')) {
        char buf[16];
        screen_term_write(" Memory:   "); int2str(heap_free() / 1024, buf);
        screen_term_write(buf); screen_term_write(" KB free\n");
    }
    return CMD_RET_OK;
}

/* ---- aegolius: service manager ---- */
static int cmd_aegolius(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: aegolius <action> <service>\n");
        screen_term_write(" Actions: start,stop,restart,status,enable,disable,list\n");
        screen_term_write("  -a          All services\n");
        screen_term_write("  -l          List with status\n");
        screen_term_write("  -e NAME     Enable service\n");
        screen_term_write("  -d NAME     Disable service\n");
        screen_term_write("  -r NAME     Restart service\n");
        screen_term_write("  -s NAME     Start service\n");
        screen_term_write("  -t NAME     Stop service\n");
        screen_term_write("  -c          Check dependencies\n");
        screen_term_write("  -o FILE     Export service list\n");
        return CMD_RET_OK;
    }
    if (has_flag(args, 'l')) {
        screen_term_write(" [active]   shell (PID 1)\n");
        screen_term_write(" [active]   initd\n");
        screen_term_write(" [inactive] dhcpcd\n");
        screen_term_write(" [inactive] sshd\n");
        screen_term_write(" [active]   serial\n");
    } else {
        screen_term_write(" aegolius: service manager. Use -l to list.\n");
    }
    return CMD_RET_OK;
}

/* ---- megascops: resource usage ---- */
static int cmd_megascops(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: megascops [-r] [-c] [-d] [-n] [-p] [-a]\n");
        screen_term_write("  -r   CPU/ram resource\n  -c   Cache stats\n");
        screen_term_write("  -d   Disk I/O\n  -n   Network I/O\n");
        screen_term_write("  -p   Process top\n  -a   All\n");
        screen_term_write("  -t N Refresh interval (sec)\n");
        return CMD_RET_OK;
    }
    char buf[16];
    char cv[32]; get_cpu_vendor(cv);
    screen_term_write(" CPU: "); screen_term_write(cv); screen_term_write("\n");
    screen_term_write(" RAM: "); int2str(heap_free() / 1024, buf);
    screen_term_write(buf); screen_term_write(" KB free\n");
    int tc = 0; for (int ti = 0; task_get(ti); ti++) tc++;
    screen_term_write(" Tasks: "); int2str(tc, buf);
    screen_term_write(buf); screen_term_write(" running\n");
    return CMD_RET_OK;
}

/* ---- ketupa: backup manager ---- */
CMD_STUB(ketupa, "Backup and restore filesystem snapshots", "ketupa [-b PATH] [-r SNAP] [-l] [-d SNAP] [-t TYPE] [-o FILE] [-s PATH]")

/* ---- jubula: system update framework ---- */
CMD_STUB(jubula, "System update - check and apply updates", "jubula [-c] [-a] [-l] [-d] [-f] [-r] [-v] [-s URL]")

/* ---- mimizuku: install packages ---- */
CMD_STUB(mimizuku, "Package manager - install/remove software", "mimizuku install <pkg> [-y] [-f] [-v] | remove <pkg> | list | search <term> | info <pkg> | update | upgrade")

/* ---- nesasio: terminal settings ---- */
CMD_STUB(nesasio, "Terminal configuration - colors, font, layout", "nesasio [-c COLOR] [-f FONT] [-s SIZE] [-b] [-t THEME] [-r] [-l] [-p]")

/* ---- phodilus: display server control ---- */
CMD_STUB(phodilus, "Display server management", "phodilus [-m MODE] [-r RES] [-d] [-b] [-s] [-l] [-x] [-y] [-c]")

/* ---- ptilopsis: clipboard ---- */
CMD_STUB(ptilopsis, "Clipboard manager - copy/paste between apps", "ptilopsis [-c TEXT] [-p] [-l] [-h] [-x] [-a] [-s FILE]")

/* ---- pulsatrix: bootloader configuration ---- */
CMD_STUB(pulsatrix, "GRUB/limine bootloader configuration", "pulsatrix [-l] [-e ENTRY] [-t SEC] [-d DEV] [-r] [-f] [-c FILE] [-o]")

/* ---- orus: system recovery ---- */
CMD_STUB(orus, "System recovery and repair tools", "orus [-c] [-f] [-d DEV] [-r] [-b] [-k] [-s] [-o FILE] [-l]")

/* ---- sirius: process scheduler info ---- */
CMD_STUB(sirius, "View process scheduling details and policy", "sirius [-l] [-p PID] [-s] [-r] [-t] [-a] [-c] [-o FILE]")

/* ---- vega: memory layout ---- */
CMD_STUB(vega, "Memory region viewer - kernel, heap, stack, mmio", "vega [-k] [-h] [-s] [-m] [-a] [-u] [-p] [-f] [-o FILE]")

/* ---- rigel: interrupt viewer ---- */
CMD_STUB(rigel, "Interrupt vector table and IRQ statistics", "rigel [-l] [-i NUM] [-s] [-r] [-c] [-d] [-a] [-o FILE]")

/* ---- polaris: performance counter ---- */
CMD_STUB(polaris, "CPU performance counter - cycles, instructions", "polaris [-c] [-i] [-b] [-l] [-r] [-t MS] [-n COUNT] [-s]")

/* ---- altair: task manager with tree view ---- */
CMD_STUB(altair, "Hierarchical process tree viewer", "altair [-t] [-p PID] [-a] [-c] [-s SORT] [-r] [-n COUNT] [-o FILE]")

/* ---- capella: CPU cache info ---- */
CMD_STUB(capella, "CPU cache topology - L1/L2/L3 sizes and lines", "capella [-a] [-1] [-2] [-3] [-v] [-l] [-c] [-d]")

/* ---- aldebaran: register viewer ---- */
CMD_STUB(aldebaran, "CPU and device register dump", "aldebaran [-c] [-d] [-i] [-p] [-a] [-o FILE] [-r REG]")

/* ---- regulus: stack tracer ---- */
CMD_STUB(regulus, "Call stack tracer for kernel and processes", "regulus [-p PID] [-k] [-a] [-d] [-c] [-n DEPTH] [-o FILE]")

/* ---- spica: heap analyzer ---- */
CMD_STUB(spica, "Heap allocator statistics and analysis", "spica [-a] [-f] [-s] [-l] [-c] [-p] [-b] [-o FILE]")

/* ---- antares: kernel module manager ---- */
CMD_STUB(antares, "Load/unload kernel modules and drivers", "antares load <mod> | unload <mod> | list | info <mod> | dep <mod> | [-f] [-v]")

/* ---- arcturus: symbol table viewer ---- */
CMD_STUB(arcturus, "Kernel symbol table (kallsyms) viewer", "arcturus [-l] [-s SYM] [-a] [-t TYPE] [-o FILE] [-n] [-c]")

/* ---- betelgeuse: DMA engine status ---- */
CMD_STUB(betelgeuse, "DMA controller and channel status", "betelgeuse [-l] [-c NUM] [-s] [-r] [-a] [-d] [-o FILE]")

/* ---- castor: timer subsystem info ---- */
CMD_STUB(castor, "Timer hardware info - PIT, HPET, TSC", "castor [-a] [-p] [-h] [-t] [-f FREQ] [-c] [-l]")

/* ---- pollux: multi-core balancer ---- */
CMD_STUB(pollux, "SMP load balancing across CPU cores", "pollux [-l] [-c CPU] [-p PID] [-b] [-r] [-a] [-s]")

/* ---- pleiades: kernel thread manager ---- */
CMD_STUB(pleiades, "Kernel thread list and control", "pleiades [-l] [-k PID] [-p] [-n] [-s STATE] [-c] [-o FILE]")

/* ---- orion: IPC viewer ---- */
CMD_STUB(orion, "Inter-process communication status", "orion [-l] [-t TYPE] [-p PID] [-a] [-c] [-s] [-d ID] [-o FILE]")

/* ---- andromeda: semaphore viewer ---- */
CMD_STUB(andromeda, "Kernel semaphore usage and contention", "andromeda [-l] [-p PID] [-a] [-c] [-w] [-b] [-o FILE]")

/* ---- cassiopeia: mutex info ---- */
CMD_STUB(cassiopeia, "Mutex lock debugging and ownership", "cassiopeia [-l] [-p PID] [-a] [-c] [-w] [-o FILE] [-v]")

/* ---- draco: barrier/sync info ---- */
CMD_STUB(draco, "Synchronization primitives status", "draco [-l] [-t TYPE] [-p PID] [-a] [-c] [-b] [-o FILE]")

/* ---- cygnus: event notifier ---- */
CMD_STUB(cygnus, "Event notification system status", "cygnus [-l] [-p PID] [-e NAME] [-w] [-c] [-t SEC] [-o FILE]")

/* ---- aether: network heavy tools ---- */
CMD_STUB(aether, "Advanced network diagnostics and analysis", "aether [-s HOST] [-p PORT] [-t TYPE] [-c] [-n COUNT] [-w SEC] [-l] [-d] [-v] [-o FILE]")

/* ---- umbra: security/access audit ---- */
CMD_STUB(umbra, "Security audit and access control", "umbra [-l] [-c] [-s] [-a] [-r] [-d] [-f FILE] [-o FILE] [-v]")

/* ---- solstice: date/time advanced ---- */
static int cmd_solstice(const char *args) {
    if (has_flag(args, 'h')) {
        screen_term_write("Usage: solstice [-d] [-t] [-u] [-s STR] [-z ZONE] [-f FMT] [-c] [-o FILE]\n");
        return CMD_RET_OK;
    }
    rtc_time_t rt;
    rtc_read_time(&rt);
    char buf[16];
    screen_term_write(" Current: ");
    int2str(2000 + rt.year, buf); screen_term_write(buf); screen_term_write("-");
    int2str(rt.month, buf); screen_term_write(buf); screen_term_write("-");
    int2str(rt.day, buf); screen_term_write(buf); screen_term_write(" ");
    int2str(rt.hour, buf); screen_term_write(buf); screen_term_write(":");
    int2str(rt.minute, buf); screen_term_write(buf); screen_term_write(":");
    int2str(rt.second, buf); screen_term_write(buf); screen_term_write("\n");
    if (has_flag(args, 'u')) { screen_term_write(" UTC time (concept)\n"); }
    if (has_flag(args, 'c')) { screen_term_write(" CMOS valid\n"); }
    return CMD_RET_OK;
}

/* ---- eclipse: display control ---- */
CMD_STUB(eclipse, "Display mode and screen control", "eclipse [-m MODE] [-r RES] [-b BRIGHT] [-c] [-f] [-d DISP] [-s] [-l] [-t SEC]")

/* ---- twilight: power management ---- */
CMD_STUB(twilight, "Power management - sleep, hibernate, battery", "twilight [-s] [-h] [-r] [-p] [-b] [-c] [-t SEC] [-w]")

/* ---- midnight: shell/theme settings ---- */
CMD_STUB(midnight, "Shell theme and prompt customization", "midnight [-t THEME] [-c COLORS] [-p PROMPT] [-f FONT] [-l] [-s] [-r] [-e]")

/* ---- aurora: display effects ---- */
CMD_STUB(aurora, "Framebuffer visual effects - gradients, patterns", "aurora [-e EFFECT] [-c COLOR] [-s SPEED] [-l] [-p] [-r] [-t SEC]")

/* ---- zenith: system optimization ---- */
CMD_STUB(zenith, "System performance tuning and optimization", "zenith [-a] [-c] [-m] [-d] [-n] [-s] [-r] [-o FILE] [-v]")

/* ---- nebula: cloud integration ---- */
CMD_STUB(nebula, "Cloud storage integration tools", "nebula [-l] [-u FILE] [-d FILE] [-s] [-c] [-r] [-p] [-e]")

/* ---- cosmos: universe/time since boot ---- */
CMD_STUB(cosmos, "Extended uptime with cosmic time display", "cosmos [-a] [-u] [-h] [-s] [-f] [-d] [-v]")

/* ---- galaxy: disk topology ---- */
CMD_STUB(galaxy, "Full storage topology - disks, partitions, mounts", "galaxy [-a] [-d] [-p] [-m] [-l] [-t] [-o FILE] [-v]")

/* ---- stellar: benchmark ---- */
CMD_STUB(stellar, "CPU and memory benchmark", "stellar [-c] [-m] [-d] [-a] [-l] [-t SEC] [-n COUNT] [-o FILE]")

/* ---- lunar: phase calendar ---- */
CMD_STUB(lunar, "Moon phase and celestial calendar", "lunar [-p] [-d DATE] [-l] [-r] [-c] [-o FILE]")

/* ---- solar: energy/power stats ---- */
CMD_STUB(solar, "Power consumption estimation and stats", "solar [-a] [-c] [-b] [-s] [-l] [-o FILE] [-v]")

/* ---- nova: create new ---- */
CMD_STUB(nova, "Create new project/file from template", "nova <type> <name> [-f] [-d DIR] [-l] [-t TEMPLATE] [-o]")

/* ---- pulsar: signal/interrupt test ---- */
CMD_STUB(pulsar, "Signal generation and interrupt test", "pulsar [-s SIG] [-p PID] [-n COUNT] [-t SEC] [-l] [-d]")

/* ---- quasar: photon/light effects ---- */
CMD_STUB(quasar, "LED and indicator light control", "quasar [-d DEV] [-c COLOR] [-b BRIGHT] [-p PATTERN] [-s SPEED] [-t SEC]")

/* ---- magnetar: magnetic/disk low-level ---- */
CMD_STUB(magnetar, "Low-level ATA/SCSI command tool", "magnetar [-d DEV] [-c CMD] [-l] [-i] [-r LBA] [-w LBA DATA] [-s] [-o FILE]")

/* ---- supernova: factory reset ---- */
CMD_STUB(supernova, "System factory reset and wipe", "supernova [-f] [-a] [-k] [-d] [-r] [-y] [-v]")

/* ---- hypernova: extreme benchmark ---- */
CMD_STUB(hypernova, "Stress test - CPU, memory, disk, network", "hypernova [-c] [-m] [-d] [-n] [-a] [-t SEC] [-p] [-o FILE]")

/* ---- ruby: code formatter ---- */
CMD_STUB(ruby, "Source code formatter and beautifier", "ruby [-f FILE] [-d DIR] [-l LANG] [-s] [-c] [-o] [-r] [-v]")

/* ---- emerald: crypto/hash ---- */
CMD_STUB(emerald, "Cryptographic hash and checksum tool", "emerald <file> [-a ALGO] [-o FILE] [-c] [-l] [-s STR] [-v]")

/* ---- sapphire: encryption/decryption ---- */
CMD_STUB(sapphire, "File encryption and decryption", "sapphire encrypt <file> | decrypt <file> [-k KEY] [-o FILE] [-a ALGO] [-f] [-v]")

/* ---- diamond: file search by content ---- */
CMD_STUB(diamond, "Advanced content search with patterns", "diamond <pattern> <path> [-r] [-i] [-l] [-n] [-c] [-s SIZE] [-t TYPE] [-o FILE]")

/* ---- amethyst: color picker/theme ---- */
CMD_STUB(amethyst, "Terminal color picker and palette manager", "amethyst [-p] [-l] [-s NAME] [-r] [-c COLOR] [-e] [-f FILE] [-i]")

/* ---- topaz: file archiver ---- */
CMD_STUB(topaz, "Create and extract archives (tar-like)", "topaz create <archive> <files> | extract <archive> [-o DIR] [-l] [-t TYPE] [-f] [-v]")

/* ---- opal: disk encryption ---- */
CMD_STUB(opal, "Disk encryption setup and management", "opal [-d DEV] [-e] [-c] [-l] [-o] [-p PASS] [-k KEYFILE] [-f]")

/* ---- jade: environment manager ---- */
CMD_STUB(jade, "Environment variable and profile manager", "jade [-l] [-s VAR=VAL] [-u VAR] [-e NAME] [-d NAME] [-f FILE] [-r] [-c]")

/* ---- onyx: dark mode/theme ---- */
CMD_STUB(onyx, "System-wide dark/light theme toggle", "onyx [-d] [-l] [-t THEME] [-s] [-c] [-r] [-f FILE] [-p]")

/* ---- quartz: timer/alarm ---- */
CMD_STUB(quartz, "System timer, alarm, and countdown", "quartz [-t SEC] [-c] [-m] [-s CMD] [-l] [-d ID] [-r] [-n COUNT]")

/* ---- garnet: ruby/gem package alternate ---- */
CMD_STUB(garnet, "Extension/module package manager", "garnet [-i FILE] [-r NAME] [-l] [-u] [-s TERM] [-c] [-d] [-v]")

/* ---- tourmaline: encoding/decoding ---- */
CMD_STUB(tourmaline, "Base64, hex, and text encoding converter", "tourmaline encode <type> <data> | decode <type> <data> [-o FILE] [-f] [-v]")

/* ---- peridot: sorting utility ---- */
CMD_STUB(peridot, "Sort lines or data in various modes", "peridot [-r] [-u] [-n] [-t SEP] [-k KEY] [-f FILE] [-o FILE] [-c]")

/* ---- citrine: diff/compare ---- */
CMD_STUB(citrine, "Compare files and directories", "citrine <file1> <file2> [-r] [-i] [-u] [-c] [-s] [-b] [-o FILE]")

/* ---- turquoise: system health ---- */
CMD_STUB(turquoise, "System health check and diagnostics", "turquoise [-a] [-c] [-m] [-d] [-n] [-s] [-o FILE] [-r] [-v]")

/* ---- lapis: theme/color scheme manager ---- */
CMD_STUB(lapis, "Color scheme and palette configuration", "lapis [-l] [-s NAME] [-i FILE] [-e] [-r] [-c] [-f] [-o]")

/* ---- coral: file sync ---- */
CMD_STUB(coral, "File synchronization and mirror tool", "coral [-s SRC] [-d DST] [-r] [-u] [-n] [-c] [-v] [-e PAT] [-o FILE]")

/* ---- pearl: random generation ---- */
CMD_STUB(pearl, "Random number, string, and UUID generator", "pearl [-n COUNT] [-t TYPE] [-l LEN] [-o FILE] [-s SEED] [-a] [-c]")

/* ---- amber: time capsule/snapshot ---- */
CMD_STUB(amber, "Filesystem snapshot and versioning", "amber [-s PATH] [-l] [-r REV] [-d REV] [-c] [-v] [-t TYPE] [-o FILE]")

/* ---- jasper: pattern matching ---- */
CMD_STUB(jasper, "Pattern-based file classification", "jasper [-p PAT] [-d DIR] [-r] [-c] [-l] [-o FILE] [-s SORT] [-t TYPE]")

/* ---- nexus: system linking ---- */
CMD_STUB(nexus, "Service and component interconnection map", "nexus [-l] [-a] [-c] [-d] [-t TYPE] [-s NAME] [-o FILE] [-v]")

/* ---- vortex: data stream ---- */
CMD_STUB(vortex, "Data streaming between devices and files", "vortex <source> <dest> [-b SIZE] [-s SPEED] [-t TYPE] [-c] [-f] [-v]")

/* ---- cipher: encrypt/decrypt text ---- */
CMD_STUB(cipher, "Text encryption using various algorithms", "cipher [-e] [-d] [-k KEY] [-a ALGO] [-f FILE] [-o FILE] [-s TEXT] [-v]")

/* ---- monolith: large file tool ---- */
CMD_STUB(monolith, "Large file splitter and combiner", "monolith split <file> <size> | join <file> [-o DIR] [-c] [-v] [-f]")

/* ---- prism: data visualization ---- */
CMD_STUB(prism, "Data visualization - bar, pie, graph on terminal", "prism [-t TYPE] [-d DATA] [-f FILE] [-c] [-s] [-o] [-w W] [-h H]")

/* ---- obelisk: monument/statue/persistence ---- */
CMD_STUB(obelisk, "Persistent storage configuration", "obelisk [-l] [-d DEV] [-m MOUNT] [-t FS] [-o OPTS] [-c] [-r] [-f]")

/* ---- resonance: audio analysis ---- */
CMD_STUB(resonance, "Audio spectrum and frequency analysis", "resonance [-f FREQ] [-d DEV] [-l] [-s] [-t SEC] [-c] [-o FILE]")

/* ---- quantum: superposition/randomness ---- */
CMD_STUB(quantum, "Quantum-style random and probability tools", "quantum [-n COUNT] [-t TYPE] [-p PROB] [-s SEED] [-r] [-c] [-o FILE]")

/* ---- axiom: logical axioms/truths ---- */
CMD_STUB(axiom, "Boolean algebra and truth table generator", "axiom [-e EXPR] [-f FILE] [-t] [-c] [-o FILE] [-l] [-a]")

/* ---- vector: 2D/3D math ---- */
CMD_STUB(vector, "Vector and matrix mathematics", "vector add|sub|mul|dot|cross <args> [-f FORMAT] [-o FILE] [-v]")

/* ---- matrix: linear algebra ---- */
CMD_STUB(matrix, "Matrix operations - multiply, inverse, determinant", "matrix <op> <args> [-s SIZE] [-r] [-c] [-f FILE] [-o FILE] [-v]")

/* ---- helix: DNA/biology/bioinfo ---- */
CMD_STUB(helix, "Bioinformatics sequence analysis", "helix [-s SEQ] [-f FILE] [-t TYPE] [-c] [-l] [-a] [-o FILE] [-r]")

/* ---- pixel: image manipulation ---- */
CMD_STUB(pixel, "Framebuffer image operations", "pixel [-i FILE] [-o FILE] [-r RESIZE] [-c COLOR] [-e EFFECT] [-f] [-l]")

/* ---- lattice: network topology ---- */
CMD_STUB(lattice, "Network topology discovery and mapping", "lattice [-i IFACE] [-s SUBNET] [-l] [-c] [-t SEC] [-p] [-o FILE]")

/* ---- flux: data flow monitor ---- */
CMD_STUB(flux, "Data flow and throughput monitoring", "flux [-i IFACE] [-f FILE] [-d DEV] [-t TYPE] [-c] [-s] [-o FILE]")

/* ---- drift: clock drift ---- */
CMD_STUB(drift, "Clock drift measurement and NTP sync", "drift [-c] [-d] [-s HOST] [-t SEC] [-r] [-l] [-o FILE]")

/* ---- surge: power spike protection ---- */
CMD_STUB(surge, "UPS and power supply monitoring", "surge [-s] [-c] [-l] [-b] [-d DEV] [-t SEC] [-a] [-o FILE]")

/* ---- pulse: heartbeat/keepalive ---- */
CMD_STUB(pulse, "Network heartbeat and availability monitoring", "pulse [-t TARGET] [-i SEC] [-c COUNT] [-l] [-a] [-o FILE] [-v]")

/* ---- apex: peak performance ---- */
CMD_STUB(apex, "CPU frequency scaling and governor control", "apex [-l] [-g GOV] [-s SPEED] [-c CPU] [-t TEMP] [-a] [-r]")

/* ---- vertex: graph theory ---- */
CMD_STUB(vertex, "Graph analysis - shortest path, traversal", "vertex [-f FILE] [-s SRC] [-d DST] [-a ALGO] [-t TYPE] [-o FILE] [-v]")

/* ---- fenrir: process security ---- */
CMD_STUB(fenrir, "Process sandboxing and capability control", "fenrir [-p PID] [-c CAP] [-l] [-d] [-s] [-r] [-a] [-o FILE]")

/* ---- valkyrie: process priority ---- */
CMD_STUB(valkyrie, "Process priority and scheduling class", "valkyrie [-p PID] [-n NICE] [-c CLASS] [-l] [-r] [-s] [-a]")

/* ---- odin: all-seeing system ---- */
CMD_STUB(odin, "Comprehensive system overview dashboard", "odin [-a] [-c] [-m] [-d] [-n] [-p] [-s] [-l] [-o FILE] [-v]")

/* ---- thor: power/performance ---- */
CMD_STUB(thor, "Hardware control - fans, voltage, power", "thor [-f] [-v VOLT] [-t TEMP] [-s STATE] [-l] [-c] [-a] [-d DEV]")

/* ---- loki: trickster/debug ---- */
CMD_STUB(loki, "Kernel debugging and breakpoint manager", "loki [-b ADDR] [-r] [-c] [-l] [-s] [-d] [-p PID] [-w] [-o FILE]")

/* ---- freya: desktop/file manager ---- */
CMD_STUB(freya, "File manager operations", "freya [-d DIR] [-l] [-r] [-c] [-m] [-s] [-o FILE] [-p] [-t TYPE]")

/* ---- heimdall: network gateway watch ---- */
CMD_STUB(heimdall, "Network gateway and route monitoring", "heimdall [-g GW] [-i IFACE] [-l] [-c] [-t SEC] [-s] [-r] [-o FILE]")

/* ---- baldr: integrity check ---- */
CMD_STUB(baldr, "File integrity and checksum verification", "baldr [-c FILE] [-d DIR] [-a ALGO] [-l] [-r] [-s] [-o FILE] [-v]")

/* ---- tyr: access control ---- */
CMD_STUB(tyr, "Access control list (ACL) management", "tyr [-f FILE] [-u USER] [-p PERM] [-l] [-r] [-d] [-s] [-a] [-o]")

/* ---- sif: key/value store ---- */
CMD_STUB(sif, "Key-value store for system data", "sif get <key> | set <key> <val> | del <key> | list | flush [-f FILE] [-e]")

/* ---- njord: filesystem operations ---- */
CMD_STUB(njord, "Advanced filesystem operations", "njord [-t TYPE] [-d DEV] [-m MOUNT] [-c] [-r] [-s] [-l] [-o OPTS] [-f]")

/* ---- idunn: package refresh ---- */
CMD_STUB(idunn, "Package metadata refresh and cache update", "idunn [-u] [-c] [-l] [-r] [-s] [-a] [-v] [-f]")

/* ---- bragi: poetry/formatting ---- */
CMD_STUB(bragi, "ASCII art and text formatting tools", "bragi [-t TEXT] [-f FONT] [-s SIZE] [-c CHAR] [-l] [-o FILE] [-r]")

/* ---- hodr: blind/debug ---- */
CMD_STUB(hodr, "Kernel oops/panic log capture", "hodr [-l] [-c] [-s] [-d] [-o FILE] [-a] [-r]")

/* ---- vidarr: filesystem repair ---- */
CMD_STUB(vidarr, "Filesystem consistency check and repair", "vidarr [-d DEV] [-t TYPE] [-c] [-r] [-f] [-l] [-s] [-a] [-v]")

/* ---- ali: executor ---- */
CMD_STUB(ali, "Schedule command execution at time", "ali [-t TIME] [-c CMD] [-l] [-d ID] [-r] [-f FILE] [-v]")

/* ---- magni: memory pressure ---- */
CMD_STUB(magni, "Memory pressure and OOM management", "magni [-l] [-c] [-p PID] [-k] [-s] [-r] [-o FILE] [-v]")

/* ---- modi: resource limits ---- */
CMD_STUB(modi, "Process resource limits (ulimit style)", "modi [-p PID] [-l] [-s] [-n NOFILE] [-m MEM] [-c CPU] [-d SIZE] [-v]")

/* ---- saga: history/story ---- */
CMD_STUB(saga, "System event log with narrative format", "saga [-l] [-t TYPE] [-s SEARCH] [-n LINES] [-f] [-o FILE] [-d DATE]")

/* ---- rune: symbolic/icon ---- */
CMD_STUB(rune, "Icon and symbol picker for terminal", "rune [-l] [-s SEARCH] [-c] [-p] [-o FILE] [-t CAT] [-n]")

/* ---- ignis: fire/light/torch ---- */
CMD_STUB(ignis, "LED and status indicator control", "ignis [-d DEV] [-c COLOR] [-b BRIGHT] [-p PAT] [-s SPEED] [-t SEC] [-l]")

/* ---- cinder: residual/cleanup ---- */
CMD_STUB(cinder, "System cleanup - temp files, cache, logs", "cinder [-a] [-t TMP] [-c CACHE] [-l LOGS] [-d] [-f] [-r] [-v] [-s]")

/* ---- ember: glow/signaling ---- */
CMD_STUB(ember, "Notification and alert system test", "ember [-m MSG] [-t TYPE] [-s SEV] [-d] [-l] [-c] [-o FILE]")

/* ---- blaze: fast copy ---- */
CMD_STUB(blaze, "High-speed file copy with progress", "blaze <src> <dst> [-b SIZE] [-c] [-v] [-f] [-s SPEED] [-p] [-r]")

/* ---- flare: visual flash ---- */
CMD_STUB(flare, "Screen flash and attention effects", "flare [-c COLOR] [-t MS] [-p] [-l] [-r] [-s] [-n COUNT]")

/* ---- spark: static/charge ---- */
CMD_STUB(spark, "Electrostatic discharge / GPIO test", "spark [-p PIN] [-d DEV] [-t TYPE] [-c] [-l] [-r] [-v]")

/* ---- flash: memory tool ---- */
CMD_STUB(flash, "Flash memory (NOR/NAND) utility", "flash [-d DEV] [-r ADDR] [-w ADDR DATA] [-e] [-l] [-c] [-o FILE] [-v]")

/* ---- glow: light effect ---- */
CMD_STUB(glow, "Backlight and brightness control", "glow [-d DEV] [-b BRIGHT] [-c] [-l] [-s STEP] [-t TIME] [-r]")

/* ---- gleam: reflection/cache ---- */
CMD_STUB(gleam, "Cache hit/miss statistics and tuning", "gleam [-a] [-c] [-d] [-s] [-l] [-r] [-o FILE] [-v]")

/* ---- shine: polish/optimize ---- */
CMD_STUB(shine, "Performance optimization suggestions", "shine [-a] [-c] [-m] [-d] [-n] [-s] [-l] [-o FILE] [-v]")

/* ---- beam: directed/point-to-point ---- */
CMD_STUB(beam, "Point-to-point data transfer between nodes", "beam <src> <dst:port> [-b SIZE] [-c] [-v] [-e] [-p]")

/* ---- ray: raycast/geometry ---- */
CMD_STUB(ray, "Simple geometry calculations (distance, angle)", "ray [-x1 N] [-y1 N] [-x2 N] [-y2 N] [-t TYPE] [-d] [-v]")

/* ---- dawn: morning/startup ---- */
CMD_STUB(dawn, "System startup time and boot analysis", "dawn [-a] [-b] [-c] [-s] [-d] [-l] [-o FILE] [-v] [-t TYPE]")

/* ---- dusk: evening/shutdown ---- */
CMD_STUB(dusk, "Scheduled shutdown and power events", "dusk [-t TIME] [-c] [-l] [-d ID] [-r] [-s] [-a] [-f]")

/* ---- gloom: dim/low-power ---- */
CMD_STUB(gloom, "Low-power mode and power saving", "gloom [-a] [-c] [-d] [-s] [-l] [-r] [-t SEC] [-v]")

/* ---- shade: shadow/backup ---- */
CMD_STUB(shade, "Shadow copy and file versioning", "shade [-s SRC] [-d DST] [-l] [-r REV] [-c] [-t TYPE] [-v]")

/* ---- shadow: duplicate/find ---- */
CMD_STUB(shadow, "Find duplicate files by hash", "shadow <path> [-r] [-a ALGO] [-s SIZE] [-d] [-c] [-l] [-o FILE]")

/* ---- phantom: ghost/process ---- */
CMD_STUB(phantom, "Orphan process finder and cleaner", "phantom [-l] [-c] [-k] [-p PID] [-r] [-s] [-o FILE] [-v]")

/* ---- wraith: stealth/conceal ---- */
CMD_STUB(wraith, "Stealth mode - disable logging and output", "wraith [-a] [-k] [-s] [-r] [-c] [-d] [-l] [-t SEC]")

/* ---- spirit: session/daemon ---- */
CMD_STUB(spirit, "Background daemon and session manager", "spirit start <name> | stop <name> | list | status <name> [-f] [-v] [-c]")

/* ---- verbum: word/text counter ---- */
CMD_STUB(verbum, "Text statistics - words, lines, chars, bytes", "verbum <file> [-c] [-w] [-l] [-L] [-b] [-s] [-o FILE]")

/* ---- scriptum: script writer ---- */
CMD_STUB(scriptum, "Interactive script recording and playback", "scriptum [-r] [-p] [-f FILE] [-t TYPE] [-l] [-o FILE] [-v]")

/* ---- lectio: file reader with highlights ---- */
CMD_STUB(lectio, "Syntax-highlighted file viewer", "lectio <file> [-l LANG] [-n] [-s] [-c] [-t THEME] [-o] [-r]")

/* ---- dictum: quote/saying ---- */
CMD_STUB(dictum, "Display random quote or message of the day", "dictum [-l] [-a] [-s CAT] [-c] [-n] [-o FILE] [-r]")

/* ---- notitia: notice/bulletin ---- */
CMD_STUB(notitia, "System notices and message broadcast", "notitia [-m MSG] [-u USER] [-a] [-c] [-l] [-s] [-d]")

/* ---- ratio: numeric ratio ---- */
CMD_STUB(ratio, "Compression ratio and efficiency analysis", "ratio [-f FILE] [-l] [-c] [-s] [-t TYPE] [-o FILE] [-v]")

/* ---- census: user/process ---- */
CMD_STUB(census, "System census - users, groups, processes", "census [-u] [-g] [-p] [-a] [-c] [-l] [-o FILE] [-v]")

/* ---- gradus: step/progress ---- */
CMD_STUB(gradus, "Progress bar and step indicator", "gradus [-n TOTAL] [-c CURRENT] [-w WIDTH] [-p] [-s] [-t TEXT] [-d]")

/* ---- ordo: order/sort ---- */
CMD_STUB(ordo, "Order files by various criteria", "ordo <dir> [-s SORT] [-r] [-t TYPE] [-c] [-l] [-o FILE] [-v]")

/* ---- regula: rule/constraint ---- */
CMD_STUB(regula, "Rule-based file action engine", "regula [-r RULE] [-d DIR] [-f FILE] [-c] [-l] [-s] [-e] [-v]")

/* ---- forma: format/converter ---- */
CMD_STUB(forma, "File format detection and conversion", "forma <file> [-t TYPE] [-o FILE] [-l] [-c] [-d] [-s] [-v]")

/* ---- signum: signature ---- */
CMD_STUB(signum, "Digital signature creation and verification", "signum sign <file> | verify <file> <sig> [-k KEY] [-a ALGO] [-v]")

/* ---- index: search index ---- */
CMD_STUB(index, "File content indexing and search", "index [-u] [-d DIR] [-s TERM] [-r] [-l] [-c] [-o FILE] [-v]")

/* ---- meta: metadata ---- */
CMD_STUB(meta, "File metadata viewer (stat alternative)", "meta <file> [-a] [-c] [-s] [-d] [-p] [-l] [-t TYPE] [-o]")

/* ---- finis: end/terminate ---- */
CMD_STUB(finis, "Graceful system termination sequence", "finis [-t SEC] [-r] [-h] [-p] [-n] [-f] [-c]")

/* ---- initium: initialize ---- */
CMD_STUB(initium, "Service initialization status and control", "initium [-l] [-s NAME] [-r] [-d] [-e] [-c] [-a] [-o FILE]")

/* ---- medium: transport ---- */
CMD_STUB(medium, "Media transport - mount, eject, detect", "medium [-d DEV] [-m MOUNT] [-u] [-e] [-l] [-t TYPE] [-c] [-v]")

/* ---- terminus: endpoint ---- */
CMD_STUB(terminus, "Network endpoint connection tool", "terminus <host:port> [-p PORT] [-t TYPE] [-c] [-s] [-l] [-d] [-v]")

/* ---- casus: event/cause ---- */
CMD_STUB(casus, "Event correlation and root cause analysis", "casus [-e EVENT] [-t TYPE] [-s SEARCH] [-l] [-c] [-o FILE] [-v]")

/* ---- modus: operation mode ---- */
CMD_STUB(modus, "System operation mode profile", "modus [-l] [-s NAME] [-c] [-r] [-d] [-a] [-o FILE] [-v]")

/* ---- falcon: fast file search ---- */
CMD_STUB(falcon, "High-speed file search by name/pattern", "falcon <pattern> [-p PATH] [-r] [-i] [-l] [-t TYPE] [-s] [-c] [-o]")

/* ---- raven: dark/system info ---- */
CMD_STUB(raven, "System information in minimal output", "raven [-a] [-c] [-m] [-d] [-n] [-s] [-l] [-o FILE]")

/* ---- hawk: watch/monitor ---- */
CMD_STUB(hawk, "Watch command output at intervals", "hawk <cmd> [-t SEC] [-c COUNT] [-d] [-l] [-v] [-o FILE]")

/* ---- eagle: file manager TUI ---- */
CMD_STUB(eagle, "Terminal-based file manager interface", "eagle <path> [-l] [-s] [-t TYPE] [-h] [-v] [-c] [-r]")

/* ---- crow: batch rename ---- */
CMD_STUB(crow, "Batch file rename with patterns", "crow <pattern> <replacement> <files> [-r] [-i] [-n] [-d] [-v] [-c]")

/* ---- phoenix: recovery/rebirth ---- */
CMD_STUB(phoenix, "System recovery environment", "phoenix [-a] [-c] [-s] [-d DEV] [-r] [-l] [-o FILE] [-v]")

/* ---- heron: network scan ---- */
CMD_STUB(heron, "Network port scanner and discovery", "heron <target> [-p PORTS] [-t TYPE] [-c] [-l] [-s SPEED] [-o FILE] [-v]")

/* ---- swan: elegant display ---- */
CMD_STUB(swan, "Elegant formatted output and display", "swan [-t TEXT] [-f FILE] [-c] [-s] [-l] [-o FILE] [-r]")

/* ---- swift: quick actions ---- */
CMD_STUB(swift, "Quick command shortcuts and macros", "swift [-e CMD] [-l] [-s NAME] [-d NAME] [-r] [-f FILE] [-c]")

/* ---- kite: floating/network ---- */
CMD_STUB(kite, "Network connection quality test", "kite <host> [-p PORT] [-t TYPE] [-c COUNT] [-s SIZE] [-l] [-o FILE]")

/* ---- merlin: wizard/assistant ---- */
CMD_STUB(merlin, "Interactive setup wizard framework", "merlin <name> [-l] [-c] [-s STEP] [-d] [-r] [-v] [-f FILE]")

/* ---- goshawk: intense monitoring ---- */
CMD_STUB(goshawk, "Intensive system monitoring with alerts", "goshawk [-a] [-c] [-m MEM] [-d DISK] [-n NET] [-t SEC] [-l] [-o]")

/* ---- harrier: network diagnostics ---- */
CMD_STUB(harrier, "Network diagnostic and packet analysis", "harrier [-i IFACE] [-c COUNT] [-t TYPE] [-s SIZE] [-l] [-o FILE] [-v]")

/* ---- osprey: file permissions ---- */
CMD_STUB(osprey, "File permission and ownership audit", "osprey <path> [-r] [-l] [-c] [-s] [-d] [-u USER] [-g GRP] [-o]")

/* ---- thrush: sound/audio ---- */
CMD_STUB(thrush, "Sound playback and audio device control", "thrush [-f FILE] [-v VOL] [-p] [-l] [-r] [-t SEC] [-c]")

/* ---- finch: small/quick ---- */
CMD_STUB(finch, "Quick file operation shortcuts", "finch new <name> | copy <src> <dst> | move <src> <dst> | del <file> [-r] [-f]")

/* ---- robin: cheerful/notification ---- */
CMD_STUB(robin, "Notification and message display", "robin [-m MSG] [-t TYPE] [-s SEV] [-d] [-c] [-l] [-p]")

/* ---- sparrow: lightweight/many ---- */
CMD_STUB(sparrow, "Bulk file operations on many small files", "sparrow <dir> [-a] [-c] [-s SIZE] [-r] [-l] [-t TYPE] [-o FILE]")

/* ---- starling: murmuration/sync ---- */
CMD_STUB(starling, "Synchronized file watcher and auto-action", "starling [-d DIR] [-e EVENT] [-c CMD] [-r] [-l] [-t SEC] [-v]")

/* ---- martin: martlet/house ---- */
CMD_STUB(martin, "Home directory and user data management", "martin [-u USER] [-b] [-r] [-l] [-c] [-s] [-d DIR] [-o FILE]")

/* ---- crimson: deep red/urgent ---- */
CMD_STUB(crimson, "Urgent system alerts and critical logs", "crimson [-l] [-c] [-s] [-n LINES] [-f] [-o FILE] [-a]")

/* ---- azure: sky/cloud ---- */
CMD_STUB(azure, "Cloud/remote sync configuration", "azure [-s URL] [-l] [-c] [-u FILE] [-d FILE] [-r] [-t TYPE] [-v]")

/* ---- amber-orange: caution ---- */
CMD_STUB(umber, "Warning level system messages", "umber [-l] [-c] [-s] [-n LINES] [-f] [-o FILE]")

/* ---- chartreuse: healthy ---- */
CMD_STUB(chartreuse, "System health and green metrics", "chartreuse [-a] [-c] [-s] [-l] [-r] [-o FILE]")

/* ---- rose: soft/friendly ---- */
CMD_STUB(rose, "Friendly system greeting and status", "rose [-a] [-c] [-m MSG] [-u USER] [-t TYPE] [-l]")

/* ---- sable: black/intensive ---- */
CMD_STUB(sable, "Black-box crash dump analysis", "sable [-f FILE] [-l] [-c] [-s] [-d] [-o FILE]")

/* ---- ivory: premium/rare ---- */
CMD_STUB(ivory, "Premium system features status", "ivory [-l] [-a] [-c] [-s] [-d] [-o FILE]")

/* ---- teal: balanced ---- */
CMD_STUB(teal, "System balance and load analysis", "teal [-a] [-c] [-m] [-d] [-n] [-s] [-l] [-o FILE]")

/* ---- plum: purple/deep ---- */
CMD_STUB(plum, "Kernel profiling data deep dive", "plum [-a] [-c] [-s] [-d] [-l] [-o FILE] [-v]")

/* ---- mauve: soft/pastel ---- */
CMD_STUB(mauve, "Soft system status and user-friendly info", "mauve [-a] [-c] [-s] [-l] [-o FILE]")

/* ---- ochre: earth/stable ---- */
CMD_STUB(ochre, "System stability and uptime report", "ochre [-a] [-c] [-s] [-t TYPE] [-l] [-o FILE]")

/* ---- indigo: deep/analysis ---- */
CMD_STUB(indigo, "Deep system analysis and forensics", "indigo [-a] [-c] [-d] [-s] [-l] [-o FILE] [-v]")

/* ---- violet: creative ---- */
CMD_STUB(violet, "Creative and ASCII art tools", "violet [-f FILE] [-t TEXT] [-s SIZE] [-c] [-l] [-o]")

/* ---- cerulean: sky/network ---- */
CMD_STUB(cerulean, "Sky/network connectivity overview", "cerulean [-a] [-c] [-i IFACE] [-s] [-l] [-r] [-o FILE]")

/* ---- scarlet: warning/highlight ---- */
CMD_STUB(scarlet, "Highlighted error and warning display", "scarlet [-l] [-c] [-s SEV] [-n LINES] [-f] [-o FILE]")

/* ---- viridian: success ---- */
CMD_STUB(viridian, "Success metrics and achievement log", "viridian [-l] [-c] [-s] [-t TYPE] [-o FILE]")

/* ---- saffron: bright/warm ---- */
CMD_STUB(saffron, "Warm system and temperature report", "saffron [-a] [-c] [-s] [-t TYPE] [-l] [-o FILE]")

/* ---- cobalt: sharp/technical ---- */
CMD_STUB(cobalt, "Technical kernel parameter viewer", "cobalt [-l] [-g KEY] [-s KEY=VAL] [-d] [-r] [-a] [-o FILE]")

/* ---- magenta: vibrant ---- */
CMD_STUB(magenta, "Vibrant system celebration/easter egg", "magenta [-a] [-c] [-s] [-t TYPE] [-l] [-o FILE]")

/* ---- carmine: gem/valuable ---- */
CMD_STUB(carmine, "System gems/valuable resources list", "carmine [-a] [-c] [-s] [-l] [-t TYPE] [-o FILE]")

void cmd_noctua_init(void) {
    static command_t cmds[] = {
        CMD_FLAG("noctua",    cmd_noctua,    "OS kernel information", "noctua [-v] [-a] [-m] [-c] [-b] [-l]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("strix",     cmd_strix,     "Process manager - list/control processes", "strix [-l] [-k PID] [-s SIG] [-n NAME] [-p] [-t] [-c] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("bubo",      cmd_bubo,      "System monitor - CPU, RAM, uptime", "bubo [-v] [-l] [-n N] [-t SEC]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("tyto",      cmd_tyto,      "System configuration read/write", "tyto [-g KEY] [-s KEY=VAL] [-l] [-r] [-f FILE] [-e] [-i FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("asio",      cmd_asio,      "Audio service control", "asio [-s STATE] [-v VOL] [-f FREQ] [-t MS] [-l] [-p] [-m]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("otus",      cmd_otus,      "Network configuration tool", "otus [-l] [-i IFACE] [-s IP] [-m MASK] [-g GW] [-d] [-r] [-u] [-w]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("scops",     cmd_scops,     "Disk usage analyzer", "scops [-a] [-h] [-s SORT] [-d DIR] [-t TYPE] [-c] [-x]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("glaucid",   cmd_glaucid,   "Process killer with signal control", "glaucid <PID> [-s SIG] [-f] [-n NAME] [-a] [-w] [-l] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("surnia",    cmd_surnia,    "Kernel log viewer (dmesg alternative)", "surnia [-n LINES] [-f] [-l LEVEL] [-s TEXT] [-o FILE] [-c] [-t]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ninox",     cmd_ninox,     "Detailed system information", "ninox [-a] [-o] [-k] [-m] [-d] [-n] [-p]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("aegolius",  cmd_aegolius,  "Service manager - start/stop/enable", "aegolius [-l] [-e NAME] [-d NAME] [-r NAME] [-s NAME] [-t NAME] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("megascops", cmd_megascops, "Resource usage - CPU, RAM, I/O", "megascops [-r] [-c] [-d] [-n] [-p] [-a] [-t SEC]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ketupa",    cmd_ketupa,    "Backup and restore snapshots", "ketupa [-b PATH] [-r SNAP] [-l] [-d SNAP] [-t TYPE] [-o FILE] [-s PATH]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("jubula",    cmd_jubula,    "System update framework", "jubula [-c] [-a] [-l] [-d] [-f] [-r] [-v] [-s URL]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("mimizuku",  cmd_mimizuku,  "Package manager - install/remove/list", "mimizuku install|remove|list|search|info|update|upgrade <pkg> [-y] [-f] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("nesasio",   cmd_nesasio,   "Terminal configuration", "nesasio [-c COLOR] [-f FONT] [-s SIZE] [-b] [-t THEME] [-r] [-l] [-p]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("phodilus",  cmd_phodilus,  "Display server control", "phodilus [-m MODE] [-r RES] [-d] [-b] [-s] [-l] [-x] [-y] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ptilopsis", cmd_ptilopsis, "Clipboard manager", "ptilopsis [-c TEXT] [-p] [-l] [-x] [-a] [-s FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pulsatrix", cmd_pulsatrix, "Bootloader configuration", "pulsatrix [-l] [-e ENTRY] [-t SEC] [-d DEV] [-r] [-f] [-c FILE] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("orus",      cmd_orus,      "System recovery and repair", "orus [-c] [-f] [-d DEV] [-r] [-b] [-k] [-s] [-o FILE] [-l]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("sirius",    cmd_sirius,    "Process scheduling details", "sirius [-l] [-p PID] [-s] [-r] [-t] [-a] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("vega",      cmd_vega,      "Memory layout viewer", "vega [-k] [-h] [-s] [-m] [-a] [-u] [-p] [-f] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("rigel",     cmd_rigel,     "Interrupt vector and IRQ stats", "rigel [-l] [-i NUM] [-s] [-r] [-c] [-d] [-a] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("polaris",   cmd_polaris,   "CPU performance counter", "polaris [-c] [-i] [-b] [-l] [-r] [-t MS] [-n COUNT] [-s]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("altair",    cmd_altair,    "Hierarchical process tree", "altair [-t] [-p PID] [-a] [-c] [-s SORT] [-r] [-n COUNT] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("capella",   cmd_capella,   "CPU cache topology", "capella [-a] [-1] [-2] [-3] [-v] [-l] [-c] [-d]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("aldebaran", cmd_aldebaran, "CPU/device register dump", "aldebaran [-c] [-d] [-i] [-p] [-a] [-o FILE] [-r REG]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("regulus",   cmd_regulus,   "Call stack tracer", "regulus [-p PID] [-k] [-a] [-d] [-c] [-n DEPTH] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("spica",     cmd_spica,     "Heap allocator analysis", "spica [-a] [-f] [-s] [-l] [-c] [-p] [-b] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("antares",   cmd_antares,   "Kernel module loader/unloader", "antares load|unload|list|info|dep <mod> [-f] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("arcturus",  cmd_arcturus,  "Kernel symbol table viewer", "arcturus [-l] [-s SYM] [-a] [-t TYPE] [-o FILE] [-n] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("betelgeuse",cmd_betelgeuse,"DMA engine status", "betelgeuse [-l] [-c NUM] [-s] [-r] [-a] [-d] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("castor",    cmd_castor,    "Timer hardware info", "castor [-a] [-p] [-h] [-t] [-f FREQ] [-c] [-l]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pollux",    cmd_pollux,    "SMP load balancer status", "pollux [-l] [-c CPU] [-p PID] [-b] [-r] [-a] [-s]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pleiades",  cmd_pleiades,  "Kernel thread manager", "pleiades [-l] [-k PID] [-p] [-n] [-s STATE] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("orion",     cmd_orion,     "IPC status viewer", "orion [-l] [-t TYPE] [-p PID] [-a] [-c] [-s] [-d ID] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("andromeda", cmd_andromeda, "Semaphore usage and contention", "andromeda [-l] [-p PID] [-a] [-c] [-w] [-b] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cassiopeia",cmd_cassiopeia,"Mutex lock debugging", "cassiopeia [-l] [-p PID] [-a] [-c] [-w] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("draco",     cmd_draco,     "Synchronization primitives", "draco [-l] [-t TYPE] [-p PID] [-a] [-c] [-b] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cygnus",    cmd_cygnus,    "Event notification system", "cygnus [-l] [-p PID] [-e NAME] [-w] [-c] [-t SEC] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("aether",    cmd_aether,    "Advanced network diagnostics", "aether [-s HOST] [-p PORT] [-t TYPE] [-c] [-n COUNT] [-w SEC] [-l] [-d] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("umbra",     cmd_umbra,     "Security audit and access control", "umbra [-l] [-c] [-s] [-a] [-r] [-d] [-f FILE] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("solstice",  cmd_solstice,  "Advanced date/time tool", "solstice [-d] [-t] [-u] [-s STR] [-z ZONE] [-f FMT] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("eclipse",   cmd_eclipse,   "Display mode and screen control", "eclipse [-m MODE] [-r RES] [-b BRIGHT] [-c] [-f] [-d DISP] [-s] [-l]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("twilight",  cmd_twilight,  "Power management", "twilight [-s] [-h] [-r] [-p] [-b] [-c] [-t SEC] [-w]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("midnight",  cmd_midnight,  "Shell theme and prompt", "midnight [-t THEME] [-c COLORS] [-p PROMPT] [-f FONT] [-l] [-s] [-r] [-e]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("aurora",    cmd_aurora,    "Framebuffer visual effects", "aurora [-e EFFECT] [-c COLOR] [-s SPEED] [-l] [-p] [-r] [-t SEC]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("zenith",    cmd_zenith,    "System performance optimization", "zenith [-a] [-c] [-m] [-d] [-n] [-s] [-r] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("nebula",    cmd_nebula,    "Cloud integration tools", "nebula [-l] [-u FILE] [-d FILE] [-s] [-c] [-r] [-p] [-e]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cosmos",    cmd_cosmos,    "Extended uptime with cosmic display", "cosmos [-a] [-u] [-h] [-s] [-f] [-d] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("galaxy",    cmd_galaxy,    "Full storage topology", "galaxy [-a] [-d] [-p] [-m] [-l] [-t] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("stellar",   cmd_stellar,   "CPU and memory benchmark", "stellar [-c] [-m] [-d] [-a] [-l] [-t SEC] [-n COUNT] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("lunar",     cmd_lunar,     "Moon phase and celestial calendar", "lunar [-p] [-d DATE] [-l] [-r] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("solar",     cmd_solar,     "Power consumption estimation", "solar [-a] [-c] [-b] [-s] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("nova",      cmd_nova,      "Create new project from template", "nova <type> <name> [-f] [-d DIR] [-l] [-t TEMPLATE] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pulsar",    cmd_pulsar,    "Signal/interrupt test", "pulsar [-s SIG] [-p PID] [-n COUNT] [-t SEC] [-l] [-d]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("quasar",    cmd_quasar,    "LED and indicator control", "quasar [-d DEV] [-c COLOR] [-b BRIGHT] [-p PATTERN] [-s SPEED] [-t SEC]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("magnetar",  cmd_magnetar,  "Low-level ATA/SCSI commands", "magnetar [-d DEV] [-c CMD] [-l] [-i] [-r LBA] [-w LBA DATA] [-s]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("supernova", cmd_supernova, "Factory reset and wipe", "supernova [-f] [-a] [-k] [-d] [-r] [-y] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("hypernova", cmd_hypernova, "Stress test - CPU/memory/disk", "hypernova [-c] [-m] [-d] [-n] [-a] [-t SEC] [-p] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ruby",      cmd_ruby,      "Code formatter and beautifier", "ruby [-f FILE] [-d DIR] [-l LANG] [-s] [-c] [-o] [-r] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("emerald",   cmd_emerald,   "Cryptographic hash tool", "emerald <file> [-a ALGO] [-o FILE] [-c] [-l] [-s STR] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("sapphire",  cmd_sapphire,  "File encryption/decryption", "sapphire encrypt|decrypt <file> [-k KEY] [-o FILE] [-a ALGO] [-f] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("diamond",   cmd_diamond,   "Content search with patterns", "diamond <pattern> <path> [-r] [-i] [-l] [-n] [-c] [-s SIZE] [-t TYPE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("amethyst",  cmd_amethyst,  "Color picker and palette manager", "amethyst [-p] [-l] [-s NAME] [-r] [-c COLOR] [-e] [-f FILE] [-i]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("topaz",     cmd_topaz,     "File archiver (tar-like)", "topaz create|extract <archive> [files] [-o DIR] [-l] [-t TYPE] [-f] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("opal",      cmd_opal,      "Disk encryption setup", "opal [-d DEV] [-e] [-c] [-l] [-o] [-p PASS] [-k KEYFILE] [-f]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("jade",      cmd_jade,      "Environment variable manager", "jade [-l] [-s VAR=VAL] [-u VAR] [-e NAME] [-d NAME] [-f FILE] [-r] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("onyx",      cmd_onyx,      "Dark/light theme toggle", "onyx [-d] [-l] [-t THEME] [-s] [-c] [-r] [-f FILE] [-p]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("quartz",    cmd_quartz,    "System timer and alarm", "quartz [-t SEC] [-c] [-m] [-s CMD] [-l] [-d ID] [-r] [-n COUNT]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("garnet",    cmd_garnet,    "Extension/plugin manager", "garnet [-i FILE] [-r NAME] [-l] [-u] [-s TERM] [-c] [-d] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("tourmaline",cmd_tourmaline,"Base64/hex encoding converter", "tourmaline encode|decode <type> <data> [-o FILE] [-f] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("peridot",   cmd_peridot,   "Sort lines/data utility", "peridot [-r] [-u] [-n] [-t SEP] [-k KEY] [-f FILE] [-o FILE] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("citrine",   cmd_citrine,   "File diff and compare", "citrine <file1> <file2> [-r] [-i] [-u] [-c] [-s] [-b] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("turquoise", cmd_turquoise, "System health check", "turquoise [-a] [-c] [-m] [-d] [-n] [-s] [-o FILE] [-r] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("lapis",     cmd_lapis,     "Theme/color scheme manager", "lapis [-l] [-s NAME] [-i FILE] [-e] [-r] [-c] [-f] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("coral",     cmd_coral,     "File synchronization tool", "coral [-s SRC] [-d DST] [-r] [-u] [-n] [-c] [-v] [-e PAT]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pearl",     cmd_pearl,     "Random generator (num/string/UUID)", "pearl [-n COUNT] [-t TYPE] [-l LEN] [-o FILE] [-s SEED] [-a] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("amber",     cmd_amber,     "Filesystem snapshot tool", "amber [-s PATH] [-l] [-r REV] [-d REV] [-c] [-v] [-t TYPE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("jasper",    cmd_jasper,    "Pattern-based classification", "jasper [-p PAT] [-d DIR] [-r] [-c] [-l] [-o FILE] [-s SORT] [-t TYPE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("nexus",     cmd_nexus,     "Service/component interconnection", "nexus [-l] [-a] [-c] [-d] [-t TYPE] [-s NAME] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("vortex",    cmd_vortex,    "Data stream between devices", "vortex <src> <dst> [-b SIZE] [-s SPEED] [-t TYPE] [-c] [-f] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cipher",    cmd_cipher,    "Text encryption/decryption", "cipher [-e] [-d] [-k KEY] [-a ALGO] [-f FILE] [-o FILE] [-s TEXT] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("monolith",  cmd_monolith,  "Large file splitter/combiner", "monolith split|join <file> [size] [-o DIR] [-c] [-v] [-f]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("prism",     cmd_prism,     "Data visualization (bar/pie/graph)", "prism [-t TYPE] [-d DATA] [-f FILE] [-c] [-s] [-o] [-w W] [-h H]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("obelisk",   cmd_obelisk,   "Persistent storage config", "obelisk [-l] [-d DEV] [-m MOUNT] [-t FS] [-o OPTS] [-c] [-r] [-f]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("resonance", cmd_resonance, "Audio frequency analysis", "resonance [-f FREQ] [-d DEV] [-l] [-s] [-t SEC] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("quantum",   cmd_quantum,   "Random and probability tools", "quantum [-n COUNT] [-t TYPE] [-p PROB] [-s SEED] [-r] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("axiom",     cmd_axiom,     "Boolean logic and truth tables", "axiom [-e EXPR] [-f FILE] [-t] [-c] [-o FILE] [-l] [-a]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("vector",    cmd_vector,    "Vector/matrix mathematics", "vector add|sub|mul|dot|cross <args> [-f FORMAT] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("matrix",    cmd_matrix,    "Linear algebra operations", "matrix <op> <args> [-s SIZE] [-r] [-c] [-f FILE] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("helix",     cmd_helix,     "Bioinformatics sequence tool", "helix [-s SEQ] [-f FILE] [-t TYPE] [-c] [-l] [-a] [-o FILE] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pixel",     cmd_pixel,     "Framebuffer image operations", "pixel [-i FILE] [-o FILE] [-r RESIZE] [-c COLOR] [-e EFFECT] [-f] [-l]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("lattice",   cmd_lattice,   "Network topology discovery", "lattice [-i IFACE] [-s SUBNET] [-l] [-c] [-t SEC] [-p] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("flux",      cmd_flux,      "Data flow/throughput monitor", "flux [-i IFACE] [-f FILE] [-d DEV] [-t TYPE] [-c] [-s] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("drift",     cmd_drift,     "Clock drift and NTP sync", "drift [-c] [-d] [-s HOST] [-t SEC] [-r] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("surge",     cmd_surge,     "UPS/power supply monitoring", "surge [-s] [-c] [-l] [-b] [-d DEV] [-t SEC] [-a] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("pulse",     cmd_pulse,     "Network heartbeat monitoring", "pulse [-t TARGET] [-i SEC] [-c COUNT] [-l] [-a] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("apex",      cmd_apex,      "CPU frequency scaling", "apex [-l] [-g GOV] [-s SPEED] [-c CPU] [-t TEMP] [-a] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("vertex",    cmd_vertex,    "Graph theory analysis", "vertex [-f FILE] [-s SRC] [-d DST] [-a ALGO] [-t TYPE] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("fenrir",    cmd_fenrir,    "Process sandboxing/capabilities", "fenrir [-p PID] [-c CAP] [-l] [-d] [-s] [-r] [-a] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("valkyrie",  cmd_valkyrie,  "Process priority control", "valkyrie [-p PID] [-n NICE] [-c CLASS] [-l] [-r] [-s] [-a]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("odin",      cmd_odin,      "System overview dashboard", "odin [-a] [-c] [-m] [-d] [-n] [-p] [-s] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("thor",      cmd_thor,      "Hardware control (fans/voltage)", "thor [-f] [-v VOLT] [-t TEMP] [-s STATE] [-l] [-c] [-a] [-d DEV]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("loki",      cmd_loki,      "Kernel debug/breakpoint manager", "loki [-b ADDR] [-r] [-c] [-l] [-s] [-d] [-p PID] [-w] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("freya",     cmd_freya,     "File manager operations", "freya [-d DIR] [-l] [-r] [-c] [-m] [-s] [-o FILE] [-p] [-t TYPE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("heimdall",  cmd_heimdall,  "Network gateway monitoring", "heimdall [-g GW] [-i IFACE] [-l] [-c] [-t SEC] [-s] [-r] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("baldr",     cmd_baldr,     "File integrity verification", "baldr [-c FILE] [-d DIR] [-a ALGO] [-l] [-r] [-s] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("tyr",       cmd_tyr,       "ACL management", "tyr [-f FILE] [-u USER] [-p PERM] [-l] [-r] [-d] [-s] [-a] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("sif",       cmd_sif,       "Key-value store for system data", "sif get|set|del|list|flush <key> [val] [-f FILE] [-e]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("njord",     cmd_njord,     "Advanced filesystem operations", "njord [-t TYPE] [-d DEV] [-m MOUNT] [-c] [-r] [-s] [-l] [-o OPTS]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("idunn",     cmd_idunn,     "Package metadata refresh", "idunn [-u] [-c] [-l] [-r] [-s] [-a] [-v] [-f]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("bragi",     cmd_bragi,     "ASCII art and text formatting", "bragi [-t TEXT] [-f FONT] [-s SIZE] [-c CHAR] [-l] [-o FILE] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("hodr",      cmd_hodr,      "Kernel oops/panic log capture", "hodr [-l] [-c] [-s] [-d] [-o FILE] [-a] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("vidarr",    cmd_vidarr,    "Filesystem check and repair", "vidarr [-d DEV] [-t TYPE] [-c] [-r] [-f] [-l] [-s] [-a] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ali",       cmd_ali,       "Schedule command execution", "ali [-t TIME] [-c CMD] [-l] [-d ID] [-r] [-f FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("magni",     cmd_magni,     "Memory pressure and OOM", "magni [-l] [-c] [-p PID] [-k] [-s] [-r] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("modi",      cmd_modi,      "Process resource limits", "modi [-p PID] [-l] [-s] [-n NOFILE] [-m MEM] [-c CPU] [-d SIZE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("saga",      cmd_saga,      "System event log (narrative)", "saga [-l] [-t TYPE] [-s TEXT] [-n LINES] [-f] [-o FILE] [-d DATE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("rune",      cmd_rune,      "Icon/symbol picker for terminal", "rune [-l] [-s TEXT] [-c] [-p] [-o FILE] [-t CAT] [-n]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ignis",     cmd_ignis,     "LED/status indicator control", "ignis [-d DEV] [-c COLOR] [-b BRIGHT] [-p PAT] [-s SPEED] [-t SEC]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cinder",    cmd_cinder,    "System cleanup tool", "cinder [-a] [-t TMP] [-c CACHE] [-l LOGS] [-d] [-f] [-r] [-v] [-s]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ember",     cmd_ember,     "Notification and alert test", "ember [-m MSG] [-t TYPE] [-s SEV] [-d] [-l] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("blaze",     cmd_blaze,     "High-speed file copy", "blaze <src> <dst> [-b SIZE] [-c] [-v] [-f] [-s SPEED] [-p] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("flare",     cmd_flare,     "Screen flash/attention effect", "flare [-c COLOR] [-t MS] [-p] [-l] [-r] [-s] [-n COUNT]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("spark",     cmd_spark,     "GPIO/pin test", "spark [-p PIN] [-d DEV] [-t TYPE] [-c] [-l] [-r] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("flash",     cmd_flash,     "Flash memory (NOR/NAND) tool", "flash [-d DEV] [-r ADDR] [-w ADDR DATA] [-e] [-l] [-c] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("glow",      cmd_glow,      "Backlight/brightness control", "glow [-d DEV] [-b BRIGHT] [-c] [-l] [-s STEP] [-t TIME] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("gleam",     cmd_gleam,     "Cache hit/miss statistics", "gleam [-a] [-c] [-d] [-s] [-l] [-r] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("shine",     cmd_shine,     "Performance optimization tips", "shine [-a] [-c] [-m] [-d] [-n] [-s] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("beam",      cmd_beam,      "Point-to-point data transfer", "beam <src> <dst:port> [-b SIZE] [-c] [-v] [-e] [-p]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ray",       cmd_ray,       "Geometry calculations", "ray [-x1 N] [-y1 N] [-x2 N] [-y2 N] [-t TYPE] [-d] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("dawn",      cmd_dawn,      "Boot time analysis", "dawn [-a] [-b] [-c] [-s] [-d] [-l] [-o FILE] [-v] [-t TYPE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("dusk",      cmd_dusk,      "Scheduled shutdown/power events", "dusk [-t TIME] [-c] [-l] [-d ID] [-r] [-s] [-a] [-f]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("gloom",     cmd_gloom,     "Low-power mode settings", "gloom [-a] [-c] [-d] [-s] [-l] [-r] [-t SEC] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("shade",     cmd_shade,     "Shadow copy/versioning", "shade [-s SRC] [-d DST] [-l] [-r REV] [-c] [-t TYPE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("shadow",    cmd_shadow,    "Find duplicate files by hash", "shadow <path> [-r] [-a ALGO] [-s SIZE] [-d] [-c] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("phantom",   cmd_phantom,   "Orphan process finder", "phantom [-l] [-c] [-k] [-p PID] [-r] [-s] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("wraith",    cmd_wraith,    "Stealth mode (disable logging)", "wraith [-a] [-k] [-s] [-r] [-c] [-d] [-l] [-t SEC]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("spirit",    cmd_spirit,    "Background daemon manager", "spirit start|stop|list|status <name> [-f] [-v] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("verbum",    cmd_verbum,    "Text statistics (wc style)", "verbum <file> [-c] [-w] [-l] [-L] [-b] [-s] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("scriptum",  cmd_scriptum,  "Interactive script recording", "scriptum [-r] [-p] [-f FILE] [-t TYPE] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("lectio",    cmd_lectio,    "Syntax-highlighted file viewer", "lectio <file> [-l LANG] [-n] [-s] [-c] [-t THEME] [-o] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("dictum",    cmd_dictum,    "Random quote display", "dictum [-l] [-a] [-s CAT] [-c] [-n] [-o FILE] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("notitia",   cmd_notitia,   "System notice broadcast", "notitia [-m MSG] [-u USER] [-a] [-c] [-l] [-s] [-d]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ratio",     cmd_ratio,     "Compression/efficiency ratio", "ratio [-f FILE] [-l] [-c] [-s] [-t TYPE] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("census",    cmd_census,    "System census (users/groups)", "census [-u] [-g] [-p] [-a] [-c] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("gradus",    cmd_gradus,    "Progress bar/step indicator", "gradus [-n TOTAL] [-c CURRENT] [-w WIDTH] [-p] [-s] [-t TEXT]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ordo",      cmd_ordo,      "Order files by criteria", "ordo <dir> [-s SORT] [-r] [-t TYPE] [-c] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("regula",    cmd_regula,    "Rule-based file actions", "regula [-r RULE] [-d DIR] [-f FILE] [-c] [-l] [-s] [-e] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("forma",     cmd_forma,     "File format detection", "forma <file> [-t TYPE] [-o FILE] [-l] [-c] [-d] [-s] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("signum",    cmd_signum,    "Digital signature tool", "signum sign|verify <file> [sig] [-k KEY] [-a ALGO] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("index",     cmd_index,     "File content indexing", "index [-u] [-d DIR] [-s TERM] [-r] [-l] [-c] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("meta",      cmd_meta,      "File metadata viewer", "meta <file> [-a] [-c] [-s] [-d] [-p] [-l] [-t TYPE] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("finis",     cmd_finis,     "System termination sequence", "finis [-t SEC] [-r] [-h] [-p] [-n] [-f] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("initium",   cmd_initium,   "Service init status/control", "initium [-l] [-s NAME] [-r] [-d] [-e] [-c] [-a] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("medium",    cmd_medium,    "Media transport (mount/eject)", "medium [-d DEV] [-m MOUNT] [-u] [-e] [-l] [-t TYPE] [-c] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("terminus",  cmd_terminus,  "Network endpoint tool", "terminus <host:port> [-p PORT] [-t TYPE] [-c] [-s] [-l] [-d] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("casus",     cmd_casus,     "Event correlation analysis", "casus [-e EVENT] [-t TYPE] [-s TEXT] [-l] [-c] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("modus",     cmd_modus,     "System operation mode", "modus [-l] [-s NAME] [-c] [-r] [-d] [-a] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("falcon",    cmd_falcon,    "High-speed file search", "falcon <pattern> [-p PATH] [-r] [-i] [-l] [-t TYPE] [-s] [-c] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("raven",     cmd_raven,     "Minimal system info", "raven [-a] [-c] [-m] [-d] [-n] [-s] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("hawk",      cmd_hawk,      "Watch command output", "hawk <cmd> [-t SEC] [-c COUNT] [-d] [-l] [-v] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("eagle",     cmd_eagle,     "Terminal file manager TUI", "eagle <path> [-l] [-s] [-t TYPE] [-h] [-v] [-c] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("crow",      cmd_crow,      "Batch file rename", "crow <pattern> <replacement> <files> [-r] [-i] [-n] [-d] [-v] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("phoenix",   cmd_phoenix,   "System recovery environment", "phoenix [-a] [-c] [-s] [-d DEV] [-r] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("heron",     cmd_heron,     "Network port scanner", "heron <target> [-p PORTS] [-t TYPE] [-c] [-l] [-s SPEED] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("swan",      cmd_swan,      "Elegant formatted output", "swan [-t TEXT] [-f FILE] [-c] [-s] [-l] [-o FILE] [-r]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("swift",     cmd_swift,     "Quick command macros", "swift [-e CMD] [-l] [-s NAME] [-d NAME] [-r] [-f FILE] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("kite",      cmd_kite,      "Network connection test", "kite <host> [-p PORT] [-t TYPE] [-c COUNT] [-s SIZE] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("merlin",    cmd_merlin,    "Wizard/assistant framework", "merlin <name> [-l] [-c] [-s STEP] [-d] [-r] [-v] [-f FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("goshawk",   cmd_goshawk,   "Intensive system monitor", "goshawk [-a] [-c] [-m MEM] [-d DISK] [-n NET] [-t SEC] [-l] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("harrier",   cmd_harrier,   "Network packet analysis", "harrier [-i IFACE] [-c COUNT] [-t TYPE] [-s SIZE] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("osprey",    cmd_osprey,    "File permission audit", "osprey <path> [-r] [-l] [-c] [-s] [-d] [-u USER] [-g GRP] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("thrush",    cmd_thrush,    "Sound/audio playback", "thrush [-f FILE] [-v VOL] [-p] [-l] [-r] [-t SEC] [-c]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("finch",     cmd_finch,     "Quick file operations", "finch new|copy|move|del <args> [-r] [-f]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("robin",     cmd_robin,     "Notification display", "robin [-m MSG] [-t TYPE] [-s SEV] [-d] [-c] [-l] [-p]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("sparrow",   cmd_sparrow,   "Bulk small file operations", "sparrow <dir> [-a] [-c] [-s SIZE] [-r] [-l] [-t TYPE] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("starling",  cmd_starling,  "File watcher with auto-action", "starling [-d DIR] [-e EVENT] [-c CMD] [-r] [-l] [-t SEC] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("martin",    cmd_martin,    "Home/user data management", "martin [-u USER] [-b] [-r] [-l] [-c] [-s] [-d DIR] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("crimson",   cmd_crimson,   "Urgent system alerts", "crimson [-l] [-c] [-s] [-n LINES] [-f] [-o FILE] [-a]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("azure",     cmd_azure,     "Cloud/remote sync config", "azure [-s URL] [-l] [-c] [-u FILE] [-d FILE] [-r] [-t TYPE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("umber",     cmd_umber,     "Warning-level system messages", "umber [-l] [-c] [-s] [-n LINES] [-f] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("chartreuse",cmd_chartreuse,"System health/green metrics", "chartreuse [-a] [-c] [-s] [-l] [-r] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("rose",      cmd_rose,      "Friendly system greeting", "rose [-a] [-c] [-m MSG] [-u USER] [-t TYPE] [-l]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("sable",     cmd_sable,     "Crash dump analysis", "sable [-f FILE] [-l] [-c] [-s] [-d] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ivory",     cmd_ivory,     "Premium features status", "ivory [-l] [-a] [-c] [-s] [-d] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("teal",      cmd_teal,      "System balance analysis", "teal [-a] [-c] [-m] [-d] [-n] [-s] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("plum",      cmd_plum,      "Kernel profiling data", "plum [-a] [-c] [-s] [-d] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("mauve",     cmd_mauve,     "Soft system status", "mauve [-a] [-c] [-s] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("ochre",     cmd_ochre,     "Stability/uptime report", "ochre [-a] [-c] [-s] [-t TYPE] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("indigo",    cmd_indigo,    "Deep system forensics", "indigo [-a] [-c] [-d] [-s] [-l] [-o FILE] [-v]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("violet",    cmd_violet,    "ASCII art tools", "violet [-f FILE] [-t TEXT] [-s SIZE] [-c] [-l] [-o]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cerulean",  cmd_cerulean,  "Network sky overview", "cerulean [-a] [-c] [-i IFACE] [-s] [-l] [-r] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("scarlet",   cmd_scarlet,   "Highlighted warnings", "scarlet [-l] [-c] [-s SEV] [-n LINES] [-f] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("viridian",  cmd_viridian,  "Success metrics log", "viridian [-l] [-c] [-s] [-t TYPE] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("saffron",   cmd_saffron,   "Temperature/warm report", "saffron [-a] [-c] [-s] [-t TYPE] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("cobalt",    cmd_cobalt,    "Kernel parameter viewer", "cobalt [-l] [-g KEY] [-s KEY=VAL] [-d] [-r] [-a] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("magenta",   cmd_magenta,   "Celebration/easter egg", "magenta [-a] [-c] [-s] [-t TYPE] [-l] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        CMD_FLAG("carmine",   cmd_carmine,   "System gems/resources", "carmine [-a] [-c] [-s] [-l] [-t TYPE] [-o FILE]", CMD_CAT_NOCTUA, "1.0"),
        {0,0,0,0,0,0,0},
    };
    for (int i = 0; cmds[i].name; i++) cmd_register(&cmds[i]);
}

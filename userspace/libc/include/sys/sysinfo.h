#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H

struct sysinfo {
    long uptime;
};

int sysinfo(struct sysinfo *info);

#endif

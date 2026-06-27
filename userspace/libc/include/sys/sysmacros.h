#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H

#define major(dev)  ((dev >> 8) & 0xff)
#define minor(dev)  (dev & 0xff)
#define makedev(major, minor) ((major << 8) | (minor))

#endif
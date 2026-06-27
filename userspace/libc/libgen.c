#include <string.h>
#include <libgen.h>

char *dirname(char *path)
{
    static char buf[256];
    char *p;
    int i;

    if (!path || !*path) {
        buf[0] = '.';
        buf[1] = '\0';
        return buf;
    }

    i = strlen(path);
    while (i > 0 && path[i - 1] == '/')
        i--;
    if (i == 0) {
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }

    path[i] = '\0';

    p = strrchr(path, '/');
    if (!p) {
        buf[0] = '.';
        buf[1] = '\0';
        return buf;
    }

    if (p == path) {
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }

    int len = p - path;
    memcpy(buf, path, len);
    buf[len] = '\0';

    return buf;
}

char *basename(char *path)
{
    static char buf[256];
    char *p;
    int i;

    if (!path || !*path) {
        buf[0] = '.';
        buf[1] = '\0';
        return buf;
    }

    i = strlen(path);
    while (i > 0 && path[i - 1] == '/')
        i--;

    if (i == 0) {
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }

    path[i] = '\0';

    p = strrchr(path, '/');
    if (!p)
        return path;

    return p + 1;
}

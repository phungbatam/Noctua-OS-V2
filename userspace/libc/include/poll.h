#ifndef _POLL_H
#define _POLL_H

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN  1
#define POLLOUT 4

int poll(struct pollfd *fds, unsigned long nfds, int timeout);

#endif

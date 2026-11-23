//
// FILE            corLinux.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef CORBASE_CORLINUX_H_
#define CORBASE_CORLINUX_H_

//
// corLinux - the Linux calls the cor-libs use, on macOS too
//
// Linux: the system's own headers and nothing else - the code a Linux build runs is the code it always ran.
// macOS (c³ runs there; coraine does not):
//
//   epoll_create1, epoll_ctl, epoll_wait   on kqueue: EPOLLIN/EPOLLRDHUP the read filter, EPOLLOUT the write
//                                          filter, EPOLLET EV_CLEAR, EPOLLONESHOT EV_ONESHOT; one event per
//                                          fd and wait, as epoll gives it
//   eventfd                                a pipe: the read end is the eventfd (poll, epoll, read); its write
//                                          end is found by corEventfdSignal and closed by corEventfdClose
//   accept4                                accept + O_NONBLOCK/FD_CLOEXEC, and SO_NOSIGPIPE on the socket
//   sendfile                               Linux's argument order on macOS's sendfile
//   MSG_NOSIGNAL                           0 - a program on macOS ignores SIGPIPE (signal(SIGPIPE, SIG_IGN))
//   pipe2                                  pipe + O_CLOEXEC/O_NONBLOCK on both ends
//   socket                                 SOCK_CLOEXEC/SOCK_NONBLOCK in the type, as on Linux
//   close_range                            each open fd from first to last closed (CLOSE_RANGE_CLOEXEC: marked)
//
// Include it after the system headers (the house order): on macOS it renames sendfile, which
// <sys/socket.h> declares with macOS's arguments.
//
#include <stdint.h>                     // uint64_t
#include <unistd.h>                     // write, close

#if defined(__linux__)

#include <sys/epoll.h>                  // epoll_create1, epoll_ctl, epoll_wait
#include <sys/eventfd.h>                // eventfd
#include <sys/sendfile.h>               // sendfile

//
// corEventfdSignal - wake whoever waits on an eventfd; corEventfdClose - close it
//
#define corEventfdSignal(fd)  do { uint64_t one_ = 1; (void) !write((fd), &one_, sizeof(one_)); } while (0)
#define corEventfdClose(fd)   close(fd)

#elif defined(__APPLE__)

#include <sys/types.h>                  // ssize_t, off_t
#include <sys/socket.h>                 // struct sockaddr, socklen_t, sendfile

typedef union epoll_data
{
  void*     ptr;
  int       fd;
  uint32_t  u32;
  uint64_t  u64;
} epoll_data_t;

struct epoll_event
{
  uint32_t      events;
  epoll_data_t  data;
};

enum
{
  EPOLLIN      = 0x001,
  EPOLLOUT     = 0x004,
  EPOLLERR     = 0x008,
  EPOLLHUP     = 0x010,
  EPOLLRDHUP   = 0x2000,
  EPOLLONESHOT = 1u << 30,
  EPOLLET      = 1u << 31
};

enum { EPOLL_CTL_ADD = 1, EPOLL_CTL_DEL = 2, EPOLL_CTL_MOD = 3 };
enum { EPOLL_CLOEXEC = 0x80000, EFD_CLOEXEC = 0x80000, EFD_NONBLOCK = 0x800 };
enum { SOCK_NONBLOCK = 0x10000000, SOCK_CLOEXEC = 0x20000000 };
enum { CLOSE_RANGE_CLOEXEC = 4 };

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

extern int     corEpollCreate1(int flags);
extern int     corEpollCtl(int epollFd, int op, int fd, struct epoll_event* evP);
extern int     corEpollWait(int epollFd, struct epoll_event* evV, int maxEvents, int timeoutMs);
extern int     corEventfd(unsigned int initval, int flags);
extern void    corEventfdSignal(int fd);
extern int     corEventfdClose(int fd);
extern int     corAccept4(int fd, struct sockaddr* addrP, socklen_t* lenP, int flags);
extern ssize_t corSendfile(int outFd, int inFd, off_t* offsetP, size_t count);
extern int     corPipe2(int fds[2], int flags);
extern int     corSocket(int domain, int type, int protocol);
extern int     corCloseRange(unsigned int first, unsigned int last, int flags);

#define epoll_create1  corEpollCreate1
#define epoll_ctl      corEpollCtl
#define epoll_wait     corEpollWait
#define eventfd        corEventfd
#define accept4        corAccept4
#define sendfile       corSendfile
#define pipe2          corPipe2
#define socket         corSocket
#define close_range    corCloseRange

#else
#error "corLinux: Linux or macOS"
#endif

#endif  // CORBASE_CORLINUX_H_

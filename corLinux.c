//
// FILE            corLinux.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
//
// SPDX-License-Identifier: Apache-2.0
//
// The macOS side of corLinux.h: epoll on kqueue, eventfd on a pipe, accept4, sendfile. Nothing on Linux.
//
#if defined(__APPLE__)

#include <errno.h>                      // errno, ENOENT
#include <fcntl.h>                      // fcntl, O_NONBLOCK, FD_CLOEXEC
#include <pthread.h>                    // pthread_mutex_*
#include <stdbool.h>                    // bool
#include <stdlib.h>                     // realloc
#include <string.h>                     // memset
#include <sys/event.h>                  // kqueue, kevent, EV_SET
#include <sys/socket.h>                 // accept, setsockopt, sendfile, SO_NOSIGPIPE
#include <sys/time.h>                   // struct timespec
#include <unistd.h>                     // pipe, close, write, lseek

#include "corBase/corLinux.h"           // Own interface

#undef sendfile                         // macOS's own, below
#undef socket



// -----------------------------------------------------------------------------
//
// corEpollCreate1 -
//
int corEpollCreate1(int flags)
{
  int kq = kqueue();

  if ((kq >= 0) && (flags & EPOLL_CLOEXEC))
    fcntl(kq, F_SETFD, FD_CLOEXEC);

  return kq;
}



// -----------------------------------------------------------------------------
//
// filterSet - one kqueue filter of an fd added (with flags) or deleted; deleting one not there is no error
//
static int filterSet(int kq, int fd, short filter, bool add, unsigned short flags, void* udata)
{
  struct kevent kv;

  EV_SET(&kv, fd, filter, add ? (EV_ADD | EV_ENABLE | flags) : EV_DELETE, 0, 0, udata);

  if ((kevent(kq, &kv, 1, NULL, 0, NULL) == -1) && ((add == true) || (errno != ENOENT)))
    return -1;

  return 0;
}



// -----------------------------------------------------------------------------
//
// corEpollCtl - ADD and MOD set the read filter for EPOLLIN/EPOLLRDHUP and the write filter for EPOLLOUT,
// and delete the one not asked for; DEL deletes both
//
int corEpollCtl(int epollFd, int op, int fd, struct epoll_event* evP)
{
  if (op == EPOLL_CTL_DEL)
  {
    int r = filterSet(epollFd, fd, EVFILT_READ, false, 0, NULL);
    int w = filterSet(epollFd, fd, EVFILT_WRITE, false, 0, NULL);

    return ((r == 0) && (w == 0)) ? 0 : -1;
  }

  uint32_t       e     = evP->events;
  unsigned short flags = ((e & EPOLLET) ? EV_CLEAR : 0) | ((e & EPOLLONESHOT) ? EV_ONESHOT : 0);
  void*          udata = (void*) (uintptr_t) evP->data.u64;

  if (filterSet(epollFd, fd, EVFILT_READ, (e & (EPOLLIN | EPOLLRDHUP)) != 0, flags, udata) != 0)
    return -1;

  return filterSet(epollFd, fd, EVFILT_WRITE, (e & EPOLLOUT) != 0, flags, udata);
}



// -----------------------------------------------------------------------------
//
// corEpollWait - the kqueue events, one per fd (a read and a write event of one fd merged, as epoll gives
// them); a one-shot event deletes the fd's other filter, so a one-shot fd fires once, as with epoll
//
int corEpollWait(int epollFd, struct epoll_event* evV, int maxEvents, int timeoutMs)
{
  struct kevent    kv[64];
  uintptr_t        identV[64];
  struct timespec  ts;
  struct timespec* tsP  = NULL;
  int              want = (maxEvents < 64) ? maxEvents : 64;

  if (timeoutMs >= 0)
  {
    ts.tv_sec  = timeoutMs / 1000;
    ts.tv_nsec = (long) (timeoutMs % 1000) * 1000000L;
    tsP        = &ts;
  }

  int n = kevent(epollFd, NULL, 0, kv, want, tsP);

  if (n <= 0)
    return n;

  int out = 0;

  for (int ix = 0; ix < n; ix++)
  {
    uint32_t e   = 0;
    bool     eof = (kv[ix].flags & EV_EOF) != 0;

    if (kv[ix].filter == EVFILT_READ)
      e = EPOLLIN | (eof ? EPOLLRDHUP : 0);
    else if (kv[ix].filter == EVFILT_WRITE)
      e = EPOLLOUT | (eof ? EPOLLHUP : 0);
    else
      continue;

    if (eof && (kv[ix].fflags != 0))
      e |= EPOLLERR;

    int jx = 0;
    while ((jx < out) && (identV[jx] != kv[ix].ident))
      ++jx;

    if (jx < out)
      evV[jx].events |= e;
    else
    {
      identV[out]          = kv[ix].ident;
      evV[out].events      = e;
      evV[out].data.u64    = (uint64_t) (uintptr_t) kv[ix].udata;
      ++out;
    }

    if (kv[ix].flags & EV_ONESHOT)
      filterSet(epollFd, (int) kv[ix].ident, (kv[ix].filter == EVFILT_READ) ? EVFILT_WRITE : EVFILT_READ, false, 0, NULL);
  }

  return out;
}



// -----------------------------------------------------------------------------
//
// The eventfds' write ends, by read end: writeEndV[readFd] = writeFd + 1 (0: not an eventfd)
//
static pthread_mutex_t  eventfdMutex = PTHREAD_MUTEX_INITIALIZER;
static int*             writeEndV    = NULL;
static int              writeEnds    = 0;



// -----------------------------------------------------------------------------
//
// corEventfd - a pipe; the read end is the eventfd
//
int corEventfd(unsigned int initval, int flags)
{
  int p[2];

  if (pipe(p) != 0)
    return -1;

  fcntl(p[1], F_SETFL, fcntl(p[1], F_GETFL) | O_NONBLOCK);          // a full pipe is a woken one already
  if (flags & EFD_NONBLOCK)
    fcntl(p[0], F_SETFL, fcntl(p[0], F_GETFL) | O_NONBLOCK);
  if (flags & EFD_CLOEXEC)
  {
    fcntl(p[0], F_SETFD, FD_CLOEXEC);
    fcntl(p[1], F_SETFD, FD_CLOEXEC);
  }

  pthread_mutex_lock(&eventfdMutex);
  if (p[0] >= writeEnds)
  {
    int  size = (p[0] + 1 > 2 * writeEnds) ? p[0] + 64 : 2 * writeEnds;
    int* v    = realloc(writeEndV, size * sizeof(int));

    if (v == NULL)
    {
      pthread_mutex_unlock(&eventfdMutex);
      close(p[0]);
      close(p[1]);
      errno = ENOMEM;
      return -1;
    }

    memset(&v[writeEnds], 0, (size - writeEnds) * sizeof(int));
    writeEndV = v;
    writeEnds = size;
  }
  writeEndV[p[0]] = p[1] + 1;
  pthread_mutex_unlock(&eventfdMutex);

  if (initval > 0)
    corEventfdSignal(p[0]);

  return p[0];
}



// -----------------------------------------------------------------------------
//
// corEventfdSignal - eight bytes into the pipe: the read end is readable until they are read
//
void corEventfdSignal(int fd)
{
  pthread_mutex_lock(&eventfdMutex);
  int w = ((fd >= 0) && (fd < writeEnds)) ? writeEndV[fd] - 1 : -1;
  pthread_mutex_unlock(&eventfdMutex);

  if (w >= 0)
  {
    uint64_t one = 1;
    (void) !write(w, &one, sizeof(one));
  }
}



// -----------------------------------------------------------------------------
//
// corEventfdClose - both ends
//
int corEventfdClose(int fd)
{
  pthread_mutex_lock(&eventfdMutex);
  int w = ((fd >= 0) && (fd < writeEnds)) ? writeEndV[fd] - 1 : -1;
  if (w >= 0)
    writeEndV[fd] = 0;
  pthread_mutex_unlock(&eventfdMutex);

  if (w >= 0)
    close(w);

  return close(fd);
}



// -----------------------------------------------------------------------------
//
// corAccept4 -
//
int corAccept4(int fd, struct sockaddr* addrP, socklen_t* lenP, int flags)
{
  int s = accept(fd, addrP, lenP);

  if (s < 0)
    return s;

  if (flags & SOCK_NONBLOCK)
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
  if (flags & SOCK_CLOEXEC)
    fcntl(s, F_SETFD, FD_CLOEXEC);

  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));

  return s;
}



// -----------------------------------------------------------------------------
//
// corSendfile - Linux's sendfile(out, in, &offset, count) on macOS's sendfile(in, out, offset, &len, ...):
// the bytes sent, also when a non-blocking socket took only some (macOS: -1 EAGAIN with len set)
//
ssize_t corSendfile(int outFd, int inFd, off_t* offsetP, size_t count)
{
  off_t offset = (offsetP != NULL) ? *offsetP : lseek(inFd, 0, SEEK_CUR);
  off_t len    = (off_t) count;

  if ((sendfile(inFd, outFd, offset, &len, NULL, 0) == -1) && (len == 0))
    return -1;

  if (offsetP != NULL)
    *offsetP += len;
  else
    lseek(inFd, len, SEEK_CUR);

  return (ssize_t) len;
}




// -----------------------------------------------------------------------------
//
// flagsSet - O_CLOEXEC/O_NONBLOCK (or their SOCK_ twins) on an fd
//
static void flagsSet(int fd, bool cloexec, bool nonblock)
{
  if (cloexec)
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  if (nonblock)
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}



// -----------------------------------------------------------------------------
//
// corPipe2 -
//
int corPipe2(int fds[2], int flags)
{
  if (pipe(fds) != 0)
    return -1;

  flagsSet(fds[0], (flags & O_CLOEXEC) != 0, (flags & O_NONBLOCK) != 0);
  flagsSet(fds[1], (flags & O_CLOEXEC) != 0, (flags & O_NONBLOCK) != 0);

  return 0;
}



// -----------------------------------------------------------------------------
//
// corSocket - the flags out of the type, then set on the socket; and no SIGPIPE from it
//
int corSocket(int domain, int type, int protocol)
{
  int s = socket(domain, type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC), protocol);

  if (s < 0)
    return s;

  flagsSet(s, (type & SOCK_CLOEXEC) != 0, (type & SOCK_NONBLOCK) != 0);

  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));

  return s;
}



// -----------------------------------------------------------------------------
//
// corCloseRange - after a fork, before an exec: every fd in [first, last] closed (or marked close-on-exec)
//
int corCloseRange(unsigned int first, unsigned int last, int flags)
{
  long max = sysconf(_SC_OPEN_MAX);

  if ((max <= 0) || (max > 65536))
    max = 65536;
  if (last >= (unsigned int) max)
    last = (unsigned int) max - 1;

  for (unsigned int fd = first; fd <= last; fd++)
  {
    if (flags & CLOSE_RANGE_CLOEXEC)
      fcntl((int) fd, F_SETFD, FD_CLOEXEC);
    else
      close((int) fd);
  }

  return 0;
}

#else

typedef int corLinuxNothingHere;        // Linux: the system's own calls (an empty file is not ISO C)

#endif

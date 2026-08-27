/* FlintRTOS lwIP port - freestanding <errno.h> shim (standard error numbers). */
#ifndef FLINT_LWIP_ERRNO_H
#define FLINT_LWIP_ERRNO_H

extern int errno;

#define EPERM            1
#define ENOENT           2
#define EINTR            4
#define EIO              5
#define ENXIO            6
#define EBADF            9
#define EAGAIN          11
#define EWOULDBLOCK     EAGAIN
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EBUSY           16
#define EEXIST          17
#define ENODEV          19
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOSPC          28
#define EPIPE           32
#define ERANGE          34
#define ENAMETOOLONG    36
#define ENOSYS          38
#define ENOTEMPTY       39
#define ELOOP           40
#define ENOMSG          42
#define EPROTO          71
#define EBADMSG         74
#define EOVERFLOW       75
#define EILSEQ          84
#define ENOTSOCK        88
#define EDESTADDRREQ    89
#define EMSGSIZE        90
#define EPROTOTYPE      91
#define ENOPROTOOPT     92
#define EPROTONOSUPPORT 93
#define ESOCKTNOSUPPORT 94
#define EOPNOTSUPP      95
#define EPFNOSUPPORT    96
#define EAFNOSUPPORT    97
#define EADDRINUSE      98
#define EADDRNOTAVAIL   99
#define ENETDOWN       100
#define ENETUNREACH    101
#define ENETRESET      102
#define ECONNABORTED   103
#define ECONNRESET     104
#define ENOBUFS        105
#define EISCONN        106
#define ENOTCONN       107
#define ESHUTDOWN      108
#define ETOOMANYREFS   109
#define ETIMEDOUT      110
#define ECONNREFUSED   111
#define EHOSTDOWN      112
#define EHOSTUNREACH   113
#define EALREADY       114
#define EINPROGRESS    115
#define ESTALE         116
#define EDQUOT         122

#endif /* FLINT_LWIP_ERRNO_H */

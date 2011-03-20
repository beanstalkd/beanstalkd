/* ugly stuff */

#ifndef _t_h_
#define _t_h_

#include <stdint.h>

#define _NEED_FDATASYNC 1
#define _NEED_POSIX_FALLOCATE 1

#if defined(__linux__)
#   include <sys/types.h>
#   undef _NEED_FDATASYNC
#   undef _NEED_POSIX_FALLOCATE
#elif defined(__gnu_linux__)
#   include <sys/types.h>
#   undef _NEED_FDATASYNC
#   undef _NEED_POSIX_FALLOCATE
#elif defined(__APPLE__)
#   include <sys/types.h>
#else
#   error "unknown system type"
#endif

#ifdef _NEED_FDATASYNC
#   define fdatasync fsync
#endif

#undef _NEED_FDATASYNC
#undef _NEED_POSIX_FALLOCATE

typedef unsigned char uchar;
typedef uchar         byte;
typedef unsigned int  uint;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;

#endif /*_t_h_*/

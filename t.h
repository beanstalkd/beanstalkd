/* ugly stuff */

#ifndef _t_h_
#define _t_h_

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

#endif /*_t_h_*/

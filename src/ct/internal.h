// include <stdint.h>

#define TmpDirPat "/tmp/ct.XXXXXX"

typedef int64_t int64;
typedef struct Test Test;
typedef struct Benchmark Benchmark;

struct Test {
    void (*f)(void);
    char *name;
    int  status;
    int  fd;
    int  pid;
    char dir[sizeof TmpDirPat];
};

struct Benchmark {
    void  (*f)(int);
    char  *name;
    int   status;
    int64 dur;
    int64 bytes;
    char  dir[sizeof TmpDirPat];
};

extern Test ctmaintest[];
extern Benchmark ctmainbench[];

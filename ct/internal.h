#define TmpDirPat "/tmp/ct.XXXXXX"

typedef struct Test Test;

struct Test {
    void (*f)(void);
    char *name;
    int  status;
    int  fd;
    int  pid;
    char dir[sizeof TmpDirPat];
};

extern Test ctmain[];

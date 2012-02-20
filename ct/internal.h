typedef struct T T;

struct T {
    void (*f)(void);
    char *name;
    int  status;
    int  fd;
};

extern T ctmain[];

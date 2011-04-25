typedef struct T T;


struct T {
    int status, fd;
    const char *name;
};


void ctreport(T ts[], int n);
void ctrun(T *t, int i, void(*f)(void), const char *name);

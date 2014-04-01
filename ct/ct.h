char *ctdir(void);
void  ctfail(void);
void  ctresettimer(void);
void  ctstarttimer(void);
void  ctstoptimer(void);
void  ctsetbytes(int);
void  ctlogpn(char*, int, char*, ...) __attribute__((format(printf, 3, 4)));
#define ctlog(...) ctlogpn(__FILE__, __LINE__, __VA_ARGS__)
#define assert(x) do if (!(x)) {\
	ctlog("%s", "test: " #x);\
	ctfail();\
} while (0)
#define assertf(x, ...) do if (!(x)) {\
	ctlog("%s", "test: " #x);\
	ctlog(__VA_ARGS__);\
	ctfail();\
} while (0)

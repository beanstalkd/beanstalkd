void ctfail(void);
void ctlogpn(char*, int, char*, ...);
#define ctlog(...) ctlogpn(__FILE__, __LINE__, __VA_ARGS__)
#define assert(x) do if (!(x)) {\
	ctlog("test: " #x);\
	ctfail();\
} while (0)
#define assertf(x, ...) do if (!(x)) {\
	ctlog("test: " #x);\
	ctlog(__VA_ARGS__);\
	ctfail();\
} while (0)

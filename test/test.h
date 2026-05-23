#define ASSERT(x, y) assert(x, y, #y)

void assert(int expected, int actual, char *code);
//#include <stdio.h> > none unsigned type specifier and etc..
extern int printf(char *fmt, ...);
extern int sprintf(char *buf, char *fmt, ...);
// #include <string.h> > none unsigned type specifier and etc..
extern int memcmp(char *p, char *q, int n);
extern int strcmp(char *p, char *q);
void exit(int n);
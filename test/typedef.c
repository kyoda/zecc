#include "test.h"

typedef int myint, myint2;

int main() {
  ASSERT(0, ({ typedef int; 0;}));
  ASSERT(0, ({ typedef t; 0;}));
  ASSERT(4, ({ typedef t; sizeof(t);}));
  ASSERT(0, ({ typedef int t; 0;}));
  ASSERT(0, ({ typedef int t, u; 0;}));
  ASSERT(4, ({ typedef int t; t a; sizeof(a);}));
  ASSERT(4, ({ typedef int t,u; u a; sizeof(a);}));
  ASSERT(8, ({ typedef long long int t; t a; sizeof(a);}));
  ASSERT(1, ({ typedef int t; t a = 1; a;}));
  ASSERT(12, ({ typedef struct { int a, b; char c; } t; t p; sizeof(p);}));
  ASSERT(3, ({ typedef struct { int a, b; char c; } t; t p; p.c = 3; p.c;}));
  ASSERT(4, ({ typedef union { int a, b; char c; } t; t p; sizeof(p);}));

  ASSERT(12, ({ typedef struct x { int a, b; char c; } t; struct x p; sizeof(p);}));
  ASSERT(4, ({ typedef struct x { int a, b; char c; } t; struct x p; p.a = 4; p.a;}));
  ASSERT(4, ({ typedef struct x { int a, b; char c; } t; t p; p.a = 4; p.a;}));

  ASSERT(3, ({ myint2 a = 3; a;}));
  ASSERT(4, ({ typedef myint myint3; myint3 a; sizeof(a);}));
  ASSERT(4, ({ myint typedef myint3; myint3 a; sizeof(a);}));

  printf("OK\n");
  return 0;
}

#include "test.h"

volatile void noreturn_fn(int *restrict x) {
  exit(0);
}

void a1(int arg[restrict 2]) {}
void a2(int arg[restrict static 2]) {}
void a3(int arg[const 2]) {}
void a4(int arg[__restrict static 2]) {}
void a5(int arg[__restrict__ static 2]) {}
void a6(int arg[volatile 2]) {}
void a7(int arg[restrict const volatile 2]) {}

int main() {
  { volatile x; }
  { int volatile x; }
  { volatile int x; }
  { volatile int volatile volatile x; }
  { int volatile * volatile volatile x; }
  { auto ** restrict __restrict __restrict__ const volatile *x; }

  printf("OK\n");
  return 0;
}
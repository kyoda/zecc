#include "test.h"

int main() {
  //ASSERT(0, ({ enum x {}; 0; })); //gcc error
  //ASSERT(4, ({ enum {} x; sizeof(x); })); //gcc error
  //ASSERT(0, ({ enum x; 0; })); //gcc ok 
  ASSERT(0, ({ enum { zero, one, two }; zero; }));
  ASSERT(1, ({ enum { zero, one, two }; one; }));
  ASSERT(2, ({ enum { zero, one, two }; two; }));
  ASSERT(5, ({ enum { five = 5, six, nine = 9 }; five; }));
  ASSERT(6, ({ enum { five = 5, six, nine = 9 }; six; }));
  ASSERT(9, ({ enum { five = 5, six, nine = 9 }; nine; }));
  ASSERT(2, ({ enum { five = 5, six, one = 1, two }; two; }));
  ASSERT(4, ({ enum xtag { a = 1, b, c, d }; enum xtag x; sizeof(x); }));
  ASSERT(4, ({ enum { a, b, c } x; sizeof(x); }));
  ASSERT(4, ({ enum { a, b, c } x; sizeof(a); }));
  ASSERT(-3, ({ enum { a = -4, b }; b; }));

  /*
  ASSERT(0, ({ int a = 0; enum { zero = a }; zero; })); gcc error
  ASSERT(2555, ({ enum { a = 2555 } x; a; })); //gcc error
  */
  printf("OK\n");
  return 0;
}
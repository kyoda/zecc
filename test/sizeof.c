#include "test.h"

int main() {
  ASSERT(1, ({ sizeof(void);}));
  ASSERT(1, ({ sizeof(char);}));
  ASSERT(2, ({ sizeof(short);}));
  ASSERT(4, ({ sizeof(int);}));
  ASSERT(8, ({ sizeof(long);}));
  ASSERT(2, ({ sizeof(short int);}));
  ASSERT(8, ({ sizeof(long int);}));
  ASSERT(8, ({ sizeof(long long);}));
  ASSERT(8, ({ sizeof(long long int);}));
  ASSERT(8, ({ sizeof(char *);}));
  ASSERT(8, ({ sizeof(int *);}));
  ASSERT(8, ({ sizeof(long *);}));
  ASSERT(8, ({ sizeof(int **);}));
  ASSERT(12, ({ sizeof(int [3]);}));
  ASSERT(24, ({ sizeof(int *[3]);}));
  ASSERT(8, ({ sizeof(int (*)[3]);}));
  ASSERT(8, ({ sizeof(int (*)());}));
  ASSERT(8, ({ sizeof(int (*)(char *x));}));
  ASSERT(48, ({ sizeof(int [3][4]);}));
  ASSERT(0, ({ sizeof(struct {}); }));
  ASSERT(16, ({ sizeof(struct {int x; long y;}); }));
  ASSERT(8, ({ sizeof(union {char x; long y;}); }));

  ASSERT(8, ({ sizeof(-3 + (long)3);}));
  ASSERT(8, ({ sizeof(-3 - (long)3);}));
  ASSERT(8, ({ sizeof(-3 * (long)3);}));
  ASSERT(8, ({ sizeof(-3 / (long)3);}));
  ASSERT(8, ({ sizeof((long)-3 + 3);}));
  ASSERT(8, ({ sizeof((long)-3 - 3);}));
  ASSERT(8, ({ sizeof((long)-3 * 3);}));
  ASSERT(8, ({ sizeof((long)-3 / 3);}));

  //ASSERT(4, ({ sizeof(++3);})); //gcc error
  ASSERT(4, ({ int i = 0; sizeof(++i);}));
  ASSERT(8, ({ int a[2]; a[0] = 0; a[1] = 1; int *p = a; sizeof(++p);}));
  ASSERT(4, ({ int i = 0; sizeof(--i);}));
  ASSERT(8, ({ int a[2]; a[0] = 0; a[1] = 1; int *p = a + 1; sizeof(--p);}));

  ASSERT(4, ({ int i = 3; sizeof(i++);}));
  ASSERT(4, ({ int i = 3; sizeof(i--);}));
  //ASSERT(4, ({ sizeof(3++);})); //gcc error
  //ASSERT(4, ({ sizeof(3--);})); //gcc error

  ASSERT(8, ({ sizeof(int (*)[][3]);}));
  
  ASSERT(4, ({ sizeof(struct { int a, b[]; });}));

  ASSERT(1, sizeof(signed char));
  ASSERT(1, sizeof(unsigned char));
  ASSERT(2, sizeof(signed short));
  ASSERT(2, sizeof(unsigned short));
  ASSERT(4, sizeof(signed));
  ASSERT(4, sizeof(unsigned));
  ASSERT(4, sizeof(signed int));
  ASSERT(4, sizeof(unsigned int));
  ASSERT(8, sizeof(signed long));
  ASSERT(8, sizeof(unsigned long));
  ASSERT(8, sizeof(signed long int));
  ASSERT(8, sizeof(unsigned long int));
  ASSERT(8, sizeof(signed long long));
  ASSERT(8, sizeof(unsigned long long));
  ASSERT(8, sizeof(signed long long int));
  ASSERT(8, sizeof(unsigned long long int));

  ASSERT(1, sizeof(char) << 31 >> 31);
  ASSERT(1, sizeof(char) << 63 >> 63);

  ASSERT(4, sizeof(float));
  ASSERT(8, sizeof(double));

  /* error
    ASSERT(8, ({ sizeof(int (*)[3][]);})); //gcc error
    ASSERT(8, ({ sizeof(int [][3]);}));
    ASSERT(4, sizeof(signed signed));
  */

  printf("OK\n");
  return 0;
}
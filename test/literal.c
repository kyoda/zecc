#include "test.h"

int main() {
  ASSERT(97, 'a');
  ASSERT(10, '\n');
  ASSERT(15, '\x000f');
  ASSERT(112, '\x70'); // 0111 0000
  ASSERT(-128, '\x80'); // 1000 0000
  ASSERT(-112, '\x90'); // 1001 0000

  ASSERT(0, 0x0);
  ASSERT(0, 0X0);
  ASSERT(0, 0x000000000000000);
  ASSERT(15, 0xf);
  ASSERT(15, 0xF);
  ASSERT(2, 0b10);
  ASSERT(2, 0B10);
  ASSERT(15, 0b01111);
  ASSERT(511, 0777); // 7*8^2 + 7*8^1 + 7*8^0 = 511

  ASSERT(8, sizeof(0l));
  ASSERT(8, sizeof(0L));
  ASSERT(4, sizeof(0u));
  ASSERT(4, sizeof(0U));
  ASSERT(8, sizeof(0ll));
  ASSERT(8, sizeof(0LL));
  ASSERT(8, sizeof(0Ul));
  ASSERT(8, sizeof(0UL));
  ASSERT(8, sizeof(0uL));
  ASSERT(8, sizeof(0Ul));
  ASSERT(8, sizeof(0LU));
  ASSERT(8, sizeof(0lU));
  ASSERT(8, sizeof(0llU));
  ASSERT(8, sizeof(0ull));
  ASSERT(8, sizeof(0ULL));

  ASSERT(8, sizeof(0x0L));
  ASSERT(8, sizeof(0b0L));
  ASSERT(4, sizeof(2147483647));
  ASSERT(8, sizeof(2147483648));
  ASSERT(-1, 0xffffffffffffffff);
  ASSERT(8, sizeof(0xffffffffffffffff));
  ASSERT(4, sizeof(4294967295U));
  ASSERT(8, sizeof(4294967296U));

  ASSERT(3, -1U>>30);
  ASSERT(3, -1Ul>>62);
  ASSERT(3, -1ull>>62);

  ASSERT(1, 0xffffffffffffffffl>>63);
  ASSERT(1, 0xffffffffffffffffll>>63);

  ASSERT(-1, 18446744073709551615);
  //ASSERT(8, sizeof(18446744073709551615)); // __int128: gcc -> 16
  //ASSERT(-1, 18446744073709551615>>63); // __int128: gcc -> 1

  ASSERT(-1, 0xffffffffffffffff);
  ASSERT(8, sizeof(0xffffffffffffffff));
  ASSERT(1, 0xffffffffffffffff>>63);

  ASSERT(-1, 01777777777777777777777);
  ASSERT(8, sizeof(01777777777777777777777));
  ASSERT(1, 01777777777777777777777>>63);

  ASSERT(-1, 0b1111111111111111111111111111111111111111111111111111111111111111);
  ASSERT(8, sizeof(0b1111111111111111111111111111111111111111111111111111111111111111));
  ASSERT(1, 0b1111111111111111111111111111111111111111111111111111111111111111>>63);

  ASSERT(8, sizeof(2147483648));
  ASSERT(4, sizeof(2147483647));

  ASSERT(8, sizeof(0x1ffffffff));
  ASSERT(4, sizeof(0xffffffff));
  ASSERT(1, 0xffffffff>>31);

  ASSERT(8, sizeof(040000000000));
  ASSERT(4, sizeof(037777777777));
  ASSERT(1, 037777777777>>31);

  ASSERT(8, sizeof(0b111111111111111111111111111111111));
  ASSERT(4, sizeof(0b11111111111111111111111111111111));
  ASSERT(1, 0b11111111111111111111111111111111>>31);

  ASSERT(-1, 1 << 31 >> 31);
  ASSERT(-1, 01 << 31 >> 31);
  ASSERT(-1, 0x1 << 31 >> 31);
  ASSERT(-1, 0b1 << 31 >> 31);

  0.0;
  .0;
  0.;
  .5f;
  1.5;
  1.5e-2F;
  1.5E+2;
  0x10.5p-2;
  5.l;
  5.0L;

  ASSERT(8, sizeof(0.0));
  ASSERT(4, sizeof(.0f));
  ASSERT(4, sizeof(0.4F));
  ASSERT(8, sizeof(.0e3));
  ASSERT(4, sizeof(.0e3f));
  //ASSERT(8, sizeof(2.0l));
  //ASSERT(8, sizeof(5.0L));
  ASSERT(0.0, 0.0f + 0.1);
  ASSERT(1, ({ 0.0 + 0.1 == 0.1; }));


  printf("OK\n");
  return 0;
}
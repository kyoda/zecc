#include "test.h"

int _Alignas(512) g1;
int _Alignas(512) g2;
char g3;
int g4;
long g5;
char g6;

int main() {
  ASSERT(1, _Alignof(char));
  ASSERT(2, _Alignof(short));
  ASSERT(4, _Alignof(int));
  ASSERT(8, _Alignof(long));
  ASSERT(8, _Alignof(long long));
  ASSERT(1, _Alignof(char[4]));
  ASSERT(4, _Alignof(int[5]));
  ASSERT(1, _Alignof(struct {char a; char b;}[1]));
  ASSERT(8, _Alignof(struct {char a; long b;}[3]));

  //gccではスタックに積む順番が逆なためマイナスとなる
  //ASSERT(1, ({ char a, b; &b-&a; }));
  //ASSERT(1, ({ _Alignas(char) char a, b; &b-&a; }));
  //ASSERT(1, ({ _Alignas(char) char _Alignas(char) _Alignas(char) a, b; &b-&a; }));
  //ASSERT(8, ({ _Alignas(long) char a, b; &b-&a; }));
  //ASSERT(32, ({ _Alignas(30+2) char a, b; &b-&a; }));
  //ASSERT(32, ({ _Alignas(32) int *a, *b; (char *)&b-(char *)&a; })); //castではレジスタのbit数を変更するが、alignは変わらない
  //ASSERT(16, ({ struct { _Alignas(16) char a, b; } c; &c.b-&c.a; }));
  //ASSERT(8, ({ struct T { _Alignas(8) char a; }; _Alignof(struct T); }));

  /*
    先頭アドレスが512バイト境界にあることを確認
    (char *)によりポインタ計算がバイト単位で行われる
  */
  ASSERT(0, (long)(char *)&g1 % 512);
  ASSERT(0, (long)(char *)&g2 % 512);
  ASSERT(0, (long)(char *)&g4 % 4);
  ASSERT(0, (long)(char *)&g5 % 8);

  ASSERT(1, ({ char a; _Alignof a;}));
  ASSERT(2, ({ short a; _Alignof a;}));
  ASSERT(4, ({ int a; _Alignof a;}));
  ASSERT(8, ({ long a; _Alignof a;}));
  ASSERT(1, ({ char a; _Alignof(a);}));
  ASSERT(2, ({ short a; _Alignof(a);}));
  ASSERT(4, ({ int a; _Alignof(a);}));
  ASSERT(8, ({ long a; _Alignof(a);}));

  ASSERT(1, _Alignof(char) << 31 >> 31);
  ASSERT(1, _Alignof(char) << 63 >> 63);
  ASSERT(1, ({ char x; _Alignof(x) << 63 >> 63; }));

  printf("OK\n");
  return 0;
}

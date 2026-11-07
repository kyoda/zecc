#include "test.h"

int main() {
  ASSERT(1, ({char a = 1; (char)a;}));
  ASSERT(1, ({char a = 1; (short)a;}));
  ASSERT(1, ({char a = 1; (int)a;}));
  ASSERT(1, ({char a = 1; (long)a;}));
  ASSERT(1, ({short a = 1; (char)a;}));
  ASSERT(1, ({short a = 1; (short)a;}));
  ASSERT(1, ({short a = 1; (int)a;}));
  ASSERT(1, ({short a = 1; (long)a;}));
  ASSERT(1, ({int a = 1; (char)a;}));
  ASSERT(1, ({int a = 1; (short)a;}));
  ASSERT(1, ({int a = 1; (int)a;}));
  ASSERT(1, ({int a = 1; (long)a;}));
  ASSERT(1, ({long a = 1; (char)a;}));
  ASSERT(1, ({long a = 1; (short)a;}));
  ASSERT(1, ({long a = 1; (int)a;}));
  ASSERT(1, ({long a = 1; (long)a;}));

  ASSERT(1, ({char a = 257; (char)a;}));
  ASSERT(1, ({char a = 257; (short)a;}));
  ASSERT(1, ({char a = 257; (int)a;}));
  ASSERT(1, ({char a = 257; (long)a;}));
  ASSERT(1, ({short a = 257; (char)a;}));
  ASSERT(257, ({short a = 257; (short)a;}));
  ASSERT(257, ({short a = 257; (int)a;}));
  ASSERT(257, ({short a = 257; (long)a;}));
  ASSERT(1, ({int a = 257; (char)a;}));
  ASSERT(257, ({int a = 257; (short)a;}));
  ASSERT(257, ({int a = 257; (int)a;}));
  ASSERT(257, ({int a = 257; (long)a;}));
  ASSERT(1, ({long a = 257; (char)a;}));
  ASSERT(257, ({long a = 257; (short)a;}));
  ASSERT(257, ({long a = 257; (int)a;}));
  ASSERT(257, ({long a = 257; (long)a;}));

  ASSERT(1, (char)257);
  ASSERT(1, (short)(1024*64+1));
  ASSERT(1, (int)(1024*1024*1024*4+1));
  ASSERT(1, ({ int a=1; (long)a;}));
  ASSERT(1, (long)1);
  ASSERT(0, (long)&*(int *)0);

  /* 
    int x=512 --> 00000000 00000000 00000010 00000000
    ポインタのchar型にしてデリファレンスするのでload時に1byte目のみの操作(movzx rax, BYTE PTR [rax])となっている
    (add_type()でND_DEREFはlhs->baseのタイプでそのlhsのタイプはND_CASTのタイプ)
  */
  ASSERT(513, ({ int x=512; *(char *)&x=1; x; }));
  ASSERT(5, ({ int x=5; long y=(long)&x; *(int*)y; }));

  (void)1;

  ASSERT(-1, (char)255);
  ASSERT(-1, (signed char)255);
  ASSERT(255, (unsigned char)255);
  ASSERT(-1, (short)65535);
  ASSERT(-1, (signed short)65535);
  ASSERT(65535, (unsigned short)65535);
  ASSERT(-1, (int)0xffffffff);
  ASSERT(-1, (signed int)0xffffffff);
  ASSERT(0xffffffff, (unsigned int)0xffffffff);

  ASSERT(-30, (unsigned int)(-30));  // ??
  ASSERT(1, -1<1);
  ASSERT(0, -1<(unsigned)1);
  ASSERT(254, (char)127 + (char)127);
  ASSERT(-128, (char)128); // 10000000bは、符号ありcharの最小値-128に変換され、signed intの-128(integer promotion)になる
  ASSERT(-128, (int)(char)128);
  ASSERT(-128, (signed int)(char)128);
  ASSERT(-128, (unsigned int)(signed int)(char)128);
  ASSERT(-1, (char)128 + (char)127);
  ASSERT(-1, (char)128 + (unsigned char)127); //integer promotionでcharはintに昇格するため、符号なしcharも符号ありcharもintに昇格する
  ASSERT(255, (int)128 + (unsigned int)127);
  ASSERT(-1, (char)128 + (unsigned int)127); //4294967295uになるが、printfの%dは符号ありintとして出力するため-1になる
  ASSERT(1, ({ (char)128 + (unsigned int)127 == (unsigned int)4294967295; })); //上記の式の確認
  ASSERT(1, ({ (unsigned int)4294967295 == -1; })); //-1は、符号なしintの最大値4294967295に変換されるため等しい
  ASSERT(65534, (short)32767+(short)32767);
  ASSERT(-1, -1>>1);
  ASSERT(1, (unsigned long)-1 == -1);
  ASSERT(1, ((unsigned)-1)>>1 == 2147483647);
  ASSERT(-50, (-100)/2);
  ASSERT(1, ((unsigned)-100)/2 == 2147483598);
  ASSERT(1, ((unsigned long)-100)/2 == 9223372036854775758);
  ASSERT(0, ((long)-1)/(unsigned)100);
  ASSERT(-2, (-100)%7);
  ASSERT(2, ((unsigned)-100)%7);
  ASSERT(6, ((unsigned long)-100)%9);

  // i8 -> i32 (movsx eax, al)
  ASSERT(-1, (int)(signed char)-1);
  ASSERT(-128, (int)(signed char)-128);
  ASSERT(127, (int)(signed char)127);

  // u8 -> i32 (movzx eax, al)
  ASSERT(0, (int)(unsigned char)0);
  ASSERT(127, (int)(unsigned char)127);
  ASSERT(255, (int)(unsigned char)255);

  // i16 -> i32 (movsx eax, ax)
  ASSERT(-1, (int)(short)-1);
  ASSERT(-32768, (int)(short)-32768);
  ASSERT(32767, (int)(short)32767);

  // u16 -> i32 (movzx eax, ax)
  ASSERT(0, (int)(unsigned short)0);
  ASSERT(32767, (int)(unsigned short)32767);
  ASSERT(65535, (int)(unsigned short)65535);

  // i32 -> i64 (movsxd rax, eax)
  ASSERT(-1, (long)(int)-1);
  ASSERT(-2147483647-1, (long)(int)(-2147483647-1));

  // u32 -> i64 (mov eax, eax)
  ASSERT(1, (unsigned long)(unsigned int)-1 == 4294967295UL);

  // Truncation: i32 -> u8
  ASSERT(255, (unsigned char)255);
  ASSERT(0, (unsigned char)256);
  ASSERT(255, (unsigned char)511);

  // Truncation: i32 -> u16
  ASSERT(65535, (unsigned short)65535);
  ASSERT(0, (unsigned short)65536);

  // other
  ASSERT(65535, ({ unsigned short x = 65535; x; }));
  ASSERT(65535, ({ unsigned short x = 65535; (int)x; }));

  ASSERT(-1, ({ typedef short T; T x = 65535; (int)x; }));
  ASSERT(65535, ({ typedef unsigned short T; T x = 65535; (int)x; }));

  ASSERT(65535, (unsigned short)(signed short)-1);
  ASSERT(65535, ({ unsigned short x = (signed short)-1; x; }));
  ASSERT(65535, (int)(unsigned short)(signed short)-1);
  ASSERT(1, ((unsigned short)(signed short)-1) == 65535);

  ASSERT(65535, ({
    unsigned short x;
    unsigned int y = (x = (short)-1);
    x;
  }));

  ASSERT(65535, ({
    unsigned short x;
    unsigned int y = (x = (short)-1);
    y;
  }));

  ASSERT(-1, (signed char)(unsigned short)65535);
  ASSERT(255, (long)(unsigned char)255);
  ASSERT(65535, (long)(unsigned short)65535);
  ASSERT(65535, (long)(unsigned short)(long)-1);
  ASSERT(65535, (unsigned long)(unsigned short)65535);

  ASSERT(255, (unsigned char)255);
  ASSERT(255, (unsigned char)(signed long)255);
  ASSERT(65535, (unsigned short)65535);
  ASSERT(65535, (signed long)(unsigned short)65535);
  ASSERT(65535, (unsigned long)(unsigned short)65535);

  //ASSERT(3, (int)(unsigned char)3.5);
  //ASSERT(3, (int)(unsigned char)3.9);
  //ASSERT(255, (int)(unsigned char)255.9);
  //ASSERT(3, (int)(signed char)3.5);
  //ASSERT(-1, (int)(signed char)-1.5);
  //ASSERT(65535, (int)(unsigned short)65535.9);

  printf("OK\n");
  return 0;
}
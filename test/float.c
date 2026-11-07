#include "test.h"

int main() {
  // i32 -> f32
  ASSERT(0, (float)(int)0);
  ASSERT(35, (float)(int)35);
  ASSERT(-1, (float)(int)-1);
  ASSERT(-128, (float)(signed char)-128);
  ASSERT(127, (float)(signed char)127);
  ASSERT(-32768, (float)(short)-32768);
  ASSERT(32767, (float)(short)32767);
  //ASSERT(1, ({ (float)(int)-2147483648LL == -2147483648.0f; }));
  //ASSERT(1, ({ (float)(int)2147483647 == 2147483648.0f; }));

  // u32 -> f32
  ASSERT(0, (float)(unsigned int)0);
  ASSERT(35, (float)(unsigned int)35);
  ASSERT(255, (float)(unsigned char)255);
  ASSERT(65535, (float)(unsigned short)65535);
  //ASSERT(1, ({ (float)(unsigned int)2147483648U == 2147483648.0f; }));
  //ASSERT(1, ({ (float)(unsigned int)4294967295U == 4294967296.0f; }));

  // i64 -> f32
  ASSERT(0, (float)(long)0);
  ASSERT(35, (float)(long)35);
  ASSERT(-1, (float)(long)-1);
  //ASSERT(1, ({ (float)(long)2147483648LL == 2147483648.0f; }));
  //ASSERT(1, ({ (float)(long)-2147483648LL == -2147483648.0f; }));
  //ASSERT(1, ({ (float)(long)9223372036854775807LL == 9223372036854775808.0f; }));

  // u64 -> f32
  ASSERT(0, (float)(unsigned long)0);
  ASSERT(35, (float)(unsigned long)35);
  //ASSERT(1, ({ (float)(unsigned long)4294967296UL == 4294967296.0f; }));
  //ASSERT(1, ({ (float)(unsigned long)9223372036854775808UL == 9223372036854775808.0f; }));
  //ASSERT(1, ({ (float)(unsigned long)18446744073709551615UL == 18446744073709551616.0f; }));

  // i32 -> f64
  ASSERT(0, (double)(int)0);
  ASSERT(35, (double)(int)35);
  ASSERT(-1, (double)(int)-1);
  ASSERT(-128, (double)(signed char)-128);
  ASSERT(127, (double)(signed char)127);
  ASSERT(-32768, (double)(short)-32768);
  ASSERT(32767, (double)(short)32767);
  //ASSERT(1, ({ (double)(int)-2147483648LL == -2147483648.0; }));
  //ASSERT(1, ({ (double)(int)2147483647 == 2147483647.0; }));

  // u32 -> f64
  ASSERT(0, (double)(unsigned int)0);
  ASSERT(35, (double)(unsigned int)35);
  ASSERT(255, (double)(unsigned char)255);
  ASSERT(65535, (double)(unsigned short)65535);
  //ASSERT(1, ({ (double)(unsigned int)2147483648U == 2147483648.0; }));
  //ASSERT(1, ({ (double)(unsigned int)4294967295U == 4294967295.0; }));

  // i64 -> f64
  ASSERT(0, (double)(long)0);
  ASSERT(35, (double)(long)35);
  ASSERT(-1, (double)(long)-1);
  //ASSERT(1, ({ (double)(long)2147483648LL == 2147483648.0; }));
  //ASSERT(1, ({ (double)(long)-2147483648LL == -2147483648.0; }));
  //ASSERT(1, ({ (double)(long)9223372036854775807LL == 9223372036854775808.0; }));

  // u64 -> f64
  ASSERT(0, (double)(unsigned long)0);
  ASSERT(35, (double)(unsigned long)35);
  //ASSERT(1, ({ (double)(unsigned long)4294967296UL == 4294967296.0; }));
  //ASSERT(1, ({ (double)(unsigned long)9223372036854775808UL == 9223372036854775808.0; }));
  //ASSERT(1, ({ (double)(unsigned long)18446744073709551615UL == 18446744073709551616.0; }));

  // f32 -> i32
  ASSERT(0, (int)(float)0);
  ASSERT(35, (int)(float)35);
  ASSERT(-1, (int)(float)-1);
  ASSERT(3, (int)(float)3.9);
  ASSERT(-3, (int)(float)-3.9);
  ASSERT(127, (signed char)(float)127);
  ASSERT(-128, (signed char)(float)-128);
  ASSERT(32767, (short)(float)32767);
  ASSERT(-32768, (short)(float)-32768);
  //ASSERT(1, ({ (int)(float)-2147483648.0 == -2147483648LL; }));

  // f32 -> u32
  ASSERT(0, (unsigned int)(float)0);
  ASSERT(35, (unsigned int)(float)35);
  ASSERT(3, (unsigned int)(float)3.9);
  ASSERT(255, (unsigned char)(float)255);
  ASSERT(65535, (unsigned short)(float)65535);
  //ASSERT(1, ({ (unsigned int)(float)2147483648.0 == 2147483648U; }));
  //ASSERT(1, ({ (unsigned int)(float)4294967040.0 == 4294967040U; }));

  // f32 -> i64
  ASSERT(0, (long)(float)0);
  ASSERT(35, (long)(float)35);
  ASSERT(-1, (long)(float)-1);
  ASSERT(3, (long)(float)3.9);
  ASSERT(-3, (long)(float)-3.9);
  //ASSERT(1, ({ (long)(float)2147483648.0 == 2147483648LL; }));
  //ASSERT(1, ({ (long)(float)-9223372036854775808.0 == (-9223372036854775807LL-1); }));

  // f32 -> u64
  ASSERT(0, (unsigned long)(float)0);
  ASSERT(35, (unsigned long)(float)35);
  ASSERT(3, (unsigned long)(float)3.9);
  //ASSERT(1, ({ (unsigned long)(float)4294967296.0 == 4294967296UL; }));
  //ASSERT(1, ({ (unsigned long)(float)9223372036854775808.0 == 9223372036854775808UL; }));

  // f64 -> i32
  ASSERT(0, (int)(double)0);
  ASSERT(35, (int)(double)35);
  ASSERT(-1, (int)(double)-1);
  ASSERT(3, (int)(double)3.9);
  ASSERT(-3, (int)(double)-3.9);
  ASSERT(127, (signed char)(double)127);
  ASSERT(-128, (signed char)(double)-128);
  ASSERT(32767, (short)(double)32767);
  ASSERT(-32768, (short)(double)-32768);
  //ASSERT(1, ({ (int)(double)2147483647.0 == 2147483647; }));
  //ASSERT(1, ({ (int)(double)-2147483648.0 == -2147483648LL; }));

  // f64 -> u32
  ASSERT(0, (unsigned int)(double)0);
  ASSERT(35, (unsigned int)(double)35);
  ASSERT(3, (unsigned int)(double)3.9);
  ASSERT(255, (unsigned char)(double)255);
  ASSERT(65535, (unsigned short)(double)65535);
  //ASSERT(1, ({ (unsigned int)(double)2147483648.0 == 2147483648U; }));
  //ASSERT(1, ({ (unsigned int)(double)4294967295.0 == 4294967295U; }));

  // f64 -> i64
  ASSERT(0, (long)(double)0);
  ASSERT(35, (long)(double)35);
  ASSERT(-1, (long)(double)-1);
  ASSERT(3, (long)(double)3.9);
  ASSERT(-3, (long)(double)-3.9);
  //ASSERT(1, ({ (long)(double)2147483648.0 == 2147483648LL; }));
  //ASSERT(1, ({ (long)(double)-9223372036854775808.0 == (-9223372036854775807LL-1); }));

  // f64 -> u64
  ASSERT(0, (unsigned long)(double)0);
  ASSERT(35, (unsigned long)(double)35);
  ASSERT(3, (unsigned long)(double)3.9);
  //ASSERT(1, ({ (unsigned long)(double)4294967296.0 == 4294967296UL; }));
  //ASSERT(1, ({ (unsigned long)(double)9223372036854775808.0 == 9223372036854775808UL; }));

  // f32 -> f64
  ASSERT(-1, (double)(float)-1);
  ASSERT(0, (double)(float)0);
  ASSERT(35, (double)(float)35);
  //ASSERT(1, ({ (double)(float)3.5 == 3.5; }));

  // f64 -> f32
  ASSERT(-1, (float)(double)-1);
  ASSERT(0, (float)(double)0);
  ASSERT(35, (float)(double)35);
  //ASSERT(1, ({ (float)(double)3.5 == 3.5f; }));

  printf("OK\n");
  return 0;
}
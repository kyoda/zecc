#include "test.h"

int main() {
  ASSERT(-1, (float)(int)-1);
  ASSERT(-1, (double)(int)-1);
  ASSERT(-1, (float)(long)-1);
  ASSERT(-1, (double)(long)-1);
  ASSERT(-1, (int)(float)-1);
  ASSERT(-1, (long)(float)-1);
  ASSERT(-1, (int)(double)-1);
  ASSERT(-1, (long)(double)-1);

  ASSERT(-1, (double)(float)-1);
  ASSERT(-1, (float)(double)-1);

  //ASSERT(2147483647, (float)(unsigned int)-1);
  //ASSERT(2147483647, (double)(unsigned int)-1);
  //ASSERT(2147483647, (unsigned int)(float)-1);
  //ASSERT(2147483647, (unsigned int)(double)-1);

  ASSERT(0, (unsigned int)(float)0);
  ASSERT(1, (unsigned int)(float)1);
  ASSERT(3, (unsigned int)(float)3.9); //cvtt（truncate）
  ASSERT(2147483648, (unsigned int)(float)2147483648);



  ASSERT(35, (float)(char)35);
  ASSERT(35, (float)(short)35);
  ASSERT(35, (float)(int)35);
  ASSERT(35, (float)(long)35);
  ASSERT(35, (float)(unsigned char)35);
  ASSERT(35, (float)(unsigned short)35);
  ASSERT(35, (float)(unsigned int)35);
  ASSERT(35, (float)(unsigned long)35);

  ASSERT(35, (double)(char)35);
  ASSERT(35, (double)(short)35);
  ASSERT(35, (double)(int)35);
  ASSERT(35, (double)(long)35);
  ASSERT(35, (double)(unsigned char)35);
  ASSERT(35, (double)(unsigned short)35);
  ASSERT(35, (double)(unsigned int)35);
  ASSERT(35, (double)(unsigned long)35);

  ASSERT(35, (char)(float)35);
  ASSERT(35, (short)(float)35);
  ASSERT(35, (int)(float)35);
  ASSERT(35, (long)(float)35);
  ASSERT(35, (unsigned char)(float)35);
  ASSERT(35, (unsigned short)(float)35);
  ASSERT(35, (unsigned int)(float)35);
  ASSERT(35, (unsigned long)(float)35);

  ASSERT(35, (char)(double)35);
  ASSERT(35, (short)(double)35);
  ASSERT(35, (int)(double)35);
  ASSERT(35, (long)(double)35);
  ASSERT(35, (unsigned char)(double)35);
  ASSERT(35, (unsigned short)(double)35);
  ASSERT(35, (unsigned int)(double)35);
  ASSERT(35, (unsigned long)(double)35);

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

  //ASSERT(3, (int)(unsigned char)3.5);
  //ASSERT(3, (int)(unsigned char)3.9);
  //ASSERT(255, (int)(unsigned char)255.9);
  //ASSERT(3, (int)(signed char)3.5);
  //ASSERT(-1, (int)(signed char)-1.5);
  //ASSERT(65535, (int)(unsigned short)65535.9);

  //ASSERT(-2147483648, (double)(unsigned long)(long)0);


  printf("OK\n");
  return 0;
}
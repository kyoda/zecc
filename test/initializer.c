#include "test.h"

int x;
char gchar1 = 1;
short gshort2 = 2;
int gint3 = 3;
long glong4 = 4;
int gint[3] = {5, 6, 7};
char gchar[] = "Hello World";
struct {int a; char b; char c[2]; } gst = {1, 2, {3, 4}};
struct {char a; int b;} gst2[2] = {{1, 2}, {3, 4}};
int g1 = 1;
int *g2 = &g1;
char g3[] = "abcdef";
char *g4 = g3 + 3;
char *g5 = 3 + g3;
char g6[3] = "abcdefg";
char g7[10] = "abcdefg";
char *g8 = g3 + 3;
int g11 = 1 + 1;
int g12 = 1 ? 2 : 3;
int g13[3] = {1, 2, 3};
long g14 = (long)(long)g13;
char *g15[] = {g3 + 0, g3 + 3, g3 - 3};
union {char a[5]; int b;} gun = {{8, 0, 0, 0, 0}, 9};
struct { struct {int a; int b; } x; int y; } gst3 = { {1, 2}, 3 };
union { union {char a; char b; } x; int y; } gun2 = { {1, 2}, 3 };
char *pgst3 = &gst3.x.a;
char *pgun2 = &gun2.x.a;
union {char a[3]; int b;} gun3 = {1, 0, 0, 0};
struct { struct { int a[3]; } x; } gst4 = { { {1, 2, 3} } };
char *g16 = gst.c + 1;
struct { int a[2]; } gst5[2] = { { {1, 2} }, { {3, 4} } };
struct { int a[2]; } gst6[2] = { { {1, 2} } };
int gint7[2][2] = {{1, 2}, 3, 4};
struct { int a[2]; } gst7[2] = { 1, 2, {3, 4} };
struct { char a[2]; char b[2]; } gst8 = { 1, 2, 3, 4 };
struct { char a, b[2]; } gst9 = { 1, 2, 3 };
union { int a; char b[8]; } gun4[2] = {0x01020304, 0x05060708};
char g17[][4] = {'f', 'o', 'o', 0, 'b', 'a', 'r', 0};

char *g18 = {"foo"};
int g19 = {1};

typedef char T1[];
T1 t1 = {1, 2, 3};
T1 t2 = {1, 2, 3, 4, 5};

typedef struct { char a, b[]; } T2;
T2 t3 = {'f', {'o', 'o', 0}};
T2 t4 = {'f', 'o', 'o', 0};

int main() {
  ASSERT(0, ({ int a = 0; int b = a; b; }));
  ASSERT(0, ({ int a[0] = {}; 0; }));
  ASSERT(0, ({ int a[0] = {}; sizeof(a); }));
  ASSERT(1, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[0][0]; }));
  ASSERT(2, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[0][1]; }));
  ASSERT(3, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[0][2]; }));

  ASSERT(4, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[1][0]; }));
  ASSERT(5, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[1][1]; }));
  ASSERT(6, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[1][2]; }));

  ASSERT(7, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[2][0]; }));
  ASSERT(8, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[2][1]; }));
  ASSERT(9, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}; a[2][2]; }));

  ASSERT(0, ({ int a[2] = {}; a[0]; }));
  ASSERT(0, ({ int a[2] = {1}; a[1]; }));
  ASSERT(0, ({ char a[3] = {}; a[0]; }));
  ASSERT(0, ({ char a[3] = {}; a[1]; }));
  ASSERT(0, ({ char a[3] = {}; a[2]; }));
  ASSERT(3, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}}; a[0][2]; }));
  ASSERT(0, ({ int a[3][3] = {{1, 2, 3}, {4, 5, 6}}; a[2][2]; }));

  //過剰な要素はスキップ
  ASSERT(1, ({ char a[1] = {1, 2, 3}; a[0]; }));
  ASSERT(5, ({ int b = 5; char a[1] = {1, 2, b = 3, 4}; b; }));
  ASSERT(5, ({ int b = 5; char a[1] = {1, 2, ++b, 4}; b; }));
  ASSERT(6, ({ x = 6; char a[1] = {1, 2, x = 3, 4}; x; }));
  ASSERT(3, ({ int b = 5; char a[1] = {b = 3, 4}; a[0]; }));
  ASSERT(3, ({ int b = 5; char a[1] = {b = 3, 4}; b; }));

  ASSERT('a', ({ char a[4] = "abc"; a[0]; }));
  ASSERT('b', ({ char a[4] = "abc"; a[1]; }));
  ASSERT('c', ({ char a[4] = "abc"; a[2]; }));
  ASSERT(0, ({ char a[4] = "abc"; a[3]; }));
  ASSERT('b', ({ char a[4][4] = {"abc", "def"}; a[0][1]; }));
  ASSERT('e', ({ char a[4][4] = {"abc", "def"}; a[1][1]; }));

  ASSERT(2, ({ int a[] = {1, 2}; a[1]; }));
  ASSERT(8, ({ int a[] = {1, 2}; sizeof(a); }));
  ASSERT(3, ({ char a[] = {1, 2, 3}; sizeof(a); }));
  ASSERT(5, ({ char a[] = "abcd"; sizeof(a); }));
  ASSERT(8, ({ char *a = "abcd"; sizeof(a); }));
  ASSERT(12, ({ char a[][6] = {"abc", "defgh"}; sizeof(a); }));
  ASSERT(6, ({ typedef char T[]; T a = "hello"; sizeof(a); }));

  ASSERT(0, ({ struct {int a; int b; int c; } x = {0, 1, 2}; x.a; }));
  ASSERT(1, ({ struct {int a; int b; int c; } x = {0, 1, 2}; x.b; }));
  ASSERT(2, ({ struct {int a; int b; int c; } x = {0, 1, 2}; x.c; }));
  ASSERT(3, ({ struct {int a; int b; int c; } x = {3}; x.a; }));
  ASSERT(0, ({ struct {int a; int b; int c; } x = {3}; x.b; }));
  ASSERT(0, ({ struct {int a; int b; int c; } x = {3}; x.c; }));
  ASSERT(1, ({ struct {int a; int b; } x[2] = {{1, 2}, {3, 4}}; x[0].a; }));
  ASSERT(2, ({ struct {int a; int b; } x[2] = {{1, 2}, {3, 4}}; x[0].b; }));
  ASSERT(3, ({ struct {int a; int b; } x[2] = {{1, 2}, {3, 4}}; x[1].a; }));
  ASSERT(4, ({ struct {int a; int b; } x[2] = {{1, 2}, {3, 4}}; x[1].b; }));
  ASSERT(0, ({ struct {int a; int b; } x[2] = {{3, 4}}; x[1].b; }));
  ASSERT(4, ({ struct {int a; int b; } x[2] = {{3, 4}}; x[0].b; }));
  ASSERT(0, ({ struct {int a; int b; } x[2] = {}; x[0].a; }));
  ASSERT(0, ({ struct {int a; int b; } x[2] = {}; x[0].b; }));
  ASSERT(4, ({ typedef struct {int a, b, c, d, e, f; } T; T x = {1, 2, 3, 4, 5, 6}; x.d; }));
  ASSERT(0, ({ typedef struct {int a, b, c, d, e, f; } T; T x = {1, 2, 3, 4, 5}; T y; T z; z = y = x; z.f;}));
  ASSERT(0, ({ typedef struct {int a, b, c, d, e, f; } T; T x = {1, 2, 3, 4, 5}; T y; T z = y = x; z.f;}));

  ASSERT(0x56, ({ union {int a; char b[5]; } x = { 0x123456 }; x.b[0]; }));
  ASSERT(0x34, ({ union {int a; char b[5]; } x = { 0x123456 }; x.b[1]; }));
  ASSERT(0x12, ({ union {int a; char b[5]; } x = { 0x123456 }; x.b[2]; }));
  ASSERT(0, ({ union {int a; char b[5]; } x = { 0x123456 }; x.b[3]; }));
  ASSERT(0, ({ union {int a; char b[5]; } x = { 0x123456 }; x.b[4]; }));
  ASSERT(0x123456, ({ union {int a; char b[5]; } x = { 0x123456 }; x.a; }));
  ASSERT(0x56, ({ union {int a; char b[5]; } x = { 0x123456, 0x12345678 }; x.b[0]; }));
  ASSERT(0x0, ({ union {int a; char b[4]; } x = { 0x123456, 0x12345678 }; x.b[3]; }));
  ASSERT(0x78563412, ({ union { struct { char a, b, c, d; } e; int f; } x = { { 0x12, 0x34, 0x56, 0x78 } }; x.f; }));
  ASSERT(0x123456, ({ typedef union {int a; char b[5]; } T; T x = { 0x123456 }; T y = x; x.a; }));
  ASSERT(0x123456, ({ typedef union {int a; char b[5]; } T; T x = { 0x123456 }; T y = x; y.a; }));
  ASSERT(0x56, ({ typedef union {int a; char b[5]; } T; T x = { 0x123456 }; T y = x; y.b[0]; }));

  ASSERT(1, gchar1);
  ASSERT(2, gshort2);
  ASSERT(3, gint3);
  ASSERT(4, glong4);
  ASSERT(5, gint[0]);
  ASSERT(6, gint[1]);
  ASSERT(7, gint[2]);
  ASSERT('l', gchar[2]);
  ASSERT('W', gchar[6]);

  ASSERT(1, gst.a);
  ASSERT(2, gst.b);
  ASSERT(3, gst.c[0]);
  ASSERT(4, gst.c[1]);
  ASSERT(1, gst2[0].a);
  ASSERT(2, gst2[0].b);
  ASSERT(3, gst2[1].a);
  ASSERT(4, gst2[1].b);

  ASSERT(2, ({ int a = 1; int b = a + 1; b;}));

  ASSERT(1, *g2);
  ASSERT(0, memcmp(g3, "abcdef", 7));
  ASSERT(0, memcmp(g4, "def", 4));
  ASSERT(0, memcmp(g5, "def", 4));
  ASSERT(0, memcmp(g6, "abc", 3));
  ASSERT(0, memcmp(g7, "abcdefg\0\0", 10));
  ASSERT(0, strcmp(g8 - 3, "abcdef"));
  ASSERT(1, *(int *)g14);
  ASSERT(0, strcmp(g15[0], "abcdef"));
  ASSERT(0, strcmp(g15[1], "def"));
  ASSERT(0, strcmp(g15[2] + 3, "abcdef"));
  ASSERT(8, gun.a[0]);
  ASSERT(0, gun.a[1]);
  ASSERT(0, gun.a[2]);
  ASSERT(0, gun.a[3]);
  ASSERT(0, gun.a[4]);
  ASSERT(8, gun.b);
  ASSERT(1, gst3.x.a);
  ASSERT(1, gun2.x.a);
  ASSERT(1, gun2.x.b);
  ASSERT(1, gun2.y);
  ASSERT(1, *pgst3);
  ASSERT(1, *pgun2);
  ASSERT(1, gun3.a[0]);
  ASSERT(0, gun3.a[1]);
  ASSERT(0, gun3.a[2]);
  ASSERT(1, gun3.b);
  ASSERT(1, gst4.x.a[0]);
  ASSERT(4, *g16);
  ASSERT(2, ({struct { int a[2]; } lst5[2] = { { {1, 2} }, { {3, 4 } } }; lst5[0].a[1];}));
  ASSERT(2, gst5[0].a[1]);
  ASSERT(0, gst6[1].a[1]);
  ASSERT(1, gint7[0][0]);
  ASSERT(2, gint7[0][1]);
  ASSERT(3, gint7[1][0]);
  ASSERT(4, gint7[1][1]);
  ASSERT(1, gst7[0].a[0]);
  ASSERT(2, gst7[0].a[1]);
  ASSERT(3, gst7[1].a[0]);
  ASSERT(4, gst7[1].a[1]);
  ASSERT(1, gst8.a[0]);
  ASSERT(2, gst8.a[1]);
  ASSERT(3, gst8.b[0]);
  ASSERT(4, gst8.b[1]);
  ASSERT(0x01020304, gun4[0].a);
  ASSERT(4, gun4[0].b[0]);
  ASSERT(3, gun4[0].b[1]);
  ASSERT(2, gun4[0].b[2]);
  ASSERT(1, gun4[0].b[3]);
  ASSERT(0x05060708, gun4[1].a);
  ASSERT(8, gun4[1].b[0]);
  ASSERT(7, gun4[1].b[1]);
  ASSERT(6, gun4[1].b[2]);
  ASSERT(5, gun4[1].b[3]);
  ASSERT(0, strcmp(g17[0], "foo"));
  ASSERT(0, strcmp(g17[1], "bar"));

  // local and global variable have a same initializer
  ASSERT(0x01020304, ({union { int a; char b[8]; } lun[2] = {0x01020304, 0x05060708}; lun[0].a;}));
  ASSERT(4, ({union { int a; char b[8]; } lun[2] = {0x01020304, 0x05060708}; lun[0].b[0];}));
  ASSERT(3, ({union { int a; char b[8]; } lun[2] = {0x01020304, 0x05060708}; lun[0].b[1];}));

  ASSERT(0, strcmp(g18, "foo"));
  ASSERT(1, g19);
  ASSERT(1, ({char a[] = {1, 2,}; a[0];}));
  ASSERT(1, ({struct {int a; int b;} x = {1, 2,}; x.a;}));
  ASSERT(1, ({union {int a; int b;} x = {1,}; x.a;}));
  ASSERT(2, ({enum {a, b, c,}; c;}));

  ASSERT(3, sizeof(gst9));

  ASSERT(3, sizeof(t1));
  ASSERT(5, sizeof(t2));
  //ASSERT(4, sizeof(t3)); //gcc error -> 1
  //ASSERT(4, sizeof(t4)); //gcc error -> 1

  ASSERT('f', t3.a);
  ASSERT('f', t4.a);
  ASSERT(0, strcmp(t3.b, "oo"));
  ASSERT(0, strcmp(t4.b, "oo"));


  /* error
    ASSERT(2, ({struct { struct { char a; char b; } x; } lst5 = { {1, 2} }; char *p = lst5.x; *(p+1);})); //gcc compile error
    //C言語でサポートしていない (初期化時にはOK)
    ASSERT(2, ({ char a[2]; a = {'a', 'b'}; sizeof(a);}));
  */

  printf("OK\n");
  return 0;
}

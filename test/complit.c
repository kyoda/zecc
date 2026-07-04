#include "test.h"

typedef struct Tree {
  int val;
  struct Tree *lhs;
  struct Tree *rhs;
} Tree;

Tree *tree = &(Tree){
  0,
  &(Tree){1, 0, 0},
  &(Tree){
    2,
    &(Tree){3, 0, 0},
    &(Tree){4, 0, 0}
  }
};

int add(int a[2]) { return a[0] + a[1]; }

int main() {
  ASSERT(1, (int){1});
  ASSERT('a', (char){'a'});
  ASSERT(7, ({ int *p = (int []){7}; *p; }));
  ASSERT(0, ((int[]){0, 1})[0]);
  ASSERT(1, ((int[]){0, 1})[1]);
  ASSERT(2, ((int[]){0, 1, 2})[2]);
  ASSERT('a', ((struct {char a; int b;}){'a', 1}).a);
  ASSERT(3, ({ int a = 3; (int){a}; }));
  (int){5} = 3;
  ASSERT(1, tree->lhs->val);
  ASSERT(2, tree->rhs->val);
  ASSERT(3, tree->rhs->lhs->val);
  ASSERT(4, tree->rhs->rhs->val);
  ASSERT(1, ((Tree){1, 2, 3}).val);
  ASSERT(2, ((Tree){1, 2, 3}).lhs);
  ASSERT(3, ((Tree){1, 2, 3}).rhs);
  ASSERT(3, add((int[]){1, 2}));

  printf("OK\n");
  return 0;
}

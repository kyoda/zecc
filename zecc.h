#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef struct Type Type;
typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Member Member;
typedef struct Relocation Relocation;

typedef enum {
  TK_IDENT, // Identifiers
  TK_PUNCT, // Punctuators
  TK_KEYWORD, // if, while, etc..
  TK_STR,
  TK_NUM,
  TK_EOF,
} TokenKind;

struct Token {
  TokenKind kind;
  Token *next;
  int64_t val;
  double fval;
  char *loc;
  int len; // not array_len but length of token
  int line; // for .loc directive
  // str
  Type *ty; // Used if TK_NUM or TK_STR
  char *str;
};

typedef enum {
  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /
  ND_MOD, // %
  ND_OR, // |
  ND_XOR, // ^
  ND_AND, // &
  ND_NEG, // unary -
  ND_COND, // ? :
  ND_LOGICALOR, // ||
  ND_LOGICALAND, // &&
  ND_ASSIGN, // =
  ND_COMMA, // ,
  ND_ADDR, // &
  ND_DEREF, // unary *
  ND_NOT, // unary !
  ND_BITNOT, // unary ~
  ND_SHL, // <<
  ND_SHR, // >>
  ND_EQ, // ==
  ND_NEQ, // !=
  ND_LT, // <
  ND_LE, // <=
  ND_FUNC, // function
  ND_VAR, // variable
  ND_NUM, // integer 
  ND_RETURN, // return
  ND_IF, // if
  ND_FOR, // for or while
  ND_DO, // do while
  ND_SWITCH, // switch
  ND_CASE, // case
  ND_GOTO, // goto
  ND_LABEL, // goto labeled statement
  ND_NULL_EXPR, // Do nothing
  ND_EXPR_STMT,
  ND_STMT_EXPR,
  ND_BLOCK, // { ... }
  ND_MEMBER, // struct member
  ND_CAST, // type cast
  ND_MEMZERO // zell-clear a stack variable
} NodeKind;

struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;

  //if or for or while or switch
  Node *cond;
  Node *init;
  Node *inc;
  Node *then;
  Node *els;

  //goto
  Node *goto_next;
  char *label;
  char *unique_label;

  char *break_label; // "break" label
  char *continue_label; // "continue" label

  //switch-case
  Node *case_next;
  Node *default_case;

  //block
  Node *body; 
  Node *next;

  //function call
  char *funcname;
  Type *func_ty;
  Node *args;

  int64_t val;
  double fval;
  Obj *var; // ND_VAR

  Token *token; // for error message
  Type *ty; // int or pointer

  Member *member; // struct member
};

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_FLOAT,
  TY_DOUBLE,
  TY_ENUM,
  TY_PTR,
  TY_ARRAY,
  TY_FUNC,
  TY_STRUCT,
  TY_UNION
} TypeKind;

struct Type {
  TypeKind kind;
  int size; //sizeof
  int align; //stacksize
  bool is_unsigned; // for integer

  Type *base; // pointer, array
  int array_len; // array length

  // function
  Type *return_ty;
  Type *params;
  bool is_variadic; // variadic function
  Type *next; //params

  // declaration
  Token *token;
  Token *name_pos;

  // struct
  Member *members;
  bool is_flexible; // flexible array member
};

// function and variable
struct Obj {
  Obj *next;
  char *name;

  // variable
  Type *ty;
  int len;
  int offset;
  bool is_local;
  int align;

  // function or global variable
  bool is_function;
  bool is_definition;
  bool is_static;

  // global variable
  char *init_data;
  Relocation *rel;

  //function
  Obj *params;
  Node *body;
  Obj *locals;
  Obj *va_area;
  Obj *reg_save_area;
  int stack_size;

};

/*
  Global変数初期化時に利用する
  Local変数の初期化時に利用するdesignatorのような役割
  int g1 = 1;
  int *g2 = &g1 + 2;
         -> label(g1 label address) + addend(num 2) * int size(4)
*/
struct Relocation {
  Relocation *next;
  int offset;
  char *label;
  long addend;
};

struct Member {
  Member *next;
  char *name;
  int align;
  int offset;
  Type *ty;
  int idx;
  Token *token;
};

// scope local, global variable, typedef, enum
typedef struct VarScope VarScope;
struct VarScope {
  char *name;
  Obj *var;
  VarScope *next;
  Type *type_def;
  Type *enum_ty;
  long enum_val;
};

// struct, union, enmu tag
typedef struct TagScope TagScope;
struct TagScope {
  char *name;
  Type *ty;
  TagScope *next;
};

typedef struct Scope Scope;
struct Scope {
  Scope *next;
  VarScope *vars;
  TagScope *tags;
};

typedef struct {
  bool is_typedef;
  bool is_static;
  bool is_extern;
  int align;
} VarAttr;


//type.c
Type *new_type(TypeKind kind, int size, int align);
extern Type *ty_void;
extern Type *ty_bool;
extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_enum;
extern Type *ty_char;
extern Type *ty_uchar;
extern Type *ty_short;
extern Type *ty_ushort;
extern Type *ty_int;
extern Type *ty_uint;
extern Type *ty_long;
extern Type *ty_ulong;
extern Type *ty_struct;
extern Type *ty_va_list;
Type *pointer_to(Type *base);
Type *ty_array(Type *base, int len);
Type *ty_func(Type *base);
Type *cp_type(Type *ty);
char *get_type_name(Type *ty);
bool is_integer(Type *ty);
bool is_flonum(Type *ty);
void add_type(Node *n);

// tokenize.c
Token *tokenize(char *p, char *file);
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
int equal(Token *t, char *key);
Token *skip(Token *t, char *op);

//parse.c
Obj *parse(Token *token);
Node *new_cast(Node *lhs, Type *ty, Token *token);

//codegen.c
void codegen(Obj *prog, FILE *outfile);
int align_to(int n, int align);
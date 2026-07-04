#include "zecc.h"

static Obj *locals;
static Obj *globals;
static Obj *current_fn;
static Scope *scope = &(Scope){};

static Node *gotos;
static Node *labels;
// current goto jump target for break
static char *break_label;
static char *continue_label;

static Node *current_switch;

typedef struct Initializer Initializer;
struct Initializer {
  Initializer *next;
  Type *ty;
  Token *token;
  Node *expr; // initialization expression
  Initializer **children; // array or struct initializer
  bool is_flexible;
};

/*
  配列を初期化する際に、要素ごとにnew_add()をしassign()するが、
  そのnew_add()のindexを保持している
*/
typedef struct InitDesignator InitDesignator;
struct InitDesignator {
  InitDesignator *next;
  int idx;
  Member *member;
  Obj *var;
};

static Type *declspec(Token **rest, Token *token, VarAttr *attr);
static Node *lvar_initializer(Token **rest, Token *token, Obj *var);
static Initializer *initializer(Token **rest, Token *token, Type *ty, Type **new_ty);
static void initializer2(Token **rest, Token *token, Initializer *init);
static int64_t eval(Node *n);
static int64_t eval2(Node *n, char **label);
static int64_t eval_rval(Node *n, char **label);
static Node *declaration(Token **rest, Token *token, Type *basety, VarAttr *attr);
static Type *declarator(Token **rest, Token *token, Type *ty);
static Type *type_suffix(Token **rest, Token *token, Type *ty);
static Type *func_params(Token **rest, Token *token, Type *ty);
static Node *compound_stmt(Token **rest, Token *token);
static Node *stmt(Token **rest, Token *token);
static Node *expr_stmt(Token **rest, Token *token);
static Node *expr(Token **rest, Token *token);
static Node *assign(Token **rest, Token *token);
static int64_t const_expr(Token **rest, Token *token);
static Node *to_assign(Node *lhs, Node *rhs, Token *token);
static Node *conditional(Token **rest, Token *token);
static Node *logicalor(Token **rest, Token *token);
static Node *logicaland(Token **rest, Token *token);
static Node *bitor(Token **rest, Token *token);
static Node *bitxor(Token **rest, Token *token);
static Node *bitand(Token **rest, Token *token);
static Node *equality(Token **rest, Token *token);
static Node *new_add(Node *lhs, Node *rhs, Token *token);
static Node *new_sub(Node *lhs, Node *rhs, Token *token);
static Node *relational(Token **rest, Token *token);
static Node *shift(Token **rest, Token *token);
static Node *add(Token **rest, Token *token);
static Node *mul(Token **rest, Token *token);
static Node *cast(Token **rest, Token *token);
static Type *typename(Token **rest , Token *token);
static Node *unary(Token **rest, Token *token);
static Node *postfix(Token **rest, Token *token);
static Node *primary(Token **rest, Token *token);

static Node *new_node(NodeKind kind, Token *token) {
  Node *n = calloc(1, sizeof(Node));
  n->kind = kind;
  n->token = token;
  return n;
}

static Node *new_node_unary(NodeKind kind, Node *lhs, Token *token) {
  Node *n = new_node(kind, token);
  n->lhs= lhs;
  return n;
}

static Node *new_node_binary(NodeKind kind, Node *lhs, Node *rhs, Token *token) {
  Node *n = new_node(kind, token);
  n->lhs = lhs;
  n->rhs = rhs;
  return n;
}

static Node *new_long(uint64_t val, Token *token) {
  Node *n = new_node(ND_NUM, token);
  n->val = val;
  n->ty = cp_type(ty_long);
  return n;
}

static Node *new_ulong(uint64_t val, Token *token) {
  Node *n = new_node(ND_NUM, token);
  n->val = val;
  n->ty = cp_type(ty_ulong);
  return n;
}

static Node *new_node_num(uint64_t val, Token *token) {
  Node *n = new_node(ND_NUM, token);
  n->val = val;
  return n;
}

static Node *new_node_var(Obj *var, Token *token) {
  Node *n = new_node(ND_VAR, token);
  n->var = var;
  return n;
}

Node *new_cast(Node *lhs, Type *ty, Token *token) {
  add_type(lhs);
  Node *n = new_node_unary(ND_CAST, lhs, token);
  n->ty = ty;
  return n;
}

static Type *find_tag_type(Token *t) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    for (TagScope *ts = sc->tags; ts; ts = ts->next) {
      if (equal(t, ts->name)) {
        return ts->ty;
      }
    }
  }

  return NULL;
}

static void *push_new_tag(Token *token, Type *ty) {
  TagScope *tag = calloc(1, sizeof(TagScope));
  tag->name = strndup(token->loc, token->len);
  tag->ty = ty;
  tag->next = scope->tags;
  scope->tags = tag;

  return NULL;
}

static VarScope *find_var(Token *t) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    for (VarScope *vs = sc->vars; vs; vs = vs->next) {
      if (equal(t, vs->name)) {
        return vs;
      }
    }
  }

  return NULL;
}

/*
  同じスコープでの再定義はエラー
*/
static VarScope *find_var_in_current_scope(Token *t) {
  for (VarScope *vs = scope->vars; vs; vs = vs->next) {
    if (equal(t, vs->name)) {
      return vs;
    }
  }

  return NULL;
}

//search only global variable
static Obj *find_var_by_name(char *name) {
  for (Obj *var = globals; var; var = var->next) {
    if (strlen(name) == strlen(var->name)
      && strncmp(name, var->name, strlen(name)) == 0) {
      return var;
    }
  }

  return NULL;
}

static VarScope *push_scope(char *name) {
  VarScope *v = calloc(1, sizeof(VarScope));
  v->name = name;
  v->next = scope->vars;
  scope->vars = v;

  return v;
}

static Type *cp_struct_type(Type *ty) {
  ty = cp_type(ty);

  Member head = {};
  Member *cur = &head;
  for (Member *m = ty->members; m; m = m->next) {
    Member *mem = calloc(1, sizeof(Member));
    *mem = *m;
    cur = cur->next = mem;
  }

  ty->members = head.next;
  
  return ty;
}

static Initializer *new_initializer(Type *ty, bool is_flexible) {
  Initializer *init = calloc(1, sizeof(Initializer));
  init->ty = ty;

  if (ty->kind == TY_ARRAY) {
    //if (is_flexible && ty->size == 0) {
    //  init->is_flexible = true;
    //  return init;
    //}

    if (is_flexible && ty->size < 0) {
      init->is_flexible = true;
      return init;
    }

    init->children = calloc(ty->array_len, sizeof(Initializer *));
    for (int i = 0; i < ty->array_len; i++) {
      init->children[i] = new_initializer(ty->base, false);
    }
    return init;
  }

  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    int len = 0;
    for (Member *mem  = ty->members; mem; mem = mem->next) {
      len++;
    }

    init->children = calloc(len, sizeof(Initializer *));

    for (Member *mem = ty->members; mem; mem = mem->next) {
      /*
        flexible array member (structの最後のメンバーが配列の場合)
        再帰的にnew_initializer()を呼び出すと通常のarray処理となりsizeが0の処理はない
          //init->children[mem->idx] = new_initializer(mem->ty, true);
        new_initializer()のarray側に処理を書くと、flexible array member処理固有のものとならない(最後のメンバーかどうかわからない)ためここで処理する
      */
      if (is_flexible && ty->is_flexible && !mem->next) {
        Initializer *child = calloc(1, sizeof(Initializer));
        child->ty = mem->ty;
        child->is_flexible = true;
        init->children[mem->idx] = child;
      } else {
        init->children[mem->idx] = new_initializer(mem->ty, false);
      }
    }

    return init;
  }

  return init;
}

static Obj *new_var(char *name, Type *ty) {
  Obj *var;
  var = calloc(1, sizeof(Obj));
  var->name = name;
  var->len = strlen(name);
  var->ty = ty;
  var->align = ty->align;

  push_scope(name)->var = var;

  return var;
}

/*
  新しい変数をリストの先頭に追加
  new --> ~~~ -> second -> first
  先頭アドレスはlocals

  例:
  int exp(int a, int b, int c) {
     int d, e, f;
     return a + b + c + d + e + f;
  };

  リストは下記となる
  f -> e -> d -> a -> b -> c
  params部分は、順番が逆としている
  
*/
static Obj *new_lvar(char *name, Type *ty) {
  Obj *lvar = new_var(name, ty);
  lvar->is_local = true;
  lvar->next = locals;
  locals = lvar;

  return lvar;
}

static Obj *new_gvar(char *name, Type *ty) {
  Obj *gvar = new_var(name, ty);
  gvar->next = globals;
  gvar->is_static = true;
  gvar->is_definition = true;
  globals = gvar;

  return gvar;
}

static char *new_unique_name() {
  static int id = 0;
  char *buf = calloc(1, 20);
  sprintf(buf, ".L..%d", id++);
  return buf;
}

static Obj *new_string_literal(char *str, Type *ty) {
  Obj *gvar = new_gvar(new_unique_name(), ty);
  gvar->init_data = str;

  return gvar;
}

/*
  scopeの先頭に新しいscopeを追加
  一番古いscopeは、global
*/
static void enter_scope() {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->next = scope;
  scope = sc;
}

/*
  変数検索では、block scopeと親のblockを探索する。
  blockを抜けた後は、利用しないためpointerを保持していない 。
*/
static void leave_scope() {
 scope = scope->next; 
}

static Type *find_typedef(Token *t) {
  //if (t->kind != TK_IDENT) {
  //  error_at(t->loc, "%s", "expected an typedef identifier");
  //}

  VarScope *vs = find_var(t);
  if (vs) {
    return vs->type_def;
  }

  return NULL;
}

static Member *find_member(Type *ty, Token *token) {
  for (Member *m = ty->members; m; m = m->next) {
    if (equal(token, m->name)) {
      return m;
    }
  }

  error_at(token->loc, "no member: %.*s", token->len, token->loc);

  return NULL; // never reach here
}

static char *get_ident_name(Token *t) {
  if (t->kind != TK_IDENT) {
    error_at(t->loc, "%s", "expected an identifier");
  }
    
  return strndup(t->loc, t->len);
}

bool consume(Token **rest, Token *token, char *op) {
  if (token->kind == TK_PUNCT && equal(token, op)) {
    *rest = token->next;
    return true;
  }

  return false;
}

bool expect(Token **rest, Token *token, char *op) {
  if (token->kind == TK_PUNCT && equal(token, op)) {
    *rest = token->next;
  } else {
    error_at(token->loc, "no op: %s", op);
    exit(1);
  }

  return false; // never reach here
}

bool expect_ident(Token **rest, Token *token) {
  if (token->kind == TK_IDENT) {
    *rest = token->next;
  } else {
    error_at(token->loc, "%s", "expect TK_IDENT");
  }

  return false; // never reach here
}

bool at_eof(Token *token) {
  return token->kind == TK_EOF;
}

bool is_end(Token *token) {
  return equal(token, "}") || (equal(token, ",") && equal(token->next, "}"));
}

bool consume_end(Token **rest, Token *token) {
  if (equal(token, "}")) {
    *rest = token->next;
    return true;
  }

  if (equal(token, ",") && equal(token->next, "}")) {
    *rest = token->next->next;
    return true;
  }

  return false;
}

// ND_ASSIGNの左辺の準備
static Node *init_desg_expr(InitDesignator *desg, Token *token) {
  if (desg->var) {
    return new_node_var(desg->var, token);
  }

  /*
                          ND_ASSIGN  -------> create_lvar_init()
                             |
                          |        |
    init_desg_expr() <-- ND_MEMBER  ND_EXPR
                           |
    init_desg_expr() <--  ND_DEREF
                           |
                           new_add()
                           |        |
                        ND_VAR    desg->idx
  */
  if (desg->member) {
    Node *lhs = init_desg_expr(desg->next, token);
    Node *n = new_node_unary(ND_MEMBER, lhs, token);
    n->member = desg->member;
    return n;
  }

  /*
    変数の頭からindex分進める
                              ND_ASSIGN  -------> create_lvar_init()
                              |        |
    init_desg_expr() <--  ND_DEREF    ND_EXPR
                              |
                            ND_ADD
                            |       |
    init_desg_expr() <-- ND_DEREF   desg->idx
                          |
                         ND_ADD
                          |     |
                      ND_VAR   desg->idx
  */
  Node *lhs = init_desg_expr(desg->next, token);
  Node *rhs = new_node_num(desg->idx, token);

  // postfixのarrayの場合の値代入の構文木と同じ
  return new_node_unary(ND_DEREF, new_add(lhs, rhs, token), token);
}

/*
  配列の初期化
  int a[2][3] = {{1, 2, 3}, {4, 5, 6}};
  の場合、
  a[0][0] = 1
  a[0][1] = 2
  a[0][2] = 3
  a[1][0] = 4
  a[1][1] = 5
  a[1][2] = 6
  をそのまま処理する。
  例えば、a[1][2] = 6の場合、
  init_desg_exprでa[1]を取得し、ポインタを4の位置に進める
  次のinit_desg_exprでa[1][2]を取得し、create_lvar_initでexprの結果として6を代入する

*/
static Node *create_lvar_init(Initializer *init, Type *ty, InitDesignator *desg, Token *token) {
  if (ty->kind == TY_ARRAY) {
    //空の配列（次のforループで何もしない）の場合、初期化は不要
    Node *node = new_node(ND_NULL_EXPR, token);

    for (int i = 0; i < ty->array_len; i++) {
      InitDesignator desg2 = {desg, i};
      Node *rhs = create_lvar_init(init->children[i], ty->base, &desg2, token);
      node = new_node_binary(ND_COMMA, node, rhs, token);
    }

    return node;
  }

  if (ty->kind == TY_STRUCT && !init->expr) {
    //空の配列（次のforループで何もしない）の場合、初期化は不要
    Node *node = new_node(ND_NULL_EXPR, token);

    
    for (Member *mem = ty->members; mem; mem = mem->next) {
      // 2番目の引数は、arrayでのみ使用するためここでは常に0とする。childrenにはmemberのidxを使う。
      InitDesignator desg2 = {desg, 0, mem};
      Node *rhs = create_lvar_init(init->children[mem->idx], mem->ty, &desg2, token);
      node = new_node_binary(ND_COMMA, node, rhs, token);
    }

    return node;
  }

  if (ty->kind == TY_UNION && !init->expr) {
      InitDesignator desg2 = {desg, 0, ty->members};
      return create_lvar_init(init->children[0], ty->members->ty, &desg2, token);
  }

  /*
    初期化するポインタを取得
    int a[3][2]
    a[0][0]: desg {next, 0, NULL} -> desg {next, 0, NULL} -> desg {NULL, 0, var}
    a[0][1]: desg {next, 1, NULL} -> desg {next, 0, NULL} -> desg {NULL, 0, var}
    a[1][0]: desg {next, 0, NULL} -> desg {next, 1, NULL} -> desg {NULL, 0, var}
    a[1][1]: desg {next, 1, NULL} -> desg {next, 1, NULL} -> desg {NULL, 0, var}
    a[2][0]: desg {next, 0, NULL} -> desg {next, 2, NULL} -> desg {NULL, 0, var}
    a[2][1]: desg {next, 1, NULL} -> desg {next, 2, NULL} -> desg {NULL, 0, var}

    例えばa[2][1]の場合、リストの最後のvarから遡ってnew_add()でpointerを2進め, 次にnew_add()でpointerを1進めるnodeを作成する
  */

  //exprがない場合はスキップ(0で初期化済み)
  if (!init->expr) {
    return new_node(ND_NULL_EXPR, token);
  } 

  Node *lhs = init_desg_expr(desg, token);
  Node *rhs = init->expr;
  return new_node_binary(ND_ASSIGN, lhs, rhs, token);
}

Token *skip_excess_elements(Token *token) {
  if (equal(token, "{")) {
    token = skip_excess_elements(token->next);
    expect(&token, token, "}");

    return token;
  }

  //下記のassign()はASTには反映しない。tokenを進めるのみ。
  assign(&token, token);
  return token;
}

static int count_array_elements(Token *token, Type *ty) {
  Initializer *dummy = new_initializer(ty, false);

  int i = 0;
  for (; !is_end(token); i++) {
    if (i > 0) {
      expect(&token, token, ",");
    }
    initializer2(&token, token, dummy);
  }

  return i;
}

/*
  string-initializer ::= string-literal
*/
static void string_initializer(Token **rest, Token *token, Initializer *init) {
  if (init->is_flexible) {
    /*
      "abc"の場合
      token->lenは、5
      token->ty->array_lenは、4
    */
    *init = *new_initializer(ty_array(init->ty->base, token->ty->array_len), false);
  }

  int len = MIN(token->len, init->ty->array_len);

  for (int i = 0; i < len; i++) {
    init->children[i]->expr = new_node_num(token->str[i], token);
  }

  *rest = token->next;
}

/*
  array-initializer1 ::= "{" initializer ("," initializer)* "}"
*/
static void array_initializer1(Token **rest, Token *token, Initializer *init) {
  expect(&token, token, "{");

  if (init->is_flexible) {
    int len = count_array_elements(token, init->ty->base);
    *init = *new_initializer(ty_array(init->ty->base, len), false);
  }

  for (int i = 0; !consume_end(&token, token); i++) {
    if (i > 0) {
      expect(&token, token, ",");
    }

    if (i < init->ty->array_len) {
      initializer2(&token, token, init->children[i]);
    } else {
      // a[1] = {1, 2, 3}のように、配列の要素数を超える初期化の場合はスキップ
      token = skip_excess_elements(token);
    }

  }

  *rest = token;
}

/*
  array-initializer2 ::= initializer ("," initializer)*
*/
static void array_initializer2(Token **rest, Token *token, Initializer *init) {
  if (init->is_flexible) {
    int len = count_array_elements(token, init->ty->base);
    *init = *new_initializer(ty_array(init->ty->base, len), false);
  }

  for (int i = 0; i < init->ty->array_len && !is_end(token); i++) {
    if (i > 0) {
      expect(&token, token, ",");
    }

    initializer2(&token, token, init->children[i]);
  }

  *rest = token;
}

/*
  struct-initializer1 ::= "{" initializer ("," initializer)* "}"
*/
static void struct_initializer1(Token **rest, Token *token, Initializer *init) {
  expect(&token, token, "{");

  Member *mem = init->ty->members;
  while (!consume_end(rest, token)) {
    if (mem != init->ty->members) {
      expect(&token, token, ",");
    }

    if (mem) {
      initializer2(&token, token, init->children[mem->idx]);
      mem = mem->next;
    } else {
      token = skip_excess_elements(token);
    }
  }
}

/*
  struct-initializer2 ::= initializer ("," initializer)*
*/
static void struct_initializer2(Token **rest, Token *token, Initializer *init) {
  bool first = true;

  for (Member *mem = init->ty->members; mem && !is_end(token); mem = mem->next) {
    if (!first) {
      expect(&token, token, ",");
    }
    first = false;
    initializer2(&token, token, init->children[mem->idx]);
  }

  *rest = token;
}

/*
  union-initializer ::= "{" initializer ("," dumy-initializer)* "}"
                      | initializer
*/
static void union_initializer(Token **rest, Token *token, Initializer *init) {
  // unionの場合は、最初の要素のみ初期化する
  bool first = true;
  Initializer *dummy = new_initializer(init->ty, false);

  if (consume(&token, token, "{")) {
    // "{"がある場合はその中はUNIONメンバーとしてみる。ただし、初期化は最初の要素のみ。
    for (Member *mem = init->ty->members; mem && !is_end(token); mem = mem->next) {
      if (first) {
        initializer2(&token, token, init->children[0]);
        first = false;
      } else {
        expect(&token, token, ",");
        initializer2(&token, token, dummy);
      }
    }

    consume(&token, token, ",");
    expect(&token, token, "}");
  } else {
    /*
      type union T; T y; T x = y;
      yというunion Tの変数を初期化式で代入するような場合の処理
    */
    Node *expr = assign(rest, token);
    add_type(expr);
    if (expr->ty->kind == TY_UNION) {
      init->expr = expr;
      return;
    }

    /*
      "{"がない場合も最初の要素をUNIONメンバーとみなし初期化するが、","が続いた場合の値は、UNIONメンバーとしてみない。
      次のtoken処理を行う

      e.g. union T {int a; int b;} x[2] = {1, 2};
      この場合、x[0].a = 1, x[1].a = 2となる
    */
    initializer2(&token, token, init->children[0]);
  }

  *rest = token;
}

/*
  initializer ::= string-initializer
                | array-initializer
                | struct-initializer
                | union-initializer
                | assign
*/
static void initializer2(Token **rest, Token *token, Initializer *init) {
  if (init->ty->kind == TY_ARRAY && token->kind == TK_STR) {
    string_initializer(rest, token, init);
    return;
  }

  if (init->ty->kind == TY_ARRAY) {
    if (equal(token, "{")) {
      array_initializer1(rest, token, init);
    } else {
      array_initializer2(rest, token, init);
    }
    return;
  }

  if (init->ty->kind == TY_STRUCT) {
    if (equal(token, "{")) {
      struct_initializer1(rest, token, init);
      return;
    }

    /*
      type struct T; T y; T x = y;
      yというstruct Tの変数を初期化式で代入するような場合の処理
    */
    Node *expr = assign(rest, token);
    add_type(expr);
    if (expr->ty->kind == TY_STRUCT) {
      init->expr = expr;
      return;
    }

    // {} で囲まれていない場合のstructの初期化
    struct_initializer2(rest, token, init);
    return;
  }

  if (init->ty->kind == TY_UNION) {
    union_initializer(rest, token, init);
    return;
  }


  /*
    {}で囲まれている場合の初期化
    e.g. 
      char *p = {"abc"};
      int p = {1};
  */
  if (consume(&token, token, "{")) {
    initializer2(&token, token, init);
    expect(&token, token, "}");
    *rest = token;
    return;
  }

  /*
    上記のarray_initializer()内のforループで配列のそれぞれの要素に対して再帰的に初期化を行う
    配列でない場合でもそのままassignが成り立つ
  */
  init->expr = assign(rest, token);
}

static Initializer *initializer(Token **rest, Token *token, Type *ty, Type **new_ty) {
  Initializer *init = new_initializer(ty, true);
  // initializer2()で再帰処理を行う
  initializer2(rest, token, init);

  // flexible array memberの確定したsizeを更新
  if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) && ty->is_flexible) {
    /*
      initializerに組み込んだtyの値を書き換えてしまう可能性があるためcpしている:w
    */
    ty = cp_struct_type(ty);
    Member *mem = ty->members;
    while (mem->next) {
      mem = mem->next;
    }

    // 元のtypeリストはarray_lenサイズが更新されていないため、更新しているinit側のtypeを渡している
    mem->ty = init->children[mem->idx]->ty;
    //structのtypeサイズにflexible array sizeで確定したサイズを加算
    ty->size += init->children[mem->idx]->ty->size;

    *new_ty = ty;

    return init;
  }

  *new_ty = init->ty;

  return init;
}

/*
  declaration内でassign時に呼び出される
  e.g. int a = 1;
             ^
  declaration -> assign -> lvar_initializer -> new_initializer
  new_initializerでtypeリストを処理しながらcalloc()で初期化された構造体を返す
  new_initializer()の第2引数は、is_flexibleで初期化式として初回はtrueで呼び出される
  initializerでtokenリストを処理しながら代入するnodeを格納する
*/
static Node *lvar_initializer(Token **rest, Token *token, Obj *var) {
  Initializer *init = initializer(rest, token, var->ty, &var->ty);
  InitDesignator desg = {NULL, 0, NULL, var};

  //ユーザ指定の初期値を入れる前に、0で初期化
  Node *lhs = new_node(ND_MEMZERO, token);
  lhs->var = var;
  Node *rhs = create_lvar_init(init, var->ty, &desg, token);

  return new_node_binary(ND_COMMA, lhs, rhs, token);
}

static bool is_function(Token *token) {
  if (equal(token, ";")) {
    return false;
  }

  Type dummy = {};
  // functionかどうか先読みを行う
  Type *ty = declarator(&token, token, &dummy);

  return ty->kind == TY_FUNC;
}

static bool is_type(Token *token) {
  char *kw[] = {
    "_Bool", "void", "char", "short", "int", "long", "float", "double",
    "struct", "union", "enum",
    "typedef", "static", "extern", "_Alignas", "signed", "unsigned",
    "const", "volatile", "auto", "register", "restrict",
    "__restrict", "__restrict__", "__Noreturn"
  };
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    if (equal(token, kw[i])) {
      return true;
    }
  }

  return find_typedef(token);
}

static void create_params(Type *param) {
  if (param) {
    create_params(param->next);
    if (!param->token) {
      error_at(param->name_pos->loc, "parameter name ommitted");
    }
    new_lvar(get_ident_name(param->token), param);
  }
}

/*
  labelは後から定義されるため、gotoのjmp先のラベル解決は後で行う
*/
static void resolve_goto_labels() {
  for (Node *x = gotos; x; x = x->goto_next) {
    for (Node *y = labels; y; y = y->goto_next) {
      if (strcmp(x->label, y->label) == 0) {
        x->unique_label = y->unique_label;
        break;
      }
    }

    if (!x->unique_label) {
      error_at(x->token->loc, "undefined label: %s", x->label);
    }
  }
}

/*
  function ::= declspec declarator ( stmt? | ";")
*/
static void *function (Token **rest, Token *token, Type *basety, VarAttr *attr) {
  Type *ty = declarator(&token, token, basety);
  if (!ty->token) {
    error_at(ty->name_pos->loc, "function name omitted");
  }

  bool is_definition = !consume(&token, token, ";");
  Obj *fn = find_var_by_name(get_ident_name(ty->token));

  /*
   * Function symbol state transition:
   *
   *   int foo(int);          // declaration
   *   int foo(int);          // redeclaration
   *   int foo(int x) { ... } // definition
   *
   * The first declaration creates the symbol.
   * Subsequent declarations reuse the existing symbol after type checking.
   * A definition upgrades the existing symbol (is_definition=true).
   * Multiple definitions are an error.
   */
  if (fn) {
    if (!fn->is_function) {
      error_at(ty->token->loc, "%s: redefinition of variable", get_ident_name(ty->token));
    }

    if (fn->is_definition) {
      if (is_definition) {
        error_at(ty->token->loc, "%s: redefinition of function", get_ident_name(ty->token));
      } else {
        *rest = token;
        return NULL;
      }
    } else {
      if (!is_definition) {
        *rest = token;
        return NULL;
      } else {
        fn->is_definition = true;
      }
    }
  } else {
    fn = new_gvar(get_ident_name(ty->token), ty);
    fn->is_function = true;
    fn->is_static = attr->is_static;

    // function declaration
    if (!is_definition) {
      fn->is_definition = false;
      *rest = token;
      return NULL;
    }
  }

  current_fn = fn;
  enter_scope();

  locals = NULL;
  //このタイミングでparamsをlvarにする
  create_params(ty->params);
  fn->params = locals;
  if (ty->is_variadic) {
    /*
      可変長引数の管理用の変数を作成: va_list
      アセンブリ側で下記の構成の値を設定する領域となる
      
      va_listは、ユーザ側のソースコード内で下記を明示して作らずに済むようにコンパイル側であらかじめ変数を用意するという方法を取っている
      struct {
        int gp_offset;
        int fp_offset;
        void *overflow_arg_area;
        void *reg_save_area;
      } __va_elem;
      typedef struct __va_elem __builtin_va_list[1];

      va_listのサイズ(24) 
      reg_save_areaのサイズ(176)(GP: 6 x 8 = 48 + FP: 8 x 16 = 128)

    */
    fn->va_area = new_lvar("__va_area", cp_type(ty_va_list));
    fn->reg_save_area = new_lvar("__reg_save_area", ty_array(cp_type(ty_char), 176));
    fn->reg_save_area->align = 16;
  }

  fn->body = compound_stmt(&token, token);
  add_type(fn->body);
  fn->locals = locals;

  leave_scope();

  resolve_goto_labels();

  *rest = token;

  return NULL;
}

static void write_buf(char *buf, uint64_t val, int size) {
  switch (size) {
    case 1:
      *(uint8_t *)buf = val;
      break;
    case 2:
      *(uint16_t *)buf = val;
      break;
    case 4:
      *(uint32_t *)buf = val;
      break;
    case 8:
      *(uint64_t *)buf = val;
      break;
    default:
      error("invalid type size: %d", size);
  }
}

static Relocation *write_gvar_data(Relocation *cur, Initializer *init, Type *ty, char *buf, int offset) {
  if (ty->kind == TY_ARRAY) {
    int size = ty->base->size;
    for (int i = 0; i < ty->array_len; i++) {
      cur = write_gvar_data(cur, init->children[i], ty->base, buf, offset + size * i);
    }

    return cur;
  }

  if (ty->kind == TY_STRUCT) {
    for (Member *mem = ty->members; mem; mem = mem->next) {
      cur = write_gvar_data(cur, init->children[mem->idx], mem->ty, buf, offset + mem->offset);
    }

    return cur;
  }

  if (ty->kind == TY_UNION) {
    // unionの場合は、最初の要素のみ初期化する
    return write_gvar_data(cur, init->children[0], ty->members->ty, buf, offset);
  }

  if (!init->expr) {
    return cur;
  }

  char *label = NULL;
  /*
    global変数の初期化における値はコンパイル時に確定するためeval()で計算する
    値が確定しないものについてはerrorとする
  */
  int64_t val = eval2(init->expr, &label);

  if (!label) {
    /*
        eval()はint64_tを返すが、write_buf()内ではuint64_tとして扱う
        符号拡張を考えずbyte列をそのまま初期化するため
    */
    write_buf(buf + offset, val, ty->size);
    return cur;
  }

  Relocation *rel = calloc(1, sizeof(Relocation));
  rel->offset = offset;
  rel->label = label;
  rel->addend = val;
  cur->next = rel;

  return cur->next;
}

/*
  Global変数の初期化
  int g1 = 1;
  int g2 = g1 + 1 --> コンパイル時にg1が決まらないため不可
  int g1[] = {1, 2};
  int *g2 = g1 + 1 --> ポインタ計算はOK
*/
static void *gvar_initializer(Token **rest, Token *token, Obj *var) {
  Initializer *init = initializer(rest, token, var->ty, &var->ty);

  Relocation head = {};
  char *buf = calloc(1, var->ty->size);
  write_gvar_data(&head, init, var->ty, buf, 0);
  var->init_data = buf;
  var->rel = head.next;

  return NULL;
}

static void *global_variable (Token **rest, Token *token, Type *basety, VarAttr *attr) {
    int i = 0;
    while (!equal(token, ";")) {
      if (i > 0) {
        expect(&token, token, ",");
      }
      i++;

      Type *ty = declarator(&token, token, basety);
      if (!ty->token) {
        error_at(ty->name_pos->loc, "%s", "variable name omitted");
      }

      Obj *gvar = new_gvar(get_ident_name(ty->token), ty);
      /*
        gvarの作成時は、is_definitionはtrueであるが、
        externの場合は、codegenのemit_dataで処理しないためにis_definitionをfalseにする
      */
      gvar->is_definition = !attr->is_extern;
      gvar->is_static = attr->is_static;

      if (attr->align) {
        gvar->align = attr->align;
      }

      if (consume(&token, token, "=")) {
        gvar_initializer(&token, token, gvar);
      }
    }
  
    expect(&token, token, ";");
    *rest = token;

    return NULL;
}

/*
  __builtin_の初期化
*/
void init_builtin(void) {
  // Add a dummy entry for __builtin_va_list to skip typedef processing
  push_scope("__builtin_va_list")->type_def = cp_type(ty_va_list);
}

/*
  parse_typedef ::= declarator ";"
*/
static void parse_typedef(Token **rest, Token *token, Type *basety) {

  int c = 0;

  // typedef int t;
  while (!consume(&token, token, ";")) {
    if (c > 0) {
      expect(&token, token, ",");
    }

    Type *ty = declarator(&token, token, basety);
    if (!ty->token) {
      error_at(ty->name_pos->loc, "%s", "typedef name omitted");
    }
    push_scope(get_ident_name(ty->token))->type_def = ty;

    c++;
  }
  
  *rest = token;
}

/*
  program ::= (declspec (parse_typedef | function | global_variable))*
*/
Obj *parse(Token *token) {
  globals = NULL;
  init_builtin();

  while (token->kind != TK_EOF) {
    VarAttr attr = {};
    Type *basety = declspec(&token, token, &attr);

    //typedef int t;
    if (attr.is_typedef) {
      parse_typedef(&token, token, basety);  
      continue;
    }

    if (is_function(token)) {
      function(&token, token, basety, &attr);
      continue;
    }

    global_variable(&token, token, basety, &attr);
  }

  return globals;
}

/*
  
  新しいmemberをリストの最後に追加
  old -> new 

  例；
  struct {
    int *a;
    int *b;
  } x;

  b->offset = 8
  a->offset = 0
  xの先頭アドレスはaのアドレスと同じ

  stack
  +------------------------------
  | rbp
  | 7fff ffff ffff ffff 
  +------------------------------
  | [rbp - x->offset + b->offset]
  | 6fff ffff ffff ffff
  +------------------------------
  | [rbp - x->offset + a->offset]
  | 5fff ffff ffff ffff
  +------------------------------
  | ~~~
  +------------------------------
  | rsp
  +------------------------------
  | ~~~
  +------------------------------
  | 0x 0000 0000 0000 0000
  +------------------------------
  
*/
/*
  struct-member ::= (declspec declarator ("," declarator)* ";")*
*/
static void struct_member(Token **rest, Token *token, Type *ty) {
  Member head = {};
  Member *cur = &head;

  int idx = 0;
  while (!equal(token, "}")) {
    VarAttr attr = {};
    Type *basety = declspec(&token, token, &attr);

    int i = 0;
    while (!consume(&token, token, ";")) {
      if (i > 0) {
        expect(&token, token, ",") ;
      }
      i++;

      Member *mem = calloc(1, sizeof(Member));
      mem->ty = declarator(&token, token, basety);
      mem->name = get_ident_name(mem->ty->token);
      mem->idx = idx++;
      mem->align = attr.align ? attr.align : mem->ty->align;
      cur = cur->next = mem;
    }
  }

  /*
    flexible array member (structの最後のmemberがincomplete配列の場合)
    ヘッダー + ペイロード(可変長)のような構造体を作成する際に利用する
    type_suffixで incommplete arrayの場合、sizeは-1としている
  */
  if (cur != &head && cur->ty->kind == TY_ARRAY && cur->ty->array_len < 0) {
    cur->ty = ty_array(cur->ty->base, 0);
    ty->is_flexible = true;
  }

  ty->members = head.next;
  *rest = token;
}

/*
  struct-union-decl ::= "struct" ident? ("{" struct-member "}")?
*/
static Type *struct_union_decl(Token **rest, Token *token) {
  // "struct"
  token = token->next;

  Token *token_tag = NULL;
  if (token->kind == TK_IDENT) {
    //tokenだけ保持しておき、structのtypeを作成した後にtagを追加する
    token_tag = token;
    token = token->next;
  }

  /*
    struct tag x; といったtagによるstructの宣言
    struct tag; も不完全なtagとして扱い、上書き可能
  */
  if (token_tag && !equal(token, "{")) {
    *rest = token;

    Type *ty = find_tag_type(token_tag);
    if (ty) {
      return ty;
    }

    // incomplete struct type for sizeof()
    ty = cp_type(ty_struct);
    ty->size = -1;
    push_new_tag(token_tag, ty);

    return ty;
  }

  expect(&token, token, "{");

  Type *ty = cp_type(ty_struct);
  struct_member(&token, token, ty);
  expect(rest, token, "}");

  if (token_tag) {
    // tagがすでにあれば、typeの中身を上書きする
    for (TagScope *ts = scope->tags; ts; ts = ts->next) {
      if (equal(token_tag, ts->name)) {
        //ts->ty = ty; としてポインタを変更してしまうと、他で参照している場合に問題が発生する
        *ts->ty = *ty;
        return ts->ty;
      }
    }

    push_new_tag(token_tag, ty);
  }

  return ty;
}

/*
  struct-decl ::= struct-union-decl
*/
static Type *struct_decl(Token **rest, Token *token) {
  Type *ty = struct_union_decl(rest, token);
  ty->kind = TY_STRUCT;

  if (ty->size < 0) {
    return ty;
  }

  int offset = 0;
  for (Member *m = ty->members; m; m = m->next) {
    // memberのtypeのalignに合わせてoffsetをalignした後、memberのtype->sizeをoffsetに加算
    offset = align_to(offset, m->align);
    /*
      alignした後にoffsetをmemberのoffsetに格納
      _Alignasの場合、_Alignasで指定されたalignに合わせてoffsetをalignした後にメンバーのoffsetに反映する
    */
    m->offset = offset;
    offset += m->ty->size;

    // structのtypeのalignは、memberのalignの最大値
    if (ty->align < m->align) {
      ty->align = m->align;
    }
  }

  //structのalign(memberのalignの最大値)に合わせてoffsetをalignしそれをsizeとする
  ty->size = align_to(offset, ty->align);

  return ty;
}

/*
  union-decl ::= struct-union-decl
*/
static Type *union_decl(Token **rest, Token *token) {
  Type *ty = struct_union_decl(rest, token);
  ty->kind = TY_UNION;

  if (ty->size < 0) {
    return ty;
  }

  for (Member *m = ty->members; m; m = m->next) {
    // unionのtypeのalignは、memberのalignの最大値
    if (ty->align < m->align) {
      ty->align = m->align;
    }
    // unionのtypeのsizeは、memberのsizeの最大値
    if (ty->size< m->ty->size) {
      ty->size = m->ty->size;
    }
  }

  ty->size = align_to(ty->size, ty->align);

  return ty;
}

/*
  enum-specifier ::= "enum" ident? "{" enum-list? "}"
                   | "enum" ident ("{" enum-list? "}")?
  enmu-list ::= ident ("=" num)? ("," ident ("=" num)?)* 
*/
static Type *enum_specifier(Token **rest, Token *token) {
  Type *ty = cp_type(ty_enum);
  // enum
  token = token->next;

  Token *token_tag = NULL;
  if (token->kind == TK_IDENT) {
    //tokenだけ保持しておき、enumのtypeを作成した後にtagを追加する
    token_tag = token;
    token = token->next;
  }

  // enum tag x; といったtagによるenumの宣言
  if (token_tag && !equal(token, "{")) {
    Type *ty = find_tag_type(token_tag);
    if (!ty) {
      error_at(token_tag->loc, "%s", "undefined enum tag");
    }

    if (ty->kind != TY_ENUM) {
      error_at(token_tag->loc, "%s", "not enum tag");
    }

    *rest = token;
    return ty;
  }


  expect(&token, token, "{");

  //enum-list
  int i = 0;
  // enumの初期値は0から始まる
  uint64_t val = 0;
  while (!is_end(token)) {
    if (i++ > 0) {
      expect(&token, token, ",");
    }

    char *name = get_ident_name(token);
    token = token->next;

    if (equal(token, "=")) {
      token = token->next;
      val = const_expr(&token, token);
    }

    VarScope *vs = push_scope(name);
    vs->enum_ty = ty;
    vs->enum_val = val++;
  }

  consume(&token, token, ",");
  expect(&token, token, "}");

  if (token_tag) {
    push_new_tag(token_tag, ty);
  }

  *rest = token;
  return ty;
}

/*
  declspec ::= ("_Bool" | "void" | "char" | "short" | "int" | "long"
             | "struct-decl" | "union-decl" | "enum-specifier" | "_Alignas" | "signed" | "unsigned"
             | "const" | "volatile" | "auto" | "register" | "restrict" | "__restrict" | "__restrict__" | "__Noreturn"
             | "typedef" | "static" | "extern" | typedef-name)+
*/
static Type *declspec(Token **rest, Token *token, VarAttr *attr) {
  enum {
    VOID = 1 << 0,
    BOOL = 1 << 2,
    CHAR = 1 << 4,
    SHORT = 1 << 6,
    INT = 1 << 8,
    LONG = 1 << 10,
    FLOAT = 1 << 12,
    DOUBLE = 1 << 14,
    OTHER = 1 << 16,
    SIGNED = 1 << 17,
    UNSIGNED = 1 << 18
  };

  Type *ty = cp_type(ty_int);
  int counter = 0; 

  while (is_type(token)) {
    if (equal(token, "typedef") || equal(token, "static") || equal(token, "extern")) {
      if (!attr) {
        error_at(token->loc, "%s", "typedef or static is not allowed");
      }

      if (equal(token, "typedef")) {
        attr->is_typedef = true;
      } else if (equal(token, "static")) {
        attr->is_static = true;
      } else {
        attr->is_extern = true;
      }

      if (attr->is_typedef && (attr->is_static || attr->is_extern)) {
        error_at(token->loc, "%s", "typedef may not be used together with static or extern");
      }

      token = token->next;
      continue;
    }

    // ignore type qualifier
    if (equal(token, "const") || equal(token, "volatile") 
        || equal(token, "auto") || equal(token, "restrict") 
        || equal(token, "__restrict") || equal(token, "__restrict__") 
        || equal(token, "__Noreturn")) {
      token = token->next;
      continue;
    }

    if (equal(token, "_Alignas")) {
      if (!attr) {
        error_at(token->loc, "%s", "_Alignas is not allowed");
      }

      expect(&token, token->next, "(");

      if (is_type(token)) {
        attr->align = typename(&token, token)->align;
      } else {
        attr->align = const_expr(&token, token);
      }

      expect(&token, token, ")");

      continue;
    }
    
    //"struct" "union" "enum" typedef-name
    Type *ty2 = find_typedef(token);
    if (equal(token, "struct") || equal(token, "union") 
        || equal(token, "enum") || ty2) {
      // struct, union, enum, typedefで定義された型は、重複した定義はない
      if (counter) {
        break;
      }

      if (equal(token, "struct")) {
        ty = struct_decl(&token, token);
      } else if (equal(token, "union")) {
        ty = union_decl(&token, token);
      } else if (equal(token, "enum")) {
        ty = enum_specifier(&token, token);
      } else {
        //t x; (typedef int t;)
        ty = ty2;
        token = token->next;
      }

      counter += OTHER;
      continue;
    }

    if (equal(token, "void")) {
      counter += VOID;
    } else if (equal(token, "_Bool")) {
      counter += BOOL;
    } else if (equal(token, "char")) {
      counter += CHAR;
    } else if (equal(token, "short")) {
      counter += SHORT;
    } else if (equal(token, "int")) {
      counter += INT;
    } else if (equal(token, "long")) {
      counter += LONG;
    } else if (equal(token, "float")) {
      counter += FLOAT;
    } else if (equal(token, "double")) {
      counter += DOUBLE;
    } else if (equal(token, "signed")) {
      //signedは、複数回指定不可
      counter += SIGNED;
    } else if (equal(token, "unsigned")) {
      //unsignedは、複数回指定不可
      counter += UNSIGNED;
    } else {
      error_at(token->loc, "%s", "invalid type specifier");
    }

    switch(counter) {
    case VOID:
      ty = cp_type(ty_void);
      break;
    case BOOL:
      ty = cp_type(ty_bool);
      break;
    case CHAR:
    case SIGNED + CHAR:
      ty = cp_type(ty_char);
      break;
    case UNSIGNED + CHAR:
      ty = cp_type(ty_uchar);
      break;
    case SHORT:
    case SHORT + INT:
    case SIGNED + SHORT:
    case SIGNED + SHORT + INT:
      ty = cp_type(ty_short);
      break;
    case UNSIGNED + SHORT:
    case UNSIGNED + SHORT + INT:
      ty = cp_type(ty_ushort);
      break;
    case INT:
    case SIGNED:
    case SIGNED + INT:
      ty = cp_type(ty_int);
      break;
    case UNSIGNED:
    case UNSIGNED + INT:
      ty = cp_type(ty_uint);
      break;
    case LONG:
    case LONG + INT:
    case LONG + LONG:
    case LONG + LONG + INT:
    case SIGNED + LONG:
    case SIGNED + LONG + INT:
    case SIGNED + LONG + LONG:
    case SIGNED + LONG + LONG + INT:
      ty = cp_type(ty_long);
      break;
    case UNSIGNED + LONG:
    case UNSIGNED + LONG + INT:
    case UNSIGNED + LONG + LONG:
    case UNSIGNED + LONG + LONG + INT:
      ty = cp_type(ty_ulong);
      break;
    case FLOAT:
      ty = cp_type(ty_float);
      break;
    case DOUBLE:
      ty = cp_type(ty_double);
      break;
    default:
      error_at(token->loc, "%s", "invalid type specifier");
    }

    token = token->next;
  }


  *rest = token;
  return ty;
}

/*
  declaration ::= (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
*/
static Node *declaration(Token **rest, Token *token, Type *basety, VarAttr *attr) {
    Node head = {};
    Node *cur = &head;
    
    int i = 0;
    while (!equal(token, ";")) {
      if (i > 0) {
        expect(&token, token, ",") ;
      }
      i++;

      Type *ty = declarator(&token, token, basety);

      if (ty->kind == TY_VOID) {
        error_at(token->loc, "%s", "void type variable");
      }

      //if (!ty->token) {
      //  error_at(ty->name_pos->loc, "%s", "function name is omitted");
      //}

      if (attr && attr->is_static) {
        // static local variable
        Obj *gvar = new_gvar(get_ident_name(ty->token), ty);
        push_scope(get_ident_name(ty->token))->var = gvar;

        if (equal(token, "=")) {
          gvar_initializer(&token, token->next, gvar);
        }

        continue;
      }

      VarScope *vs = find_var_in_current_scope(ty->token);
      if (vs) {
        error_at(ty->token->loc, "%s", "defined variable");
      }

      Obj *lvar = new_lvar(get_ident_name(ty->token), ty);

      if (attr && attr->align) {
        lvar->align = attr->align;
      }

      Node *lhs = new_node_var(lvar, token);

      if (equal(token, "=")) {
        Node *expr = lvar_initializer(&token, token->next, lvar);
        cur = cur->next = new_node_unary(ND_EXPR_STMT, expr, token);
      } else {
        cur = cur->next = lhs;
      }

      if (lvar->ty->size < 0) {
        error_at(token->loc, "%s", "incomplete type");
      }

    }

    Node *n = new_node(ND_BLOCK, token);
    n->body = head.next;
    expect(&token, token, ";");
    *rest = token;
    return n;
}

/*
  pointer ::= ("*" (const | volatile | auto | restrict | __restrict | __restrict__ | __Noreturn)*)*
*/
static Type *pointers(Token **rest, Token *token, Type *ty) {
    while (consume(&token, token, "*")) {
      ty = pointer_to(ty);
      while (equal(token, "const") || equal(token, "volatile") 
          || equal(token, "auto") || equal(token, "restrict")
          || equal(token, "__restrict") || equal(token, "__restrict__")
          || equal(token, "__Noreturn")) {
        token = token->next;
      }
    }

    *rest = token;
    return ty;
}

/*
  declarator ::= pointers ( "(" declarator ")" | ident ) type-suffix
*/
static Type *declarator(Token **rest, Token *token, Type *ty) {
    ty = pointers(&token, token, ty);

    if (equal(token, "(")) {
      Token *start = token;
      token = token->next;
      
      Type dummy = {};
      declarator(&token, token, &dummy);
      expect(&token, token, ")");

      /*
        nest内のtypeは、nest外のsuffixのtypeを指すため先読みしておく
        進めたtoken(rest)は使わない
      */
      ty = type_suffix(rest, token, ty);

      /*
        tokenは、"("の次のtoken
      */
      return declarator(&token, start->next, ty);
    }

    //if (token->kind != TK_IDENT) {
    //  error_at(token->loc, "%s", "expect TK_IDENT");
    //}

    /*
      identは、type_suffixの後に取得する
      type_suffix内で、functionかarrayかを判断しtypeを作成

      下記のように引数名の省略もあるためそのtokenのあるなしとエラー出力のためのpositionを保存しておく

       - int func(int a) のような引数名の取得
       - int func(int); のような宣言での引数名省略


    */
    Token *name = NULL;
    Token *name_pos = token;
    if (token->kind == TK_IDENT) {
      name = token;
      token = token->next;
    }

    ty = type_suffix(rest, token, ty);
    //ident取得用
    ty->token = name;
    ty->name_pos = name_pos;

    return ty;
}

/*
  array_dimensions ::= ("static" || "restrict" || "__restrict" || "__restrict__")* const-expr? "]" type-suffix
*/
static Type *array_dimensions(Token **rest, Token *token, Type *ty) {
  //incomplete array
  if (equal(token, "]")) {
    ty = type_suffix(rest, token->next, ty);
    return ty_array(ty, -1);
  }

  // Now, skip array qualifier
  while (equal(token, "static") || equal(token, "const") 
   || equal(token, "volatile") || equal(token, "restrict")
   || equal(token, "__restrict") || equal(token, "__restrict__")) {
    token = token->next;
  }

  int64_t size = const_expr(&token, token);
  expect(&token, token, "]");
  ty = type_suffix(&token, token, ty);
  *rest = token;

  return ty_array(ty, size);
}

/*
  type-suffix ::= "(" func-params ")"
                | array-dimensions
                | ε
*/
static Type *type_suffix(Token **rest, Token *token, Type *ty) {
  if (equal(token, "(")) {
    return func_params(rest, token->next, ty);
  }

  if (equal(token, "[")) {
    return array_dimensions(rest, token->next, ty);
  }

  *rest = token;
  return ty;
}

/*
  func-params ::= "(" "void" | (declspec declarator ("," declspec declarator)* ("," "...")?)? ")"
*/
static Type *func_params(Token **rest, Token *token, Type *ty) {
  if (equal(token, "void") && equal(token->next, ")")) {
    *rest = token->next->next;
    return ty_func(ty);
  }

  Type head = {};
  Type *cur = &head;
  bool is_variadic = false;

  int i = 0;
  while (!equal(token, ")")) {
    if (i > 0) {
      expect(&token, token, ",");
    }
    i++;

    if (equal(token, "...")) {
      is_variadic = true;
      token = token->next;
      break;
    }

    Type *basety = declspec(&token, token, NULL);
    Type *ty2 = declarator(&token, token, basety);

    // func(int a[])
    // a[] -> *a
    if (ty2->kind == TY_ARRAY) {
      Token *ty2_token = ty2->token;
      ty2 = pointer_to(ty2->base);
      ty2->token = ty2_token;
    }

    /*
      paramsのリストは引数指定順
      func(a, b, c, d, e, f)
      a -> b -> c -> d -> e -> f
    */
    ty2->next = NULL;
    cur = cur->next = cp_type(ty2);
  }

  /*
    TYPE_FUNC
    ty->params: Type型でparamごとにnextでのリスト
    ローカル変数生成は、function内でtoken->nameから取得しlocalsに追加する
  */

  ty = ty_func(ty);
  ty->params = head.next;
  ty->is_variadic = is_variadic;
  expect(&token, token, ")");
  *rest = token;

  return ty;
}

/*
  compound-stmt ::= declspec (parse_typedef | function | extern | global_variable)*
*/
static Node *compound_stmt(Token **rest, Token *token) {
  expect(&token, token, "{");

  Node *n = new_node(ND_BLOCK, token);
  
  enter_scope();

  Node head = {};
  Node *cur = &head;
  while (!equal(token, "}")) {
    /*
      type(int, typedef, etc..)
      exclude label ":"
    */
    if (is_type(token) && !equal(token->next, ":")) {
      VarAttr attr = {};
      Type *basety = declspec(&token, token, &attr);

      //typedef int t;
      if (attr.is_typedef) {
        parse_typedef(&token, token, basety);
        continue;
      }

      /*
        function内のfunction宣言
        例: int func(int a);
      */
      if (is_function(token)) {
        function(&token, token, basety, &attr);
        continue;
      }

      if (attr.is_extern) {
        global_variable(&token, token, basety, &attr);
        continue;
      }

      cur = cur->next = declaration(&token, token, basety, &attr); 
    } else {
      cur = cur->next = stmt(&token, token);
    }

    add_type(cur);
  }
  

  expect(&token, token, "}");

  leave_scope();

  n->body = head.next;

  *rest = token;
  return n;
}

/*
  stmt ::= compound-stmt
         | "return" expr? ";"
         | "if" "(" expr ")" stmt ("else" stmt)?
         | "while" "(" expr ")" stmt
         | "do" stmt "while" "(" expr ")" ";"
         | "for" "(" expr? ";" expr? ";" expr? ";"  ")" stmt
         | "switch" "(" expr ")" stmt
         | "case" num ":" stmt
         | "default" ":" stmt
         | "goto" ident ";"
         | "break" ";"
         | "continue" ";"
         | ident ":" stmt
         | expr-stmt
*/
static Node *stmt(Token **rest, Token *token) {
  Node *n;

  if (equal(token, "{")) {
    return compound_stmt(rest, token);
  }

  if (equal(token, "return")) {
    n = new_node(ND_RETURN, token);
    token = token->next;

    if (consume(rest, token, ";")) {
      return n;
    }

    Node *exp = expr(&token, token);
    add_type(exp);

    expect(&token, token, ";");
    n->lhs = new_cast(exp, current_fn->ty->return_ty, token);

    *rest = token;
    return n;
  }

  if (equal(token, "if")) {
    n = new_node(ND_IF, token);
    token = token->next;
    expect(&token, token, "(");
    n->cond = expr(&token, token);
    expect(&token, token, ")");
    n->then = stmt(&token, token);

    if (equal(token, "else")) {
      token = token->next;
      n->els = stmt(&token, token);
    }

    *rest = token;
    return n;
  }

  if (equal(token, "while")) {
    n = new_node(ND_FOR, token);
    token = token->next;
    expect(&token, token, "(");
    n->cond = expr(&token, token);
    expect(&token, token, ")");

    //1個前のbreak, continueのlabelを保持
    char *tmp_break = break_label;
    char *tmp_continue = continue_label;
    //n->then内でbreak, continueがあった場合のlabelを設定する
    break_label = n->break_label = new_unique_name();
    continue_label = n->continue_label = new_unique_name();

    n->then = stmt(&token, token);

    //1個前のbreak, continueのlabelを復元
    break_label = tmp_break;
    continue_label = tmp_continue;

    *rest = token;
    return n;
  }

  if (equal(token, "do")) {
    n = new_node(ND_DO, token);
    token = token->next;

    //1個前のbreak, continueのlabelを保持
    char *tmp_break = break_label;
    char *tmp_continue = continue_label;
    //n->then内でbreak, continueがあった場合のlabelを設定する
    break_label = n->break_label = new_unique_name();
    continue_label = n->continue_label = new_unique_name();

    n->then = stmt(&token, token);

    //1個前のbreak, continueのlabelを復元
    break_label = tmp_break;
    continue_label = tmp_continue;
    
    token = skip(token, "while");
    expect(&token, token, "(");
    n->cond = expr(&token, token);
    expect(&token, token, ")");

    *rest = token;
    return n;
  }

  if (equal(token, "for")) {
    n = new_node(ND_FOR, token);
    token = token->next;
    expect(&token, token, "(");

    enter_scope();

    if (is_type(token)) {
      Type *basety = declspec(&token, token, NULL);
      n->init = declaration(&token, token, basety, NULL);
    } else {
      n->init = expr_stmt(&token, token);
    }

    if(!equal(token, ";")) {
      n->cond = expr(&token, token); 
    }
    expect(&token, token, ";");

    if(!equal(token, ")")) {
      n->inc = expr(&token, token); 
    }
    expect(&token, token, ")");

    //1個前のbreak, continueのlabelを保持
    char *tmp_break = break_label;
    char *tmp_continue = continue_label;
    //n->then内でbreak, continueがあった場合のlabelを設定する
    break_label = n->break_label = new_unique_name();
    continue_label = n->continue_label = new_unique_name();

    n->then = stmt(&token, token);

    leave_scope();

    //1個前のbreak, continueのlabelを復元
    break_label = tmp_break;
    continue_label = tmp_continue;

    *rest = token;
    return n;
  }

  if (equal(token, "switch")) {
    n = new_node(ND_SWITCH, token);
    token = token->next;
    expect(&token, token, "(");
    n->cond = expr(&token, token);
    expect(&token, token, ")");

    Node *tmp_switch = current_switch;
    current_switch = n;

    char *tmp_break = break_label;
    break_label = n->break_label = new_unique_name();

    n->then = stmt(&token, token);

    current_switch = tmp_switch;
    break_label = tmp_break;

    *rest = token;
    return n;
  }

  if (equal(token, "case")) {
    if (!current_switch) {
      error_at(token->loc, "%s", "stray case");
    }

    n = new_node(ND_CASE, token);
    token = token->next;
    int64_t val = const_expr(&token, token);
    expect(&token, token, ":");

    n->unique_label = new_unique_name();
    n->lhs = stmt(&token, token);
    n->val = val;
    n->case_next = current_switch->case_next;
    current_switch->case_next = n;

    *rest = token;
    return n;
  }

  if (equal(token, "default")) {
    if (!current_switch) {
      error_at(token->loc, "%s", "stray default");
    }
    n = new_node(ND_CASE, token);
    token = token->next;
    expect(&token, token, ":");

    n->unique_label = new_unique_name();
    n->lhs = stmt(&token, token);

    current_switch->default_case = n;

    *rest = token;
    return n;
  }

  if (equal(token, "goto")) {
    n = new_node(ND_GOTO, token);
    n->label = get_ident_name(token->next);
    n->goto_next = gotos;
    gotos = n;
    token = token->next->next;
    
    expect(rest, token, ";");
    return n;
  }

  // label statement
  if (token->kind == TK_IDENT && equal(token->next, ":")) {
    n = new_node(ND_LABEL, token);
    n->label = get_ident_name(token);
    n->unique_label = new_unique_name();
    n->goto_next = labels;
    labels = n;
    token = token->next->next;
    n->lhs = stmt(rest, token);
    
    return n;
  }

  if (equal(token, "break")) {
    if (!break_label) {
      error_at(token->loc, "%s", "stray break");
    }

    n = new_node(ND_GOTO, token);
    n->unique_label = break_label;
    token = token->next;
    expect(rest, token, ";");
    return n;
  }

  if (equal(token, "continue")) {
    if (!continue_label) {
      error_at(token->loc, "%s", "stray continue");
    }

    n = new_node(ND_GOTO, token);
    n->unique_label = continue_label;
    token = token->next;
    expect(rest, token, ";");
    return n;
  }

  n = expr_stmt(&token, token);

  *rest = token;
  return n;
}

/*
  expr-stmt ::= ";" | expr ";"
*/
static Node *expr_stmt(Token **rest, Token *token) {
  if (consume(&token, token, ";")) {
    *rest = token;
    return new_node(ND_BLOCK, token);
  }

  Node *n = new_node_unary(ND_EXPR_STMT, expr(&token, token), token);
  expect(&token, token, ";");
  *rest = token;
  return n;
}

/*
  expr ::= assign ("," expr)*
*/
static Node *expr(Token **rest, Token *token) {
  Node *n = assign(&token, token);

  if (consume(&token, token, ",")) {
    return new_node_binary(ND_COMMA, n, expr(rest, token), token);
  }

  *rest = token;
  return n;
}

static int64_t eval(Node *n) {
  return eval2(n, NULL);
}

/*
  + global変数の初期化式
    int g1 = 1;
    int g2 = g1 + 1; --> local変数時と違いコンパイル時にg1が決まらないといけないため、これは不可
    char g3[] = "abc";
    char *g4 = g3 + 1; --> ポインタ計算はOK

  + local変数
    初期化式やcase文の式など、コンパイル時に値が決まる箇所での計算
    int a = 1; int b = a + 1; --> 実行時にaが初期化されるためOK(eval()は通らない)
    enum { two = 1+1 }; --> 実行時に初期化されるためOK
    int a = 1; enum { two = a+1 }; --> コンパイル時にaが決まらないため不可
    int x[3+3]; --> 実行時に初期化されるためOK
    int a = 1; int x[a+3]; --> コンパイル時にaが決まらないため不可
    case 1+2: --> 実行時に初期化されるためOK
    int a = 1; case a+2: --> コンパイル時にaが決まらないため不可
*/
static int64_t eval2(Node *n, char **label) {
  add_type(n);

  /*
   * These operators need unsigned handling because their semantics
   * differ between signed and unsigned integers:
   *   /  %  >>  <  <=  >  >=
   * Examples:
   *   -1 / 2                   => 0
   *   (uint64_t)-1 / 2         => 9223372036854775807
   *
   *   -1 >> 1                  => -1
   *   (uint64_t)-1 >> 1        => 9223372036854775807
   * 
   * Other operators can usually be evaluated as signed values because
   * they produce the same result on the underlying bit patterns.
   */
  switch (n->kind) {
  case ND_ADD:
    /*
      変数を利用した足し算はなし
      ポインタの足し算は、new_add()により左辺がポインタになる
    */
    return eval2(n->lhs, label) + eval(n->rhs);
  case ND_SUB:
    return eval2(n->lhs, label) - eval(n->rhs);
  case ND_MUL:
    return eval(n->lhs) * eval(n->rhs);
  case ND_DIV:
    if (n->ty->is_unsigned) {
      return (uint64_t)eval(n->lhs) / eval(n->rhs);
    }
    return eval(n->lhs) / eval(n->rhs);
  case ND_MOD:
    if (n->ty->is_unsigned) {
      return (uint64_t)eval(n->lhs) % eval(n->rhs);
    }
    return eval(n->lhs) % eval(n->rhs);
  case ND_OR:
    return eval(n->lhs) | eval(n->rhs);
  case ND_XOR:
    return eval(n->lhs) ^ eval(n->rhs);
  case ND_AND:
    return eval(n->lhs) & eval(n->rhs);
  case ND_NEG:
    return -eval(n->lhs);
  case ND_COND:
    return eval(n->cond) ? eval2(n->then, label) : eval2(n->els, label);
  case ND_LOGICALOR:
    return eval(n->lhs) || eval(n->rhs);
  case ND_LOGICALAND:
    return eval(n->lhs) && eval(n->rhs);
  case ND_COMMA:
    return eval2(n->rhs, label);
  case ND_NOT:
    return !eval(n->lhs);
  case ND_BITNOT:
    return ~eval(n->lhs);
  case ND_SHL:
    return eval(n->lhs) << eval(n->rhs);
  case ND_SHR:
    /*
     * Only 64-bit unsigned values need an explicit cast here.
     * Smaller types are promoted to signed int before evaluation,
     * so their right shift already follows the host C compiler rules.
     * 
     * Unsigned right shift must be a logical shift (zero-fill).
     *
     * Example:
     *   (uint64_t)-1 >> 1 => 0x7fffffffffffffff
     *
     * Signed right shift is typically arithmetic (sign-fill):
     *   -1 >> 1 => -1
     */
    if (n->ty->is_unsigned && n->ty->size == 8) {
      return (uint64_t)eval(n->lhs) >> eval(n->rhs);
    }
    return eval(n->lhs) >> eval(n->rhs);
  case ND_EQ:
    return eval(n->lhs) == eval(n->rhs);
  case ND_NEQ:
    return eval(n->lhs) != eval(n->rhs);
  case ND_LT:
    if (n->ty->is_unsigned) {
      return (uint64_t)eval(n->lhs) < eval(n->rhs);
    }
    return eval(n->lhs) < eval(n->rhs);
  case ND_LE:
    if (n->ty->is_unsigned) {
      return (uint64_t)eval(n->lhs) <= eval(n->rhs);
    }
    return eval(n->lhs) <= eval(n->rhs);
  case ND_CAST:
    int64_t val = eval2(n->lhs, label);

    if (is_integer(n->ty)) {
      switch (n->ty->size) {
      case 1:
        return n->ty->is_unsigned ? (uint8_t)val : (int8_t)val;
      case 2:
        return n->ty->is_unsigned ? (uint16_t)val : (int16_t)val;
      case 4:
        return n->ty->is_unsigned ? (uint32_t)val : (int32_t)val;
      }
    }

    return val;
  case ND_ADDR:
    /*
      global変数のアドレス取得
      int a = 1; int *p = &a;
      この場合の、&aの計算
    */
    return eval_rval(n->lhs, label);
  case ND_MEMBER:
    if (!label) {
      error_at(n->token->loc, "%s", "not a compile-time constant");
    }

    /*
      struct { int a[2]; } x = {1, 2};
      int *p = x.a + 1;
      のような場合の初期化計算
    */
    if (n->member->ty->kind != TY_ARRAY) {
      error_at(n->token->loc, "%s", "invalid initializer");
    }

    return eval_rval(n->lhs, label) + n->member->offset;
  case ND_VAR:
    if (!label) {
      error_at(n->token->loc, "%s", "not a compile-time constant");
    }

    if (n->var->ty->kind != TY_ARRAY && n->var->ty->kind != TY_FUNC) {
      error_at(n->token->loc, "%s", "invalid initializer");
    }

    *label = n->var->name;
    return 0;
  case ND_NUM:
    return n->val;
  }

  error_at(n->token->loc, "%s", "not a constant expression"); 

  return 0; // never reach here
}

static int64_t eval_rval(Node *n, char **label) {
  switch (n->kind) {
  case ND_VAR:
    if (n->var->is_local) {
      /*
        int a = 1;
        int b[&a] = {1};
      */
      error_at(n->token->loc, "%s", "not a compile-time constant");
    }
    *label = n->var->name;
    return 0;
  case ND_DEREF:
    return eval2(n->lhs, label);
  case ND_MEMBER:
    /*
      union { int a; int b; } x; int *p = &x.b; 
      ND_ADDR
        |
      ND_VAR
        |
      ND_MEMBER
        |
      ND_VAR
    */
    return eval_rval(n->lhs, label) + n->member->offset;
  }

  error_at(n->token->loc, "%s", "invalid initializer");

  return 0; // never reach here
}

/*
  値が決まる箇所の計算
  enum { two = 1+1 };, x[3+3], case 1+2: など
*/
static int64_t const_expr(Token **rest, Token *token) {
  Node *n = conditional(rest, token);
  return eval(n);
}

static Node *to_assign(Node *lhs, Node *rhs, Token *token) {
  return new_node_binary(ND_ASSIGN, lhs, rhs, token);
}

/*
  assign ::= bitor (assign-op assign)?
  assign-op ::= "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "|=" | "^=" | "&="
*/
static Node *assign(Token **rest, Token *token) {
  Node *n = conditional(&token, token);
  if (consume(&token, token, "=")) {
    return new_node_binary(ND_ASSIGN, n, assign(rest, token), token);
  }

  /*
    a += 1; -> a = a + 1;
      asign
    |       |
    n     new_add
        |         |
        n        assign
  */
  if (consume(&token, token, "+=")) {
    return to_assign(n, new_add(n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "-=")) {
    return to_assign(n, new_sub(n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "*=")) {
    return to_assign(n, new_node_binary(ND_MUL, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "/=")) {
    return to_assign(n, new_node_binary(ND_DIV, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "%=")) {
    return to_assign(n, new_node_binary(ND_MOD, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "|=")) {
    return to_assign(n, new_node_binary(ND_OR, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "^=")) {
    return to_assign(n, new_node_binary(ND_XOR, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "&=")) {
    return to_assign(n, new_node_binary(ND_AND, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, "<<=")) {
    return to_assign(n, new_node_binary(ND_SHL, n, assign(rest, token), token), token);
  }

  if (consume(&token, token, ">>=")) {
    return to_assign(n, new_node_binary(ND_SHR, n, assign(rest, token), token), token);
  }

  *rest = token;
  return n;
}

/*
  conditional ::= logicalor ("?" expr ":" conditional)?
*/
static Node *conditional(Token **rest, Token *token) {
  Node *n = logicalor(&token, token);

  if (equal(token, "?")) {
    Node *cond = new_node(ND_COND, token);
    cond->cond = n;
    token = token->next;
    cond->then = expr(&token, token);
    expect(&token, token, ":");
    cond->els = conditional(&token, token);

    *rest = token;
    return cond;
  }

  *rest = token;
  return n;
}

/*
  logicalor ::= logicaland ("||" logicaland)*
*/
static Node *logicalor(Token **rest, Token *token) {
  Node *n = logicaland(&token, token);

  while (equal(token, "||")) {
    Token *start = token;
    n = new_node_binary(ND_LOGICALOR, n, logicaland(&token, token->next), start);
  }

  *rest = token;
  return n;
}

/*
  logicaland ::= bitor ("&&" bitor)*
*/
static Node *logicaland(Token **rest, Token *token) {
  Node *n = bitor(&token, token);

  while (equal(token, "&&")) {
    Token *start = token;
    n = new_node_binary(ND_LOGICALAND, n, bitor(&token, token->next), start);
  }

  *rest = token;
  return n;
}

/*
  bitor ::= bitxor ("|" bitxor)*
*/
static Node *bitor(Token **rest, Token *token) {
  Node *n = bitxor(&token, token);

  while(equal(token, "|")) {
    Token *start = token;
    n = new_node_binary(ND_OR, n, bitxor(&token, token->next), start);
  }

  *rest = token;
  return n;
}

/*
  bitxor ::= bitand ("^" bitand)*
*/
static Node *bitxor(Token **rest, Token *token) {
  Node *n = bitand(&token, token);

  while (equal(token, "^")) {
    Token *start = token;
    n = new_node_binary(ND_XOR, n, bitand(&token, token->next), start);
  }

  *rest = token;
  return n;
}

/*
  bitand ::= equality ("&" equality)*
*/
static Node *bitand(Token **rest, Token *token) {
  Node *n = equality(&token, token);

  while (equal(token, "&")) {
    Token *start = token;
    n = new_node_binary(ND_AND, n, equality(&token, token->next), start);
  }

  *rest = token;
  return n;
}

/*
  equality ::= relational ("==" relational | "!=" relational)*
*/
static Node *equality(Token **rest, Token *token) {
  Node *n = relational(&token, token);

  for (;;) {
    if (consume(&token, token, "==")) {
      n = new_node_binary(ND_EQ, n, relational(&token, token), token);
    } else if (consume(&token, token, "!=")) {
      n = new_node_binary(ND_NEQ, n, relational(&token, token), token);
    } else {
      *rest = token;
      return n;
    }
  }

  *rest = token;
  return n;
}

/*
  relational ::= shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
*/
static Node *relational(Token **rest, Token *token) {
  Node *n = shift(&token, token);

  for (;;) {
    if (consume(&token, token, "<")) {
      n = new_node_binary(ND_LT, n, add(&token, token), token);
    } else if (consume(&token, token, ">")) {
      n = new_node_binary(ND_LT, add(&token, token), n, token);
    } else if (consume(&token, token, "<=")) {
      n = new_node_binary(ND_LE, n, add(&token, token), token);
    } else if (consume(&token, token, ">=")) {
      n = new_node_binary(ND_LE, add(&token, token), n, token);
    } else {
      *rest = token;
      return n;
    }
  }

  *rest = token;
  return n;
}

/*
  shift ::= add ("<<" add | ">>" add)*
*/
static Node *shift(Token **rest, Token *token) {
  Node *n = add(&token, token);

  for (;;) {
    if (consume(&token, token, "<<")) {
      n = new_node_binary(ND_SHL, n, add(&token, token), token);
    } else if (consume(&token, token, ">>")) {
      n = new_node_binary(ND_SHR, n, add(&token, token), token);
    } else {
      *rest = token;
      return n;
    }
  }

  *rest = token;
  return n;
}

static Node *new_add(Node *lhs, Node *rhs, Token *token) {
  add_type(lhs);
  add_type(rhs);

  Node *n;

  // num + num
  if (!lhs->ty->base && !rhs->ty->base) {
    n = new_node_binary(ND_ADD, lhs, rhs, token);
    return n;
  }

  // pointer + pointer
  if (lhs->ty->base && rhs->ty->base) {
    error_at(token->loc, "%s", "invalid operand");
  }

  // num + pointer to pointer + num
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }
  
  // pointer + num * ty->size
  // int -> 4byte
  n = new_node_binary(ND_ADD, lhs, new_node_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, token), token), token);

  return n;
}

static Node *new_sub(Node *lhs, Node *rhs, Token *token) {
  add_type(lhs);
  add_type(rhs);

  Node *n;

  // num - num
  if (!lhs->ty->base && !rhs->ty->base) {
    n = new_node_binary(ND_SUB, lhs, rhs, token);
    return n;
  }

  // pointer - num * ty->size 
  // int -> 4byte
  if (lhs->ty->base && !rhs->ty->base) {
    n = new_node_binary(ND_SUB, lhs, new_node_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, token), token), token);
    return n;
  }

  // pointer - pointer, return int elements between pointer and pointer
  if (lhs->ty->base && rhs->ty->base) {
    n = new_node_binary(ND_SUB, lhs, rhs, token);
    n->ty = cp_type(ty_long);
    return new_node_binary(ND_DIV, n, new_node_num(lhs->ty->base->size, token), token);
  }

  error_at(token->loc, "%s", "invalid operand");

  return NULL; // never reach here
}

/*
  add ::= mul ("+" new_add | "-" new_sub)*
*/
static Node *add(Token **rest, Token *token) {
  Node *n = mul(&token, token);

  for (;;) {
    if (consume(&token, token, "+")) {
      n = new_add(n, mul(&token, token), token);
    } else if (consume(&token, token, "-")) {
      n = new_sub(n, mul(&token, token), token);
    } else {
      *rest = token;
      return n;
    }
  }

  *rest = token;
  return n;
}

/*
  mul ::= cast ("*" cast | "/" cast | "%" cast)*
*/
static Node *mul(Token **rest, Token *token) {
  Node *n = cast(&token, token);

  for (;;) {
    if (consume(&token, token, "*")) {
      n = new_node_binary(ND_MUL, n, cast(&token, token), token);
    } else if (consume(&token, token, "/")) {
      n = new_node_binary(ND_DIV, n, cast(&token, token), token);
    } else if (consume(&token, token, "%")) {
      n = new_node_binary(ND_MOD, n, cast(&token, token), token);
    } else {
      *rest = token;
      return n;
    }
  }

  *rest = token;
  return n;
}

/*
  cast ::= "(" typename ")" cast
         | unary
*/
static Node *cast(Token **rest, Token *token) {
  if (equal(token, "(") && is_type(token->next)) {
    Token *start = token;
    token = token->next;

    Type *ty = typename(&token, token);
    expect(&token, token, ")");

    // compound literal
    if (equal(token, "{")) {
      // 上記parseは無視して次に行く
      return unary(rest, start);
    }

    Node *nc = cast(&token, token);

    *rest = token;
    return new_cast(nc, ty, start);
  }

  return unary(rest, token);
}

/*
  abstract-declarator ::= pointers ("(" abstract-declarator ")")? type-suffix
*/
static Type *abstract_declarator(Token **rest, Token *token, Type *ty) {
  ty = pointers(&token, token, ty);
  
  if (equal(token, "(")) {
    Token *start = token;
    token = token->next;
    
    Type dummy = {};
    abstract_declarator(&token, token, &dummy);
    expect(&token, token, ")");

    ty = type_suffix(rest, token, ty);

    return abstract_declarator(&token, start->next, ty);
  }

  return type_suffix(rest, token, ty);
}

/*
  typename ::= declspec abstract-declarator
*/
static Type *typename(Token **rest , Token *token) {
  Type *basety = declspec(&token, token, NULL);
  return abstract_declarator(rest, token, basety);
}

/*
  unary ::= "sizeof" "(" typename ")"
          | "sizeof" cast
          | "_Alignof" "(" typename ")"
          | "_Alignof" cast
          | ("+" | "-" | "*" | "&" | "!") cast
          | ("++" | "--") unary
          | postfix
*/
static Node *unary(Token **rest, Token *token) {
  Node *n;

  if (equal(token, "sizeof") && equal(token->next, "(") && is_type(token->next->next)) {
    Token *start = token;
    token = token->next->next;

    Type *ty = typename(&token, token);

    if (ty->kind == TY_ARRAY && ty->size < 0) {
      error_at(token->loc, "%s", "incomplete array type");
    }

    n = new_ulong(ty->size, start);
    expect(&token, token, ")");

    *rest = token;
    return n;
  }

  if (equal(token, "sizeof")) {
    token = token->next;
    n = cast(&token, token);
    add_type(n);

    *rest = token;
    return new_ulong(n->ty->size, token);
  }

  if (equal(token, "_Alignof") && equal(token->next, "(") && is_type(token->next->next)) {
    token = token->next->next;
    Type *ty = typename(&token, token);
    expect(&token, token, ")");

    *rest = token;
    return new_ulong(ty->align, token);
  }

  if (equal(token, "_Alignof")) {
    token = token->next;
    n = cast(&token, token);
    add_type(n);

    *rest = token;
    return new_ulong(n->ty->align, token);
  }

  if (consume(&token, token, "+")) {
    n = cast(&token, token);
    *rest = token;
    return n;
  }

  if (consume(&token, token, "-")) {
    n = new_node_binary(ND_NEG, cast(&token, token), NULL, token);
    *rest = token;
    return n;
  }

  if (consume(&token, token, "*")) {
    n = new_node(ND_DEREF, token);
    n->lhs = cast(&token, token);

    *rest = token;
    return n;
  }

  if (consume(&token, token, "&")) {
    n = new_node(ND_ADDR, token);
    n->lhs = cast(&token, token);

    *rest = token;
    return n;
  }

  if (consume(&token, token, "!")) {
    n = new_node(ND_NOT, token);
    n->lhs = cast(&token, token);

    *rest = token;
    return n;
  }

  if (consume(&token, token, "~")) {
    n = new_node(ND_BITNOT, token);
    n->lhs = cast(&token, token);

    *rest = token;
    return n;
  }

  /*
    ++a -> a = a + 1
    --a -> a = a - 1
  */
  if (consume(&token, token, "++")) {
    n = unary(rest, token);
    return new_node_binary(ND_ASSIGN, n, new_add(n, new_node_num(1, token), token), token);
  }

  if (consume(&token, token, "--")) {
    n = unary(rest, token);
    return new_node_binary(ND_ASSIGN, n, new_sub(n, new_node_num(1, token), token), token);
  }

  n = postfix(&token, token);
  *rest = token;
  return n;
}

static Node *struct_ref(Token *token, Node *lhs) {
  // token is "."
  add_type(lhs);

  if (lhs->ty->kind != TY_STRUCT && lhs->ty->kind != TY_UNION) {
    error_at(token->loc, "%s", "not struct");
  }

  Node *n = new_node_unary(ND_MEMBER, lhs, token);
  n->member = find_member(lhs->ty, token->next);

  return n;
}

/*
  postfix ::= "(" type-name ")" "{" initializer-list "}"
            | primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
*/
static Node *postfix(Token **rest, Token *token) {
  // compound literal
  if (equal(token, "(") && is_type(token->next)) {
    Token *start = token;
    token = token->next;

    Type *ty = typename(&token, token);
    expect(&token, token, ")");
    
    // global variable
    if (scope->next == NULL) {
      Obj *gvar = new_gvar(new_unique_name(), ty);
      gvar_initializer(&token, token, gvar);
      *rest = token;
      return new_node_var(gvar, start);
    }

    // local variable
    Obj *lvar = new_lvar("", ty);
    Node *lhs = lvar_initializer(&token, token, lvar);
    Node *rhs = new_node_var(lvar, token);
    *rest = token;
    return new_node_binary(ND_COMMA, lhs, rhs, start);
  }

  Node *n = primary(&token, token);

  //array
  for (;;) {
    // array
    if (equal(token, "[")) {
      token = token->next;

      Node *ex = expr(&token, token);

      // a[3] -> *(a+3) -> *(a + 3 * ty->size)
      n = new_node_binary(ND_DEREF, new_add(n, ex, token), NULL, token);

      expect(&token, token, "]");
      continue;
    }

    //struct member
    if (equal(token, ".")) {
      n = struct_ref(token, n);
      token = token->next->next;

      continue;
    }

    //struct member
    // a->b is (*a).b
    if (equal(token, "->")) {
      n = struct_ref(token, new_node_unary(ND_DEREF, n, token));
      token = token->next->next;

      continue;
    }

    // a++ -> (typeof a)((a += 1) - 1)
    if (equal(token, "++")) {
      n = to_assign(n, new_add(n, new_node_num(1, token), token), token);
      n = new_sub(n, new_node_num(1, token), token);

      add_type(n);
      n = new_cast(n, n->ty, token);

      token = token->next;
      continue;
    }

    // a-- -> (typeof a)((a -= 1) + 1)
    if (equal(token, "--")) {
      n = to_assign(n, new_sub(n, new_node_num(1, token), token), token);
      n = new_sub(n, new_node_num(-1, token), token);

      add_type(n);
      n = new_cast(n, n->ty, token);

      token = token->next;
      continue;
    }

    *rest = token;
    return n;
  }
}

static Node *funcall(Token **rest, Token *token) {
  Token *start = token;

  if (equal(token, "__builtin_va_start")) {
    token = token->next;
    expect(&token, token, "(");

    Node head = {};
    Node *cur = &head;
    // 1つ目の引数: ap
    Node *arg = assign(&token, token);
    add_type(arg);

    cur = cur->next = arg;
    expect(&token, token, ",");

    // 2つ目の引数: last
    arg = assign(&token, token);
    add_type(arg);
    cur = cur->next = arg;

    expect(&token, token, ")");


    Type *ty = cp_type(ty_void);
    Node *n = new_node(ND_FUNC, token);
    n->func_ty = ty_func(ty);
    n->ty = ty;
    n->funcname = strndup(start->loc, start->len);
    n->args = head.next;

    *rest = token;
    return n;
  }

  if (equal(token, "__builtin_va_arg")) {
    // __builtin_va_argは、引数の型をコンパイル時に決める必要があるため、特殊に処理する
    token = token->next;
    expect(&token, token, "(");

    Node head = {};
    Node *cur = &head;
    // 1つ目の引数: ap
    Node *arg = assign(&token, token);
    add_type(arg);

    cur = cur->next = arg;
    expect(&token, token, ",");

    // 2つ目の引数: type
    Type *ty = typename(&token, token);

    /*
      va_arg(ap, type)のtypeにおいて4byte未満の整数型は、intに昇格(default argument promotions)するが、
      指定するtype名は、charやshortなどの4byte未満の整数型は指定できない。
    */
    switch (ty->kind) {
    case TY_CHAR:
    case TY_SHORT:
      error("%s: invalid type for va_arg", get_type_name(ty));
    case TY_INT:
    case TY_LONG:
      break;
    default:
      error("%s: invalid type for va_arg", get_type_name(ty));
    }

    expect(&token, token, ")");


    Node *n = new_node(ND_FUNC, token);
    /*
      返り値のtypeは、va_start(ap, type)のtype
    */
    ty->return_ty = ty;
    n->func_ty = ty_func(ty);
    n->ty = ty;
    n->funcname = strndup(start->loc, start->len);
    n->args = head.next;

    *rest = token;
    return n;
  }

  /*
    va_end(): va_startで初期化したva_listの使用終了を示す。
    多くの環境では何も処理しないが、実装依存で必要な後処理を行うために存在する。
  */
  if (equal(token, "__builtin_va_end")) {
    token = token->next;
    expect(&token, token, "(");
    Node head = {};
    Node *cur = &head;
    // 引数: ap
    Node *arg = assign(&token, token);
    add_type(arg);

    cur = cur->next = arg;

    expect(&token, token, ")");

    Type *ty = cp_type(ty_void);
    Node *n = new_node(ND_FUNC, token);
    n->func_ty = ty_func(ty);
    n->ty = ty;
    n->funcname = strndup(start->loc, start->len);
    n->args = head.next;

    *rest = token;
    return n;
  }

  VarScope *vs = find_var(token);
  if (!vs) {
    error_at(token->loc, "%s", "this function not definded");
  }
  if (!vs->var || vs->var->ty->kind != TY_FUNC) {
    error_at(token->loc, "%s", "not function");
  }

  Type *ty = vs->var->ty;
  Type *param_ty = ty->params;

  Node head = {};
  Node *cur = &head;
  
  // functionname (arg, ...);
  token = token->next;
  expect(&token, token, "(");

  while (!equal(token, ")")) {
    if (cur != &head)
      expect(&token, token, ",");

    Node *arg = assign(&token, token);
    add_type(arg);

    if (param_ty) {
      if (param_ty->kind == TY_STRUCT || param_ty->kind == TY_UNION) {
        error_at(token->loc, "%s", "struct or union is not permitted");
      }

      arg = new_cast(arg, param_ty, token);
      param_ty = param_ty->next;
    } else {
      if (ty->is_variadic) {
        if (arg->ty->kind == TY_STRUCT || arg->ty->kind == TY_UNION) {
          error_at(token->loc, "%s", "struct or union is not permitted");
        }

        // default argument promotions
        if (is_integer(arg->ty) && arg->ty->size < 4) {
          arg = new_cast(arg, cp_type(ty_int), token);
        }
      } else {
        error_at(token->loc, "%s", "too many arguments");
      }
    }

    cur = cur->next = arg;
  }

  expect(&token, token, ")");

  Node *n = new_node(ND_FUNC, token);
  n->func_ty = ty;
  n->ty = ty->return_ty;
  n->funcname = strndup(start->loc, start->len);
  n->args = head.next;

  *rest = token;
  return n;
}

/*
  primary ::= "(" "{" stmt+ "}" ")"
            | "(" expr ")"
            | ident ( "(" assign "," ")" )?
            | str
            | num
*/
static Node *primary(Token **rest, Token *token) {
  if (equal(token, "(") && equal(token->next, "{")) {
    token = token->next;
    /*
      add_type()のND_STMT_EXPRで下記パターンを除外

      + ({compound-stmt+})のcompound-stmtの中でreturn
      + empty block
    */
    Node *n = new_node(ND_STMT_EXPR, token);
    n->body = compound_stmt(&token, token)->body;
    expect(&token, token, ")");

    *rest = token;
    return n;
  }

  if (consume(&token, token, "(")) {
    Node *n = expr(&token, token);
    expect(&token, token, ")");
    *rest = token;
    return n;
  }

  if (token->kind == TK_IDENT) {
    //funcall
    if (equal(token->next, "("))
      return funcall(rest, token);

    //var
    VarScope *vs = find_var(token);
    if (!vs || (!vs->var && !vs->enum_ty)) {
      error_at(token->loc, "%s", "variable not definded");
    }

    Node *n;
    if (vs->var) {
      n = new_node_var(vs->var, token);
    } else {
      n = new_node_num(vs->enum_val, token);
    }

    token = token->next;
    *rest = token;
    return n;
  }

  if (token->kind == TK_STR) {
    // string literalは、.data領域に格納する。
    Obj *var = new_string_literal(token->str, token->ty);
    *rest = token->next;
    return new_node_var(var, token);
  }

  if (token->kind == TK_NUM) {
    Node *n;
    if (is_flonum(token->ty)) {
      n = new_node(ND_NUM, token);
      n->fval = token->fval;
    } else {
      n = new_node_num(token->val, token);
    }

    n->ty = token->ty;
    *rest = token->next;
    return n;
  }

  error_at(token->loc, "%s", "expect ({}), (), ident, funcall, str and num");

  return NULL; // never reach here
}

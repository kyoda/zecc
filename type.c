#include "9cc.h"

Type *ty_void = &(Type){.kind = TY_VOID, .size = 1, .align = 1};
Type *ty_bool = &(Type){.kind = TY_BOOL, .size = 1, .align = 1};
Type *ty_char = &(Type){.kind = TY_CHAR, .size = 1, .align = 1, .is_unsigned = false};
Type *ty_uchar = &(Type){.kind = TY_CHAR, .size = 1, .align = 1, .is_unsigned = true};
Type *ty_short = &(Type){.kind = TY_SHORT, .size = 2, .align = 2, .is_unsigned = false};
Type *ty_ushort = &(Type){.kind = TY_SHORT, .size = 2, .align = 2, .is_unsigned = true};
Type *ty_int = &(Type){.kind = TY_INT, .size = 4, .align = 4, .is_unsigned = false};
Type *ty_uint = &(Type){.kind = TY_INT, .size = 4, .align = 4, .is_unsigned = true};
Type *ty_long = &(Type){.kind = TY_LONG, .size = 8, .align = 8, .is_unsigned = false};
Type *ty_ulong = &(Type){.kind = TY_LONG, .size = 8, .align = 8, .is_unsigned = true};
Type *ty_enum = &(Type){.kind = TY_ENUM, .size = 4, .align = 4};
Type *ty_struct = &(Type){.kind = TY_STRUCT, .size = 0, .align = 1};
Type *ty_va_list = &(Type){.kind = TY_STRUCT, .size = 24, .align = 8};

Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  ty->is_unsigned = true;

  return ty;
}

Type *ty_array(Type *base, int len) {
  Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;

  return ty;
}

Type *ty_func(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_FUNC;
  ty->return_ty = base;

  return ty;
}

Type *cp_type(Type *ty) {
  Type *new_ty = calloc(1, sizeof(Type));
  *new_ty = *ty;
  return new_ty;
}

static Type *get_common_type(Type *ty1, Type *ty2) {
  // ty1 is pointer
  if (ty1->base) {
    return pointer_to(ty1->base);
  }

  // sizeが4byte未満の整数型は、intに昇格する(integer promotion)
  if (ty1->size < 4) {
    ty1 = cp_type(ty_int);
  }

  if (ty2->size < 4) {
    ty2 = cp_type(ty_int);
  }

  // 2つの型のうち、サイズが大きい方を採用する
  // この場合、signedとunsignedは考慮しない
  if (ty1->size != ty2->size) {
    return ty1->size < ty2->size ? ty2 : ty1;
  }

  /*
     サイズが同じであれば、unsignedを優先する。下記パターンがある。
     signed, unsigned
     unsigned, unsigned
     unsigned, signed
     signed, signed
  */
  if (ty2->is_unsigned) {
    return ty2;
  }

  // わかりやすさ優先で下記条件式を入れている。なくても同じ結果になる。
  if (ty1->is_unsigned) {
    return ty1;
  }

  return ty1;
}

//usual arithmetic conversions
static void usual_arith_conv(Node **lhs, Node **rhs) {
  Type *ty = get_common_type((*lhs)->ty, (*rhs)->ty);

  *lhs = new_cast(*lhs, ty, (*lhs)->token);
  *rhs = new_cast(*rhs, ty, (*rhs)->token);
}

char *get_type_name(Type *ty) {
  switch (ty->kind) {
  case TY_VOID: return "void";
  case TY_BOOL: return "_Bool";
  case TY_CHAR: return "char";
  case TY_SHORT: return "short";
  case TY_INT: return "int";
  case TY_LONG: return "long";
  case TY_ENUM: return "enum";
  case TY_STRUCT: return "struct";
  case TY_UNION: return "union";
  default: error("%s", "invalid type");
  }

  return NULL;
}

bool is_integer(Type *ty) {
  TypeKind kind = ty->kind;
  return kind == TY_BOOL || 
         kind == TY_CHAR ||
         kind == TY_SHORT ||
         kind == TY_INT ||
         kind == TY_ENUM ||
         kind == TY_LONG;
}

void add_type(Node *n) {
  if (!n || n->ty) {
    return;
  }

  add_type(n->lhs);
  add_type(n->rhs);
  add_type(n->init);
  add_type(n->cond);
  add_type(n->inc);
  add_type(n->then);
  add_type(n->els);

  for (Node *node = n->body; node; node = node->next) {
    add_type(node);
  }

  for (Node *node = n->args; node; node = node->next) {
    add_type(node);
  }

  switch(n->kind) {
  case ND_NUM:
    n->ty = cp_type(ty_int);
    return;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_OR:
  case ND_XOR:
  case ND_AND:
    usual_arith_conv(&n->lhs, &n->rhs);
    n->ty = n->lhs->ty;
    return;
  case ND_NEG: {
    Type *ty = get_common_type(cp_type(ty_int), n->lhs->ty);
    n->lhs = new_cast(n->lhs, ty, n->lhs->token);
    n->ty = ty;
    return;
  }
  case ND_ASSIGN:
    if (n->lhs->ty->kind == TY_ARRAY) {
      error("%s", "left variable is array");
    }

    if (n->lhs->ty->kind != TY_STRUCT) {
      n->rhs = new_cast(n->rhs, n->lhs->ty, n->rhs->token);
    }

    n->ty = n->lhs->ty;

    return;
  case ND_COMMA:
    n->ty = n->rhs->ty;
    return;
  case ND_MEMBER:
    n->ty = n->member->ty;
    return;
  case ND_EQ:
  case ND_NEQ:
  case ND_LT:
  case ND_LE:
    usual_arith_conv(&n->lhs, &n->rhs);
    n->ty = cp_type(ty_int);
    return;
  case ND_FUNC:
    n->ty = cp_type(ty_long);
    return;
  case ND_VAR:
    n->ty = n->var->ty;
    return;
  case ND_ADDR:
    n->ty = pointer_to(n->lhs->ty);
    return;
  case ND_DEREF:
    if (!n->lhs->ty->base) {
      error("%s", "invalid deref");
    }

    if (n->lhs->ty->base->kind == TY_VOID) {
      error("%s", "dereferencing a void pointer");
    }

    n->ty = n->lhs->ty->base;
    return;
  case ND_COND:
    if (n->then->ty->kind == TY_VOID || n->els->ty->kind == TY_VOID) {
      n->ty = cp_type(ty_void);
    } else {
      usual_arith_conv(&n->then, &n->els);
      n->ty = n->then->ty;
    }

    return;
  case ND_NOT:
  case ND_LOGICALOR:
  case ND_LOGICALAND:
    n->ty = cp_type(ty_int);
    return;
  case ND_BITNOT:
  case ND_SHL:
  case ND_SHR:
    n->ty = n->lhs->ty;
    return;
  case ND_STMT_EXPR:
    /*
      GNU C拡張機能のStatement Expressions
      https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html#Statement-Exprs
    */
    if (n->body) {
      /*
        bodyの最後がND_EXPR_STMTであることを確認し、そのlhsの型を割り当てる
        そのためbodyの最後は、「expr ";"」
        下記のようなパターンは許可しない
          + ({})
          + ({return 0;})
      */
      Node *stmt = n->body;
      while (stmt->next) {
        stmt = stmt->next;
      }

      if (stmt->kind == ND_EXPR_STMT) {
        n->ty = stmt->lhs->ty;
        return;
      }
    }

    error("%s", "statement expression returning void is not supported");
    return;
  }

}

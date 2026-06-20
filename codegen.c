#include "9cc.h"

static Obj *current_fn;
static FILE *out;
static int depth;
static int current_line;
static void gen_stmt(Node *n);
static void gen_expr(Node *n);
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};

static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);
  fprintf(out, "\n");
}

static int count() {
  static int i = 1;
  return i++;
}

static void push(void) {
  println("  push rax");
  depth++;
}

static void pop(char *arg) {
  println("  pop %s", arg);
  depth--;
}

static void load(Type *ty) {
  if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    return;
  }

  char *mov = ty->is_unsigned ? "movzx" : "movsx";

  switch(ty->size) {
  case 1:
    println("  %s eax, BYTE PTR [rax]", mov);
    break;
  case 2:
    println("  %s eax, WORD PTR [rax]", mov);
    break;
  case 4:
    println("  %s rax, DWORD PTR [rax]", mov);
    break;
  default:
    println("  mov rax, [rax]");
    break;
  } 

}

static void store(Type *ty) {
  pop("rdi"); //rhs
  pop("rax"); //lhs

  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (int i = 0; i < ty->size; i++) {
      println("  mov r8b, [rdi + %d]", i);
      println("  mov [rax + %d], r8b", i);
    }

    println("  mov rax, rdi");
    return;
  }


  switch (ty->size) {
  case 1:
    println("  mov [rax], dil");
    break;
  case 2: 
    println("  mov [rax], di");
    break;
  case 4: 
    println("  mov [rax], edi");
    break;
  default:
    println("  mov [rax], rdi");
    break;
  }

  println("  mov rax, rdi");
}

static void gen_addr(Node *n) {
  switch(n->kind) {
  case ND_VAR:
    if (n->var->is_local) {
      // local variable
      println("  lea rax, [rbp - %d]", n->var->offset);
    } else {
      // global variable
      println("  lea rax, %s", n->var->name);
      //println("  lea rax, %s[rip]", n->var->name);
      //println("  mov rax, offset %s", n->var->name);
    }
    return;
  case ND_MEMBER:
    gen_addr(n->lhs);
    println("  add rax, %d", n->member->offset);
    return;
  case ND_DEREF:
    gen_expr(n->lhs);
    return;
  case ND_COMMA:
    gen_expr(n->lhs);
    gen_addr(n->rhs);
    return; 
  }

  error("expected a variable");
}

// 引数をstackに逆順にpushする(right to leftでpushするための再帰関数)
static void push_stacked(Node *n) {
  if (!n) {
    return;
  }
  push_stacked(n->next);
  gen_expr(n);
  push();
}

enum {I8, I16, I32, I64, U8, U16, U32, U64};
static int getTypeId(Type *ty) {
  switch (ty->kind) {
  case TY_CHAR:
    return ty->is_unsigned ? U8 : I8;
  case TY_SHORT:
    return ty->is_unsigned ? U16 : I16;
  case TY_INT:
    return ty->is_unsigned ? U32 : I32;
  default:
    return ty->is_unsigned ? U64 : I64;
  }
}

/*
  拡張は3種類しかない。また、fromがsignedかunsignedかでmovsxかmovzxを選択する。
  
  1. sign extend
  2. zero extend
  3. truncate(何もしない)

  [sign-extend]
    + 同じbit数同士は、cast不要
    + 32bit未満の値は、演算や比較の前に必要に応じて32bitへ拡張する。
      - signedならmovsx、unsignedならmovzxを使う。
      - 32bit値を64bit unsignedへ拡張する場合は、eaxへ書き込むことでrax上位32bitがゼロクリアされる。
    + 下位bitから64bitに拡張する場合は32bitレジスタを直接64bitへ
      - movsxで符号拡張
      - 自動で上位bitは0埋めされる
    + 上記以外の下位bitから64bitに拡張する場合は、上記の内容の通り32bit以下はeaxのため、32bit to 64bitと同じ
  [zero-extend]
  [truncate]
    + 64bitから32bitへは、何もしない
    + 上位bitから下位bitに縮小する場合は、32bitレジスタへ
    + truncate時にmovsx/movzxするのではなく、truncate後の値を再び32bit以上として使うときにmovsx/movzxする
  
   [tips]
      movzx は 8bit/16bit → 32bit/64bit のゼロ拡張に使う

        movzx r32, r8/r16
        movzx r64, r8/r16

      ※ 32bit → 64bit の movzx は存在しない
        movzx r64, r32  // 不可

      ※ 32bit → 64bit のゼロ拡張は、
         r32 に書き込むことで r64 の上位32bitが自動で0になる
*/

static char i8to32[] = "movsx eax, al";
static char u8to32[] = "movzx eax, al";
static char i16to32[] = "movsx eax, ax";
static char u16to32[] = "movzx eax, ax";
static char i32to64[] = "movsxd rax, eax";
static char u32to64[] = "mov eax, eax"; // eax書けば上位は0埋めされる

static char *cast_table[][8] = {
  // to I8, I16, I32, I64, U8, U16, U32, U64
  {i8to32, i8to32, i8to32, i32to64, i8to32, i8to32, i8to32, i32to64},     //from I8
  {i8to32, i16to32, i16to32, i32to64, i8to32, i16to32, i16to32, i32to64}, //from I16
  {i8to32, i16to32, NULL, i32to64, i8to32, i16to32, NULL, i32to64},       //from I32
  {i8to32, i16to32, NULL, NULL, i8to32, i16to32, NULL, NULL},             //from I64
  {u8to32, u8to32, u8to32, u32to64, u8to32, u8to32, u8to32, u32to64},     //from U8
  {u8to32, u16to32, u16to32, u32to64, u8to32, u16to32, u16to32, u32to64}, //from U16
  {u8to32, u16to32, NULL, u32to64, u8to32, u16to32, NULL, u32to64},       //from U32
  {u8to32, u16to32, NULL, NULL, u8to32, u16to32, NULL, NULL}              //from U64
};

static void cast(Type *from, Type *to) {
  if (to->kind == TY_VOID) {
    return;
  }

  if (to->kind == TY_BOOL) {
    if (is_integer(from) && from->size <= 4) {
      println("  cmp eax, 0");
    } else {
      println("  cmp rax, 0");
    }

    println("  setne al");
    println("  movzb eax, al");
    return;
  }

  int t1 = getTypeId(from);
  int t2 = getTypeId(to);
  if (cast_table[t1][t2]) {
    println("  %s", cast_table[t1][t2]);
  }
}

static void gen_expr(Node *n) {
  if (n->token->line != current_line) {
    current_line = n->token->line;
    println("  .loc 1 %d", n->token->line);
  }

  switch (n->kind) {
  case ND_NUM:

    /* 
      IEEE754: Floating-point format
        exponent uses a bias (127 for float, 1023 for double).
        fraction stores bits after the implicit leading 1.
        value = (-1)^sign * 1.fraction * 2^(exp-bias)

      float(32bit)
        1bit: sign
        8bit: exponent
        23bit: fraction

      31      30         23 22                0
      +---------+-----------+------------------+
      | Sign(1) | Exp(8)    | Fraction(23)     |
      +---------+-----------+------------------+

      double(64bit)
      63      62          52 51                 0
      +---------+------------+-------------------+
      | Sign(1) | Exp(11)    | Fraction(52)      |
      +---------+------------+-------------------+

    */
    // Reinterpret floating-point values as integer bit patterns.
    union {
      float f32;
      double f64;
      uint32_t u32;
      uint64_t u64;
    } u;

    // Move float/double literal to xmm0 via its IEEE754 bit pattern.
    switch (n->ty->kind) {
    case TY_FLOAT:
      u.f32 = n->fval;
      println("  mov eax, %u", u.u32);
      println("  movq xmm0, rax");
      break;
    case TY_DOUBLE:
      u.f64 = n->fval;
      println("  mov rax, %lu", u.u64);
      println("  movq xmm0, rax");
      break;
    }


    println("  mov rax, %ld", n->val);

    return;
  case ND_NEG:
    gen_expr(n->lhs);
    if (n->ty->size == 8) {
      println("  neg rax");
    } else {
      println("  neg eax");
    }

    return;
  case ND_VAR:
  case ND_MEMBER:
    gen_addr(n);
    load(n->ty);

    return;
  case ND_NULL_EXPR:
    return;
  case ND_STMT_EXPR: {
    //expresion to statement
    for (Node *nb = n->body; nb; nb = nb->next) {
      gen_stmt(nb);
    }

    return;
  }
  case ND_FUNC: {

    /*
      通常であれば、引数をセットしてcall命令を実行するが、組み込み関数の場合はそのまま処理する
    */
    if (strcmp(n->funcname, "__builtin_va_start") == 0) {
      /*
        void __builtin_va_start(va_list ap, last);
        last: 可変長引数の直前の引数, 32bit時代の名残であり利用しない
        ap: va_list構造体へのポインタ

        struct {
          unsigned int gp_offset; // 汎用レジスタをどこまで読んだかを指す。最大6 * 8byte , つまり可変長引数として最初どこを読むかを指している
          unsigned int fp_offset; // 浮動小数点レジスタをどこまで読んだかを指す
          void *overflow_arg_area; //レジスタに入りきらなかった「7番目以降の引数」が置かれているスタック上の位置を指すポインタ
          void *reg_save_area; //関数の最初(プロローグ)で、引数レジスタ*rdi, rsi, rdx, …, xmm0…xmm7)の内容を一時的にメモリにコピーしておく領域のアドレス。
        } va_elem;
      */

      println("  # __builtin_va_start");

      // apのアドレスをrdiにセット
      gen_addr(n->args);
      println("  mov rdi, rax");
      // rsiにva_areaのアドレスをセット
      println("  lea rsi, [rbp - %d]", current_fn->va_area->offset);
      // gp_offsetとfp_offset
      println("  mov rax, QWORD PTR [rsi + 0]"); // va_area->gp_offset, va_area->fp_offsetの値をraxに取得
      println("  mov QWORD PTR [rdi + 0], rax"); // va_area->gp_offset, va_area->fp_offsetの値をap->gp_offset, ap->fp_offsetにコピー
      // overflow_arg_area
      println("  mov rax, QWORD PTR [rsi + 8]"); // va_area->overflow_arg_areaの値をraxに取得
      println("  mov QWORD PTR [rdi + 8], rax"); // va_area->overflow_arg_areaの値をap->overflow_arg_areaにコピー
      // reg_save_area
      println("  mov rax, QWORD PTR [rsi + 16]"); // va_area->reg_save_areaの値をraxに取得
      println("  mov QWORD PTR [rdi + 16], rax"); // va_area->reg_save_areaの値をap->reg_save_areaにコピー

      return;
    }

    if (strcmp(n->funcname, "__builtin_va_arg") == 0) {
      /*
        type __builtin_va_arg(va_list ap, type);
        ap: va_list構造体へのポインタ
        type: 取得する引数の型
      */

      int c = count();
      println("  # __builtin_va_arg");
      // apのアドレスをrdiにセット
      gen_addr(n->args);
      println("  mov rdi, rax");
      // gp_offsetの値をrsiに取得
      println("  mov esi, DWORD PTR [rdi + 0]"); // ap->gp_offsetの値をrsiに取得

      /*
        ap->gp_offsetの値が48未満であれば、reg_save_areaから引数を取得する
        そうでなければ、overflow_arg_areaから引数を取得する
      */
      // cmp命令でgp_offsetの値が48未満かを判定
      println("  cmp rsi, 48");
      println("  jl .Luse_reg%03d", c);

      // 48以上であれば、overflow_arg_areaから引数を取得する
      println("  mov rdx, QWORD PTR [rdi + 8]");

      switch (n->ty->size) {
      case 1:
        println("  movzx eax, BYTE PTR [rdx]");
        break;
      case 2:
        println("  movzx eax, WORD PTR [rdx]");
        break;
      case 4: 
        println("  mov eax, DWORD PTR [rdx]");
        break;
      default:
        println("  mov rax, [rdx]");
        break;
      }
      // overflow_arg_areaを更新する
      println("  add QWORD PTR [rdi + 8], 8");
      println("  jmp .Luse_reg_end%03d", c);

      // gp_offset < 48であれば、reg_save_areaから引数を取得する
      println(".Luse_reg%03d:", c);
      println("  mov rdx, QWORD PTR [rdi + 16]"); // ap->reg_save_areaの値をrdxに取得
      println("  add rdx, rsi"); // reg_save_areaの先頭からgp_offsetの値だけ進める

      switch (n->ty->size) {
      case 1:
        println("  movzx eax, BYTE PTR [rdx]");
        break;
      case 2:
        println("  movzx eax, WORD PTR [rdx]");
        break;
      case 4: 
        println("  mov eax, DWORD PTR [rdx]");
        break;
      default:
        println("  mov rax, [rdx]");
        break;
      }

      // gp_offsetを更新する
      println("  add DWORD PTR [rdi], 8");

      // overflow_arg_areaから引数を取得した後はここに飛ぶ
      println(".Luse_reg_end%03d:", c);

      return;
    }

    if (strcmp(n->funcname, "__builtin_va_end") == 0) {
      println("  # __builtin_va_end");
      println("  # no operation is needed for __builtin_va_end");
      return;
    }


    /*
      sum(a1, a2, a3, a4, a5, a6, a7, a8);
      引数の順番(n->args)は、a1->a2->a3->...->a8
      a7, a8は, stackに残るがその際にright-to-leftでpushする必要がある

      stack
      +------------------------------
      | ~~~
      +------------------------------
      | a8
      | 7fff ffff ffff ffff 
      +------------------------------
      | a7
      | 6fff ffff ffff ffff
      +------------------------------
      | rsp
      +------------------------------
      | ~~~
      +------------------------------
      | 0x 0000 0000 0000 0000
      +------------------------------

      ND_FUNC終了時にgp(general-purpose register)に引数をセットする必要がある
      gen_expr();はgpを利用するため、引数をひとつずつGPRにセットすることができない
      そのため、すべてのgen_expr();結果をstackにpushししておき最後にgpにpopする必要がある
    */

    // Count number of arguments
    int nargs = 0;
    for (Node *arg = n->args; arg; arg = arg->next) {
      nargs++;
    }

    // Load up to max 6 arguments into registers
    int num_reg_args = nargs > 6 ? 6 : nargs;
    int num_stack_args = nargs > 6 ? nargs - 6 : 0;

    // 7以上の引数がある場合連続して次のcalleeをstackに積まないといけないためこのタイミングでalignmentを考慮してrspを調整する必要がある
    bool is_padding = (depth + num_stack_args) % 2 == 0 ? false : true;
    if (is_padding) {
      // call命令の前に、スタックのalignmentを16byteにするために8byte分rspを減らす必要がある
      println("  sub rsp, 8");
    }

    // Push arguments onto stack in reverse order
    push_stacked(n->args);

    // Load arguments from stack into registers
    for (int i = 0; i < num_reg_args; i++) {
      pop(argreg64[i]);
    }

    println("  mov rax, 0");
    println("  call %s", n->funcname);

    // 呼び出し前のalignmentをcall後に元に戻す
    if (is_padding) {
      println("  add rsp, 8");
    }

    // スタックにpushした引数の分だけrspが増えているため、呼び出し後にrspを元に戻す必要がある
    println("  add rsp, %d", num_stack_args * 8);

    // スタックにpushした引数の分だけdepthが増えているため全体で調整する
    depth -= num_stack_args;

    /*
      関数の戻り値はraxに入るが下記のタイプでは上位32bitにゴミが残っている可能性があるため適宜処理する。
      TY_BOOLは符号なしのため残りビットは0で初期化
      TY_CHARは符号拡張
      TY_SHORTは符号拡張
      また、eaxにmovすることで、raxの上位32ビットは0で初期化される
    */
    switch(n->ty->size) {
    case TY_BOOL:
      println("  movzx eax, al");
      return;
    case TY_CHAR:
      if (n->ty->is_unsigned) {
        println("  movzx eax, al");
      } else {
        println("  movsx eax, al");
      }
      return;
    case TY_SHORT:
      if (n->ty->is_unsigned) {
        println("  movzx eax, ax");
      } else {
        println("  movsx eax, ax");
      }
      return;
    }

    return;
  }
  case ND_CAST:
    gen_expr(n->lhs);
    // cast(from, to);
    cast(n->lhs->ty, n->ty);
    return;
  case ND_ASSIGN:
    gen_addr(n->lhs);
    push();
    gen_expr(n->rhs);
    push();

    store(n->ty);

    return;
  case ND_MEMZERO:
    println("  mov rcx, %d", n->var->ty->size); //size分repしstosbを繰り返す
    println("  lea rdi, [rbp - %d]", n->var->offset); //offsetの場所から書き込む
    println("  mov al, 0"); //alの値をrdiにコピーする
    println("  cld"); //dfを0にし、repをインクリメントする設定にする
    println("  rep stosb"); //rdiの位置にalの値を書き込み、rdiをインクリメントし、それをrcx回繰り返す

    return;
  case ND_COND: {
    int c = count();

    gen_expr(n->cond);
    println("  cmp rax, 0");
    println("  je .Lelse%03d", c);
    gen_expr(n->then);
    println("  jmp .Lend%03d", c);

    println(".Lelse%03d:", c);
    gen_expr(n->els);
    println(".Lend%03d:", c);

    return;
  }
  case ND_LOGICALOR: {
    int c = count();
    // 左辺の評価 0でなければ、右辺を見ずにtrueとして終了
    gen_expr(n->lhs);
    println("  cmp rax, 0");
    println("  jne .Ltrue.%d", c);

    gen_expr(n->rhs);
    println("  cmp rax, 0");
    println("  jne .Ltrue.%d", c);
    println("  mov rax, 0");
    println("  jmp .Lend.%d", c);
    println(".Ltrue.%d:", c);
    println("  mov rax, 1");
    println(".Lend.%d:", c);
    return;
  }
  case ND_LOGICALAND: {
    int c = count();
    // 左辺の評価 0であれば、右辺を見ずにfalseとして終了
    gen_expr(n->lhs);
    println("  cmp rax, 0");
    println("  je .Lfalse.%d", c);

    gen_expr(n->rhs);
    println("  cmp rax, 0");
    println("  je .Lfalse.%d", c);
    println("  mov rax, 1");
    println("  jmp .Lend.%d", c);
    println(".Lfalse.%d:", c);
    println("  mov rax, 0");
    println(".Lend.%d:", c);
    return;
  }
  case ND_COMMA:
    gen_expr(n->lhs);
    gen_expr(n->rhs);

    return;
  case ND_DEREF:
    gen_expr(n->lhs);
    load(n->ty);

    return;
  case ND_ADDR:
    gen_addr(n->lhs);

    return;
  case ND_NOT:
    gen_expr(n->lhs);
    println("  cmp rax, 0");

    println("  sete al");
    println("  movzb rax, al");

    return;
  case ND_BITNOT:
    gen_expr(n->lhs);
    println("  not rax");

    return;
  default:
    break;
  }

  gen_expr(n->lhs);
  push();
  gen_expr(n->rhs);
  push();

  pop("rdi");
  pop("rax");

  /*
    [32bit除算]
    edx:eax / r/m32 → eax = 商, edx = 余り
    [64bit除算]
    rdx:rax / r/m64 → rax = 商, rdx = 余り
  */
  char *ax, *di, *dx;
  if (n->lhs->ty->kind == TY_LONG || n->lhs->ty->base) {
    ax = "rax";
    di = "rdi";
    dx = "rdx";
  } else {
    ax = "eax";
    di = "edi";
    dx = "edx";
  }

  switch(n->kind) {
  // レジスタでシフト量を指定するためにはCLレジスタしか使えない
  // RCX, ECXレジスタは、シフト演算時には使わない
  case ND_SHL:
    println("  mov rcx, rdi");
    println("  shl %s, cl", ax);
    break;
  case ND_SHR:
    println("  mov rcx, rdi");
    if (n->lhs->ty->is_unsigned) {
      println("  shr %s, cl", ax);
    } else {
      println("  sar %s, cl", ax);
    }
    break;
  case ND_ADD:
    println("  add %s, %s", ax, di);
    break;
  case ND_SUB:
    println("  sub %s, %s", ax, di);
    break;
  case ND_MUL:
    println("  imul %s, %s", ax, di);
    break;
  case ND_DIV:
  case ND_MOD:
    if (n->ty->is_unsigned) {
      println("  mov %s, 0", dx);
      println("  div %s", di);
    } else {
      if (n->lhs->ty->size == 8) {
        println("  cqo");
      } else {
        println("  cdq");
      }

      println("  idiv %s", di);
    }

    if (n->kind == ND_MOD) {
      println("  mov %s, %s", ax, dx);
    }

    break;
  case ND_OR:
    println("  or %s, %s", ax, di);
    break;
  case ND_XOR:
    println("  xor %s, %s", ax, di);
    break;
  case ND_AND:
    println("  and %s, %s", ax, di);
    break;
  case ND_EQ:
  case ND_NEQ:
  case ND_LT:
  case ND_LE:
    println("  cmp %s, %s", ax, di);

    if (n->kind == ND_EQ) {
      println("  sete al");
    } else if (n->kind == ND_NEQ) {
      println("  setne al");
    } else if (n->kind == ND_LT) {
      if (n->lhs->ty->is_unsigned) {
        println("  setb al");
      } else {
        println("  setl al");
      }
    } else if (n->kind == ND_LE) {
      if (n->lhs->ty->is_unsigned) {
        println("  setbe al");
      } else {
        println("  setle al");
      }
    }

    println("  movzb %s, al", ax);
    break;
  default:
    break;
  }

}

static void gen_stmt(Node *n) {
  if (n->token->line != current_line) {
    current_line = n->token->line;
    println("  .loc 1 %d", n->token->line);
  }

  int c;
  switch (n->kind) {
  case ND_BLOCK:
    for (Node *nb = n->body; nb; nb = nb->next) {
      gen_stmt(nb);
    }

    return;
  case ND_GOTO:
    println("  jmp %s", n->unique_label);
    return;
  case ND_LABEL:
    println("%s:", n->unique_label);
    gen_stmt(n->lhs);
    return;
  case ND_EXPR_STMT:
    // statement to expression
    gen_expr(n->lhs);
    return;
  case ND_RETURN:
    if (n->lhs) {
      gen_expr(n->lhs);
    }
    println("  jmp .Lreturn.%s", current_fn->name);

    return;
  case ND_IF:
    c = count();

    gen_expr(n->cond);
    println("  cmp rax, 0");
    println("  je .Lelse%03d", c);
    gen_stmt(n->then);
    println("  jmp .Lend%03d", c);

    println(".Lelse%03d:", c);
    if (n->els) {
      gen_stmt(n->els);
    }
    println(".Lend%03d:", c);

    return;
  case ND_FOR:
    c = count();

    if (n->init) {
      gen_stmt(n->init);
    }
    println(".Lbegin%03d:", c);

    if (n->cond) {
      gen_expr(n->cond);
    }
    println("  cmp rax, 0");
    println("  je %s", n->break_label);
    gen_stmt(n->then);
    println("%s:", n->continue_label);
    if (n->inc) {
      gen_expr(n->inc);
    }
    println("  jmp .Lbegin%03d", c);
    println("%s:", n->break_label);

    return;
  case ND_DO:
    c = count();

    println(".Lbegin%03d:", c);
    gen_stmt(n->then);
    println("%s:", n->continue_label);

    gen_expr(n->cond);
    println("  cmp rax, 0");
    println("  jne .Lbegin%03d", c);
    println("%s:", n->break_label);

    return;
  case ND_SWITCH:
    gen_expr(n->cond);

    char *reg = n->cond->ty->size == 8 ? "rax" : "eax";
    for (Node *c = n->case_next; c; c = c->case_next) {
      println("  cmp %s, %ld", reg, c->val);
      println("  je %s", c->unique_label);
    }

    if (n->default_case) {
      println("  jmp %s", n->default_case->unique_label); 
    }

    println("  jmp %s", n->break_label);

    gen_stmt(n->then);
    println("%s:", n->break_label);

    return;
  case ND_CASE:
    println("%s:", n->unique_label);
    gen_stmt(n->lhs);

    return;
  default:
    break;
  }

  gen_expr(n);
}

/*
  n以上の最小のalignの倍数を返す 
  例:
  0, 4 -> 0
  0, 8 -> 0
  1, 8 -> 8
  11, 8 -> 16
  17, 8 -> 24
*/
int align_to(int n, int align) {
  return n % align == 0 ? n : ((n / align + 1) * align);
}

/*
  新しい変数をリストの先頭に追加
  new --> ~~~ -> second -> first
  先頭アドレスはlocals
  align_stack_size()でoffsetをリストの最初から加算していくので、stack上は新しい変数ほどアドレスは大きい

  stack
  +------------------------------
  | rbp
  | 7fff ffff ffff ffff 
  +------------------------------
  | [rbp - new->offset]    (offset = 8 の場合)
  | 6fff ffff ffff ffff
  +------------------------------
  | ~~~
  +------------------------------
  | [rbp - (first->offset)]
  +------------------------------
  | ~~~
  +------------------------------
  | rsp
  +------------------------------
  | ~~~
  +------------------------------
  | 0x 0000 0000 0000 0000
  +------------------------------

  例:
  int exp(int a, int b, int c) {
     int d, e, f;
     return a + b + c + d + e + f;
  };

  stack_sizeは、functionのparamsも含む 
  リストは下記となる
  f -> e -> d -> c -> a -> b -> c
  params部分は、順番が逆としている
*/
static void align_stack_size(Obj *prog) {
  int offset;
  for (Obj *fn = prog; fn; fn = fn->next) {
    offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      /*
        例:
        int a; char b;
        locals = b --> a
        offset = 0 --> offset = 1(b->offset) --> offset = 1 + 4 --> align_to(5, 4) = 8(a->offset)
        char a; int b;
        offset = 0 --> offset = 4(b->offset) --> offset = 4 + 1 --> align_to(5, 1) = 5(a->offset)
      */
      offset = align_to(offset, var->align);
      var->offset = offset;
    }

    /*
      x86-64 ABIの要件としてfunctionのスタックフレーム境界が16byteであることを保証するために、stack_sizeを16の倍数にする
      https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf
      3.2.2 The Stack Frame
    */
    fn->stack_size = align_to(offset, 16);
  }
}

static void emit_data(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function || !var->is_definition) {
      continue;
    }

    if (var->is_static) {
      println("  .local %s", var->name);
    } else {
      println("  .global %s", var->name);
    }

    if (var->init_data) {
      println("  .data");
      /*
        .alignディレクティブを使うことで型ごとの適切なデータ配置を行う(CPUによってはアライメントが必要)
      */
      println("  .align %d", var->align);
      println("%s:", var->name);

      Relocation *rel = var->rel;
      int pos = 0;
      while (pos < var->ty->size) {
        if (rel && rel->offset == pos) {
          // そのラベルに8byteデータ配置の場合は.quad
          println("  .quad %s%+ld", rel->label, rel->addend);
          rel = rel->next;
          pos += 8;
        } else {
          // そのラベルに1byteデータ配置の場合は.byte
          println("  .byte %d", var->init_data[pos++]);
        }
      }
      continue;
    }

    /*
      初期化されていない変数は.bssに配置
      実際の0埋めのデータが実行ファイルには含まれないため実行ファイルサイズを小さくできる
    */
    println("  .bss");
    println("  .align %d", var->align);
    println("%s:", var->name);
    println("  .zero %d", var->ty->size);
  }

}

static void emit_text(Obj *prog) {
  println(".intel_syntax noprefix");

  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition) {
      continue;
    }

    current_fn = fn;
    if (fn->is_static) {
      println("  .local %s", fn->name);
    } else {
      println("  .global %s", fn->name);
    }

    println("  .text ");
    println("%s:", fn->name);

    //prologue
    println("  push rbp");
    println("  mov rbp, rsp");
    println("  sub rsp, %d", fn->stack_size);

    /*
      va_areaはlocalsに含まれており、align_stack_size()でoffsetが設定されている
      fn->paramsは関数の引数リストでlocals全体を含んでいないため、`...`の前の引数の個数を数えることができる

      paramsのリストは引数指定順としている
      func(a, b, c, d, e, f)
      a -> b -> c -> d -> e -> f

      スタック例：

      sum(1, 2, 3, 4, 5, 6, 7, 8);

      int sum(int a, int b, ...) {
        va_list ap;
        va_start(ap, b);            --> 可変引数の直前の引数の場所を指定(32bit時代の名残であり利用しない)
        int p3 = va_arg(ap, int);   --> apが指している次の引数を取得
        int p4 = va_arg(ap, int);
        int p5 = va_arg(ap, int);
        int p6 = va_arg(ap, int);
        int p7 = va_arg(ap, int);
        int p8 = va_arg(ap, int);

        return a + b + p3 + p4 + p5 + p6 + p7 + p8;
      }

     [stack by 8 byte align]

      | caller function
      +------------------------------
      | p8 (= 8)                      ; [rbp + 24]
      +------------------------------
      | p7 (= 7)                      ; [rbp + 16]
      +------------------------------ 
      | return address <--- caller rsp
      +------------------------------
      | old RBP <--- caller rbp       ; [rbp' + 0]
      +------------------------------ <-- ★ ここから下は callee が sub rsp で確保した領域

      | local vars (例: spill/padding/c等)
      +------------------------------  calleeが作る退避領域 (va_area(24byte), reg_save_area(176byte)の GP, FP合わせて確保)
      | ...  xmm registers save area
      | +48  saved R9  (p6=6)
      | +40  saved R8  (p5=5)
      | +32  saved RCX (p4=4)
      | +16  saved RDX (p3=3)
      | +8   saved RSI (b=2)
      | +0   saved RDI (a=1)
      ...
      +------------------------------ <-- reg_save_areaの先頭アドレス
      | va_elem (va_list本体, 24Byte)
      |  - reg_save_area
      |  - overflow_arg_area
      |  - gp_offset, fp_offset
      +------------------------------ <-- va_elem->offset
      | (padding / align)
      +------------------------------
      | xxx <--- rsp
      +------------------------------
      | ~~~
      +------------------------------
    */


    if (fn->va_area) {
      int gp = 0;
      for (Obj *var = fn->params; var; var = var->next) {
        gp++;
      }
      gp = MIN(gp, 6); // 最大6個までレジスタ渡し可能

      int offset = fn->va_area->offset;

      /*
        gp(general-purpose register) -> 汎用レジスタ
        fp(floating-point register) -> 浮動小数点レジスタ

        va_elemの定義
        可変長引数の関数が作成された際にva_list用の領域を設定してある
        その領域に、アセンブリ側で値をセットする
        struct {
          unsigned int gp_offset; // 汎用レジスタをどこまで読んだかを指す。最大6 * 8byte , つまり可変長引数として最初どこを読むかを指している
          unsigned int fp_offset; // 浮動小数点レジスタをどこまで読んだかを指す
          void *overflow_arg_area; //レジスタに入りきらなかった「7番目以降の引数」が置かれているスタック上の位置を指すポインタ
          void *reg_save_area; //関数の最初(プロローグ)で、引数レジスタ*rdi, rsi, rdx, …, xmm0…xmm7)の内容を一時的にメモリにコピーしておく領域のアドレス。
        } va_elem;
      */
      // gp_offset
      println("  mov DWORD PTR [rbp - %d], %d", offset, gp * 8);
      // fp_offset
      println("  mov DWORD PTR [rbp - %d + 4], 48", offset);
      // overflow_arg_area
      println("  lea rax, [rbp + 16]");
      println("  mov QWORD PTR [rbp - %d + 8], rax", offset);
      // reg_save_area
      //println("  lea rax, [rbp - %d + 24]", offset); // reg_save_areaの先頭アドレス, 24byteはva_elemのサイズ
      println("  lea rax, [rbp - %d]", fn->reg_save_area->offset); // reg_save_areaの先頭アドレス
      println("  mov QWORD PTR [rbp - %d + 16], rax", offset);

      /*
       reg_save_areaにレジスタの値を保存
       reg_save_area は reg_save_area 領域のベース（最小アドレス）を指し、そこから +offset で上（高アドレス）方向に並べる。va_elem の直後に配置している。
      */
      println("  mov QWORD PTR [rax + 0], rdi"); // RDI
      println("  mov QWORD PTR [rax + 8], rsi"); // RSI
      println("  mov QWORD PTR [rax + 16], rdx"); // RDX
      println("  mov QWORD PTR [rax + 24], rcx"); // RCX
      println("  mov QWORD PTR [rax + 32], r8"); // R8
      println("  mov QWORD PTR [rax + 40], r9"); // R9



    }

    // レジスタに渡された関数の引数をスタックに保存
    /*
      paramsのリストは引数指定順
      func(a, b, c, d, e, f)
      a -> b -> c -> d -> e -> f
    */
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next) {
      switch (var->ty->size) {
      case 1:
        println("  mov [rbp - %d], %s", var->offset, argreg8[i++]);
        break;
      case 2:
        println("  mov [rbp - %d], %s", var->offset, argreg16[i++]);
        break;
      case 4:
        println("  mov [rbp - %d], %s", var->offset, argreg32[i++]);
        break;
      default:
        println("  mov [rbp - %d], %s", var->offset, argreg64[i++]);
        break;
      }
    }

    gen_stmt(fn->body);
    assert(depth == 0);

    //epilogue
    println(".Lreturn.%s:", fn->name);
    println("  mov rsp, rbp");
    println("  pop rbp");
    println("  ret");
  }
}

void codegen(Obj *prog, FILE *outfile) {
  out = outfile;

  align_stack_size(prog);
  emit_data(prog);
  emit_text(prog);
}


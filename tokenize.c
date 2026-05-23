#include "9cc.h"
char *user_input;
char *infile;

// errorは、エラーを報告するための関数。printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

// error_atは、エラー箇所を指し示すための関数
void error_at(char *loc, char *fmt, ...) {
  char *start = loc;
  while (user_input < start && start[-1] != '\n') {
    start--;
  }

  char *end = loc;
  while (*end != '\n') {
    end++;
  }

  int line_num = 1;
  for(char *p = user_input; p < start; p++) {
    if (*p == '\n') {
      line_num++;
    }
  }

  va_list ap;
  va_start(ap, fmt);

  int indent = fprintf(stderr, "%s:%d: ", infile, line_num);
  fprintf(stderr, "%.*s\n", (int)(end - start), start);

  int pos = loc - start + indent;
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

static Token *new_token(TokenKind kind, char *start, char *end) {
  Token *t = calloc(1, sizeof(Token));
  t->kind = kind;
  t->loc = start;
  t->len = end - start;
  return t;
}

static int is_ident1(char p) {
  return (
    ('a' <= p && p <= 'z') || 
    ('A' <= p && p <= 'Z') || 
    '_' == p
  );
}

static int is_ident2(char p) {
  return (is_ident1(p) || ('0' <= p && p <= '9'));
}

int equal(Token *t, char *key) {
  return strncmp(t->loc, key, t->len) == 0 && key[t->len] == '\0';
}

Token *skip(Token *t, char *op) {
  if (!equal(t, op))
    error_at(t->loc, "expected %s", op);
  return t->next;
}

static int keyword_len(char *p) {
  char *key[] = {"return", "if", "else", "for", "while", "do",
                "_Bool", "void", "char", "short", "int", "long",
                "struct", "union", "enum", "_Alignas","signed","unsigned",
                "const", "volatile", "auto", "register", "restrict",
                "__restrict", "__restrict__", "__Noreturn",
                "sizeof", "_Alignof",
                "typedef", "static", "extern",
                "goto", "break", "continue", "switch", "case", "default"
                };
  int key_len;
  for (int i = 0; i < sizeof(key) / sizeof(*key); i++) {
    key_len = strlen(key[i]);
    if (strncmp(key[i], p, key_len) == 0 && ! is_ident2(p[key_len])) {
      return key_len;
    }
  }
  return 0;
}

static char *string_literal_end(char *p) {
  while (*p != '"') {
    if (*p == '\n' || *p == '\0') {
      error_at(p, "unclosed string literal");
    }

    // skip escaped double qoute
    if (*p == '\\') {
      p++;
    }

    p++;
  }

  return p;
}

static int from_hex(char *p) {
  if ('0' <= *p && *p <= '9') {
    return *p - '0';
  } 

  if ('a' <= *p && *p <= 'f') {
    return *p - 'a' + 10;
  }

  if ('A' <= *p && *p <= 'F') {
    return *p - 'A' + 10;
  }

  error_at(p, "invalid hex escape sequence");

  return 0;
}

static int read_escaped_char(char **new_pos, char *p) {
  // hex escape sequence
  if (*p == 'x') {
    p++;
    if (!isxdigit(*p)) {
      error_at(p, "invalid hex escape sequence");
    }

    int c = 0;
    for (; isxdigit(*p); p++) {
      c = c * 16 + from_hex(p);
    }

    *new_pos = p;
    return c;
  }

  // octal escape sequence
  if ('0' <= *p && *p <= '7') {
    int c = *p++ - '0';
    if ('0' <= *p && *p <= '7') {
      c = c*8 + (*p++ - '0');
      if ('0' <= *p && *p <= '7') {
        c = c*8 + (*p++ - '0');
      }
    }

    *new_pos = p;
    return c;
  }

  *new_pos = p + 1;

  switch (*p) {
    case 'a':
      return '\a';
    case 'b':
      return '\b';
    case 't':
      return '\t';
    case 'n':
      return '\n';
    case 'v':
      return '\v';
    case 'f':
      return '\f';
    case 'r':
      return '\r';
    default:
      return *p;
  }
}

static Token *read_char_literal(char *p) {
  // start -> '
  char *start = p++;
  char *end;

  char c;
  if (*p == '\\') {
    c = read_escaped_char(&p, ++p);
  } else {
    c = *p;
    p++;
  }

  if (*p != '\'') {
    error_at(p, "unclosed char literal");
  }
  // end -> '
  end = p;

  Token *t = new_token(TK_NUM, start, end + 1);
  t->val = c;
  t->ty = cp_type(ty_int);
  
  return t;
}

static Token *read_int_literal(char *p) {
  char *start = p;

  // Read a binary, octal, decimal or hexadecimal integer literal and return a token.
  int base = 10;
  if (strncasecmp(p, "0x", 2) == 0 && isalnum(p[2])) {
    p += 2;
    base = 16;
  } else if (strncasecmp(p, "0b", 2) == 0 && (p[2] == '0' || p[2] == '1')) {
    p += 2;
    base = 2;
  } else if (*p == '0' && isdigit(p[1])) {
    p += 1;
    base = 8;
  } else {
    base = 10;
  }

  uint64_t val = strtoull(p, &p, base);


  /*
    Read U, L or LL suffixes.

    u/U -> unsigned
    l/L -> long
    ll/LL -> long long
  */

  bool u = false;
  bool l = false;
  bool ll = false;


  if (strncmp(p, "ull", 3) == 0 || strncmp(p, "llu", 3) == 0
      || strncmp(p, "ULL", 3) == 0 || strncmp(p, "LLU", 3) == 0
      || strncmp(p, "uLL", 3) == 0 || strncmp(p, "LLu", 3) == 0
      || strncmp(p, "Ull", 3) == 0 || strncmp(p, "llU", 3) == 0) {
    u = true;
    l = true;
    p += 3;
  } else if (strncmp(p, "ll", 2) == 0 || strncmp(p, "LL", 2) == 0) {
    l = true;
    p += 2;
  } else if (strncasecmp(p, "ul", 2) == 0 || strncasecmp(p, "lu", 2) == 0) {
    u = true;
    l = true;
    p += 2;
  } else if (strncasecmp(p, "u", 1) == 0) {
    u = true;
    p += 1;
  } else if (strncasecmp(p, "l", 1) == 0) {
    l = true;
    p += 1;
  }

  if (isalnum(*p)) {
    error_at(p, "invalid digit");
  }

  /*
  * Infer the type of an integer literal based on its value and suffixes.
  *
  * [Decimal (base 10) without suffix]
  *   - int -> long -> long long
  *   - only signed types are considered
  *
  * [Non-decimal (base 2/8/16) without suffix]
  *   - int -> unsigned int -> long -> unsigned long
  *     -> long long -> unsigned long long
  *   - both signed and unsigned types are considered
  *
  * Note:
  *   - Integer literals are parsed as numeric values, not as signed bit patterns.
  *     (e.g., 0xffffffff represents 4294967295, not -1)
  *   - The first type that can represent the value is chosen.
  *   - Suffixes (U, L, LL) restrict the candidate types.
  */
  Type *ty;
  if (base == 10) {
    // In this compiler, long long is treated as long.
    // If the value does not fit in signed long, use unsigned long.
    /*
      10進整数リテラル

                    u なし (signed only)     u あり
                   ┌──────────────────┬──────────────────┐
      L/LL なし     │  123             │  123u            │
                   │  int             │  unsigned int    │
                   │   → long         │   → unsigned long│
                   │   → long long    │                  │
                   ├──────────────────┼──────────────────┤
      L/LL あり     │  123L / 123LL    │  123UL / 123ULL  │
                   │  long            │  unsigned long   │
                   │   → long long    │                  │
                   └──────────────────┴──────────────────┘
    */
    if ((l || ll) && u) {
      ty = cp_type(ty_ulong);
    } else if (l || ll) {
      ty = cp_type(ty_long);
    } else if (u) {
        ty = val > 0xffffffff ? cp_type(ty_ulong) : cp_type(ty_uint);
    } else {
        ty = val > 0x7fffffff ? cp_type(ty_long) : cp_type(ty_int);
    }
  } else {
    if ((l || ll) && u) {
      ty = cp_type(ty_ulong);
    } else if (l || ll) {
      ty = val > 0x7fffffffffffffff ? cp_type(ty_ulong) : cp_type(ty_long);
    } else if (u) {
        ty = val > 0xffffffff ? cp_type(ty_ulong) : cp_type(ty_uint);
    } else if (val <= 0x7fffffff) {
      ty = cp_type(ty_int);
    } else {
      if (val > 0x7ffffffffffffff) {
        ty = cp_type(ty_ulong);
      } else if (val > 0xffffffff) {
        ty = cp_type(ty_long);
      } else if (val > 0x7fffffff) {
        ty = cp_type(ty_uint);
      } else {
        ty = cp_type(ty_int);
      }
    }
  }

  Token *t = new_token(TK_NUM, start, p);
  t->val = val;
  t->ty = ty;
  return t;
}

/*
  start -> 最初の"
  end -> 最後の"
*/
static Token *read_string_literal(char *start) {
  char *end = string_literal_end(start + 1);
  // callocの際に終端文字の0を含めている
  char *buf = calloc(1, end - start);

  int len = 0;
  for (char *p = start + 1; p < end;) {
    if (*p == '\\') {
      buf[len++] = read_escaped_char(&p, ++p);
    } else {
      buf[len++] = *p++;
    }
  }

  Token *t = new_token(TK_STR, start, end + 1);
  t->ty = ty_array(cp_type(ty_char), len + 1);
  t->str = buf;

  return t;
}

static void add_lines(Token *token) {
  int line_num = 1;
  char *p = user_input;

  while (*p) {
    if (p == token->loc) {
      token->line = line_num;
      token = token->next;
    }

    if (*p == '\n') {
      line_num++;
    }

    p++;
  }
}

static int read_punct(char *p) {
  char *key[] = {
                  "<<=", ">>=", "==", "!=", "<=",
                  ">=", "->", "+=", "-=", "*=",
                  "/=", "%=", "++", "--", "|=",
                  "^=", "&=", "&&", "||", "<<",
                  ">>", "..."
                };
  for (int i = 0; i < sizeof(key) / sizeof(*key); i++) {
    if (strncmp(p, key[i], strlen(key[i])) == 0) {
      return strlen(key[i]);
    }
  }

  return ispunct(*p) ? 1 : 0;
}

Token *tokenize(char *p, char *file) {
  user_input = p;
  infile = file;

  Token head;
  head.next = NULL;
  Token *cur = &head;

  char *start;

  while (*p) {
    //skip line comments
    if (strncmp(p, "//", 2) == 0) {
      p += 2;
      while (*p != '\n') {
        p++;
      }
      continue;
    }

    //skip block comments
    if (strncmp(p, "/*", 2) == 0) {
      char *q = strstr(p + 2, "*/");
      if (!q) {
        error_at(p, "unclosed block comment");
      }
      p = q + 2;
      continue;
    }

    if (isspace(*p)) {
      p++;
      continue;
    }

    if (isdigit(*p)) {
      cur = cur->next = read_int_literal(p);
      p += cur->len;
      continue;
    }

    //string literal
    if (*p == '"') {
      cur = cur->next = read_string_literal(p);
      p += cur->len;
      continue;
    }

    //char literal
    if (*p == '\'') {
      cur = cur->next = read_char_literal(p);
      p += cur->len;
      continue;
    }

    //KEYWORDS
    int kl = keyword_len(p);
    if (kl) {
      cur = cur->next = new_token(TK_KEYWORD, p, p + kl);
      p += kl;
      continue;
    }

    if (is_ident1(*p)) {
      start = p;
      p++;

      while (is_ident2(*p)) {
        p++;
      }

      cur = cur->next = new_token(TK_IDENT, start, p);
      continue;
    }

    /* Punctuator:
    * Read C punctuator tokens.
    * Examples: + - * / == != <= >= -> ; , ( ) { } [ ]
    * Derived from "punctuation".
    */
    int pl = read_punct(p);
    if (pl) {
      cur = cur->next = new_token(TK_PUNCT, p, p + pl);
      p += pl;
      continue;
    }
    
    error_at(p, "%s", "can't tokenize");
  }

  cur = cur->next = new_token(TK_EOF, p, p);
  add_lines(head.next);
  return head.next;

}

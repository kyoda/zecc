# 🧠 Compiler Implementation Reference

自作Cコンパイラ開発中によく忘れる・混乱しやすい内容をまとめたメモ。
対象は主に x86-64 / System V ABI / Intel記法 / AT&T記法 / C言語。

---

# 1. Compiler Concepts

## 用語

| 分類    | 用語                         | 意味 / 役割              | 例                                     | 備考               |
| ----- | -------------------------- | -------------------- | ------------------------------------- | ---------------- |
| 字句解析  | Tokenizer                  | ソースコード文字列をトークン列へ分割する | `"a + 3"` → `IDENT(a), '+', NUM(3)`   | lexer            |
| 構文解析  | Syntax Tree                | プログラム構造を木で表現したもの     | `(1 + 2) * 3` の木                      | CST / AST を含む広義  |
| 構文解析  | CST (Concrete Syntax Tree) | 文法をそのまま反映した構文木       | 括弧や文法ノードも保持                           | パース結果に近い         |
| 構文解析  | AST (Abstract Syntax Tree) | 不要な文法情報を省いた構文木       | `(+ 1 (* 2 3))`                       | コンパイラ内部で利用       |
| 文法    | Production Rule            | 記号を別の記号列へ展開するルール     | `Expr → Expr "+" Term`                | 文法の基本単位          |
| 文法    | Chomsky Hierarchy          | 文法を計算能力で分類する体系       | Type 0 ～ Type 3                       | 下に行くほど制約が強い      |
| 文法    | CFG (Context-Free Grammar) | 左辺が単一の非終端記号である文法     | `Expr → Expr "+" Term`                | 多くの言語で利用         |
| 文法    | BNF (Backus-Naur Form)     | 文法を記述する記法            | `<expr> ::= <expr> "+" <term>`        | CFGの表現に利用        |
| 文法    | EBNF (Extended BNF)        | BNFを拡張した記法           | `<expr> = <term> { "+" <term> }`      | `{}` や `[]` が使える |
| 文法    | Terminal Symbol            | それ以上展開されない記号         | `"+"`, `"if"`, `"("`                  | tokenizerの出力に対応  |
| 文法    | Nonterminal Symbol         | 他の記号列へ展開される記号        | `<expr>`, `<stmt>`                    | 文法上のカテゴリ         |
| 構文解析  | Recursive Descent Parsing  | 関数の再帰呼び出しで文法を処理する手法  | `parse_expr()`                        | LL系パーサ           |
| 式     | Operator                   | 計算や操作を表す記号           | `+`, `-`, `*`, `/`, `&`               | 優先順位あり           |
| 式     | Operand                    | 演算の対象となる値や式          | `a`, `3`, `x + 1`                     | ASTの子ノード         |
| 式     | Dereference Operator       | ポインタの指す先の値を取得        | `*p`                                  | 単項演算子            |
| 式     | Address-of Operator        | 変数などのアドレスを取得         | `&x`                                  | lvalue → pointer |
| データ構造 | Self-referential Structure | 自分自身の型へのポインタを持つ構造体   | `struct Node { struct Node *next; };` | リスト・木で使用         |

---

# 2. x86-64 Architecture / ABI

## CPU / Register

| 用語                                       | 意味 / 役割                     | 例                                     | 備考                    |
| ---------------------------------------- | --------------------------- | ------------------------------------- | --------------------- |
| GPR (General-Purpose Register)           | 整数・ポインタ・アドレス計算などに使う汎用レジスタ   | `rax`, `rbx`, `rcx`, `rdx`            | x86-64                |
| XMM Register                             | SSE系命令で使用する128bitレジスタ       | `xmm0` ～ `xmm15`                      | `float` / `double`で使用 |
| SSE (Streaming SIMD Extensions)          | 浮動小数点演算やSIMD演算を行うx86拡張命令セット | `movss`, `movsd`, `addss`, `cvtsi2ss` | SSE2でdouble対応         |
| SIMD (Single Instruction, Multiple Data) | 1命令で複数データを同時処理する方式          | `addps`, `mulps`                      | SSE / AVXなど           |
| FLAGS                                    | 演算結果の状態を保持するCPUフラグ群         | `ZF`, `SF`, `CF`, `OF`                | 条件分岐で使用               |

### SSE / GPR の関係

```text
SSE
 ├─ 命令セット
 │    movss
 │    movsd
 │    addss
 │    cvtsi2ss
 │
 └─ 主に使用するレジスタ
      XMM0 ～ XMM15

GPR
 └─ 汎用レジスタ
      RAX, RBX, RCX, RDX, ...
```

---

## FLAGS

| Flag | 名前            | 意味                   |
| ---- | ------------- | -------------------- |
| `ZF` | Zero Flag     | 結果が0                 |
| `SF` | Sign Flag     | 結果の最上位bitが1          |
| `CF` | Carry Flag    | unsigned演算の桁上がり / 借り |
| `OF` | Overflow Flag | signed演算のオーバーフロー     |

```text
signed比較   → 主に SF / OF / ZF
unsigned比較 → 主に CF / ZF
```

---

## ABI

| 用語                                 | 意味 / 役割                      | 例                             | 備考                  |
| ---------------------------------- | ---------------------------- | ----------------------------- | ------------------- |
| ABI (Application Binary Interface) | 関数呼び出し、レジスタ使用、データ配置などのバイナリ規約 | x86-64 System V ABI           | コンパイラ・OS・ライブラリ間の互換性 |
| System V ABI                       | UNIX系で広く使われるABI              | `rdi,rsi,rdx,rcx,r8,r9` に整数引数 | Vはローマ数字の5           |

### System V x86-64 の主な整数引数レジスタ

```text
1: rdi
2: rsi
3: rdx
4: rcx
5: r8
6: r9
```

浮動小数点引数は主に、

```text
xmm0, xmm1, xmm2, ...
```

を使用する。

---

## Stack

スタックは LIFO (Last-In First-Out) 構造。

x86-64では、高アドレスから低アドレス方向へ伸びる。

```text
push → rsp が減る
pop  → rsp が増える
```

| 命令         | 意味                 | おおよその内部動作                      |
| ---------- | ------------------ | ------------------------------ |
| `push rax` | RAXをスタックへ積む        | `rsp -= 8; [rsp] = rax`        |
| `pop rax`  | スタックトップをRAXへ取り出す   | `rax = [rsp]; rsp += 8`        |
| `call foo` | 戻りアドレスを積んでfooへジャンプ | `push return_address; jmp foo` |
| `ret`      | スタックトップの戻りアドレスへ復帰  | `pop rip` 相当                   |

例:

```text
初期:
rsp = 0x7fffffffe000

push rax

結果:
rsp = 0x7fffffffdff8
```

---

# 3. x86-64 Assembly Syntax

## 3.1 Intel記法 vs AT&T記法

| 項目         | Intel記法                    | AT&T記法                   |
| ---------- | -------------------------- | ------------------------ |
| オペランド順     | `destination, source`      | `source, destination`    |
| レジスタ       | `rax`                      | `%rax`                   |
| 即値         | `10`                       | `$10`                    |
| メモリ        | `[rax]`                    | `(%rax)`                 |
| オフセット付きメモリ | `[rax + 8]`                | `8(%rax)`                |
| サイズ        | `BYTE PTR`, `DWORD PTR` など | 命令末尾の `b`, `w`, `l`, `q` |
| 例          | `mov rax, rdi`             | `mov %rdi, %rax`         |

### Operand Order

Intel:

```asm
mov rax, rdi
```

意味:

```text
rax = rdi
```

AT&T:

```asm
mov %rdi, %rax
```

意味は同じ。

```text
Intel : destination, source
AT&T  : source, destination
```

---

## Registers

Intel:

```asm
rax
eax
al
xmm0
```

AT&T:

```asm
%rax
%eax
%al
%xmm0
```

---

## Immediate Values

Intel:

```asm
mov eax, 10
```

AT&T:

```asm
mov $10, %eax
```

AT&Tでは即値に `$` を付ける。

---

## Memory Access

Intel:

```asm
mov eax, DWORD PTR [rax]
```

AT&T:

```asm
movl (%rax), %eax
```

オフセット付き:

Intel:

```asm
mov eax, DWORD PTR [rax + 8]
```

AT&T:

```asm
movl 8(%rax), %eax
```

---

## Addressing Mode

Intel:

```text
[base + index * scale + displacement]
```

例:

```asm
[rax + rcx * 4 + 8]
```

AT&T:

```text
displacement(base, index, scale)
```

例:

```asm
8(%rax,%rcx,4)
```

---

## AT&T Operand Size Suffix

| 接尾辞 | 意味           |
| --- | ------------ |
| `b` | byte = 8bit  |
| `w` | word = 16bit |
| `l` | long = 32bit |
| `q` | quad = 64bit |

例:

| 意味         | Intel                      | AT&T                |
| ---------- | -------------------------- | ------------------- |
| 8bit load  | `mov al, BYTE PTR [rax]`   | `movb (%rax), %al`  |
| 16bit load | `mov ax, WORD PTR [rax]`   | `movw (%rax), %ax`  |
| 32bit load | `mov eax, DWORD PTR [rax]` | `movl (%rax), %eax` |
| 64bit load | `mov rax, QWORD PTR [rdi]` | `movq (%rdi), %rax` |

---

# 4. x86-64 Instructions

## 4.1 Data Movement

| 命令       | 意味                | 例                  | 備考             |
| -------- | ----------------- | ------------------ | -------------- |
| `mov`    | 値をコピー             | `mov rax, rdi`     | FLAGSは変更しない    |
| `movsx`  | 符号拡張してコピー         | `movsx eax, al`    | signed         |
| `movsxd` | 32bit → 64bit符号拡張 | `movsxd rax, eax`  | `int` → `long` |
| `movzx`  | ゼロ拡張してコピー         | `movzx eax, al`    | unsigned       |
| `lea`    | 有効アドレスを計算         | `lea rax, [rbp-8]` | メモリ値を読み込まない    |

---

## 4.2 Arithmetic

| 命令     | 意味         | 例               |
| ------ | ---------- | --------------- |
| `add`  | 加算         | `add rax, rdi`  |
| `sub`  | 減算         | `sub rax, rdi`  |
| `imul` | 整数乗算       | `imul rax, rdi` |
| `idiv` | signed整数除算 | `idiv rdi`      |

---

## 4.3 Bit Operations

| 命令    | 意味      | 例              | 備考                 |
| ----- | ------- | -------------- | ------------------ |
| `and` | bit AND | `and eax, 1`   | bit抽出              |
| `or`  | bit OR  | `or rdi, rax`  | bitを立てる            |
| `xor` | bit XOR | `xor eax, eax` | 0クリアにも利用           |
| `shl` | 左シフト    | `shl rax, 1`   | ×2相当               |
| `shr` | 論理右シフト  | `shr rax, 1`   | 左を0で埋める、unsigned向け |
| `sar` | 算術右シフト  | `sar rax, 1`   | 符号bitを維持、signed向け  |

---

## 4.4 Compare / Test

### cmp

```asm
cmp rax, rdi
```

内部的には、

```text
rax - rdi
```

を計算するが、結果は保存せずFLAGSだけ更新する。

AT&T:

```asm
cmp %rdi, %rax
```

### test

```asm
test rax, rax
```

内部的には、

```text
rax & rax
```

を計算するが、結果は保存せずFLAGSだけ更新する。

AT&T:

```asm
test %rax, %rax
```

| 命令          | 内部計算    | 結果保存 |
| ----------- | ------- | ---- |
| `cmp a, b`  | `a - b` | しない  |
| `test a, b` | `a & b` | しない  |

例:

```asm
test rax, rax
je .Lzero
```

`rax == 0` なら `ZF=1`。

例:

```asm
test rax, rax
js .Llarge
```

`rax` のbit63が1なら `SF=1`。

`u64 → double` 変換では負数判定ではなく、

```text
bit63 = 1
↓
u64値が2^63以上
```

かどうかを見るために利用する。

---

## 4.5 Conditional Jump

| 命令            | 意味                   | 主な条件                 |
| ------------- | -------------------- | -------------------- |
| `je` / `jz`   | equal / zero         | `ZF=1`               |
| `jne` / `jnz` | not equal / non-zero | `ZF=0`               |
| `js`          | sign bit set         | `SF=1`               |
| `jns`         | sign bit clear       | `SF=0`               |
| `jl`          | signed `<`           | `SF != OF`           |
| `jle`         | signed `<=`          | `ZF=1` or `SF != OF` |
| `jg`          | signed `>`           | signed比較             |
| `jge`         | signed `>=`          | signed比較             |
| `jb`          | unsigned `<`         | `CF=1`               |
| `jbe`         | unsigned `<=`        | unsigned比較           |
| `ja`          | unsigned `>`         | unsigned比較           |
| `jae`         | unsigned `>=`        | unsigned比較           |
| `jmp`         | 無条件ジャンプ              | 常にジャンプ               |

---

## 4.6 Memory Operand Size

Intel記法では、メモリアクセス時にサイズが曖昧な場合にサイズ指定を行う。

| 指定子         |   サイズ | C型の例            |
| ----------- | ----: | --------------- |
| `BYTE PTR`  |  8bit | `char`          |
| `WORD PTR`  | 16bit | `short`         |
| `DWORD PTR` | 32bit | `int`           |
| `QWORD PTR` | 64bit | `long`, pointer |

例:

```asm
mov rdi, QWORD PTR [rax]
mov eax, DWORD PTR [rax]
movzx eax, BYTE PTR [rax]
```

---

## 4.7 movsx / movzx

### Sign Extension

| 変換        | Intel             | AT&T                |
| --------- | ----------------- | ------------------- |
| i8 → i16  | `movsx ax, al`    | `movsbw %al, %ax`   |
| i8 → i32  | `movsx eax, al`   | `movsbl %al, %eax`  |
| i8 → i64  | `movsx rax, al`   | `movsbq %al, %rax`  |
| i16 → i32 | `movsx eax, ax`   | `movswl %ax, %eax`  |
| i16 → i64 | `movsx rax, ax`   | `movswq %ax, %rax`  |
| i32 → i64 | `movsxd rax, eax` | `movslq %eax, %rax` |

AT&Tでは、

```text
mov s b l
    │ │ │
    │ │ └─ destination = long = 32bit
    │ └─── source = byte = 8bit
    └───── sign extension
```

のように読める。

### Zero Extension

| 変換        | Intel           | AT&T                      |
| --------- | --------------- | ------------------------- |
| u8 → u16  | `movzx ax, al`  | `movzbw %al, %ax`         |
| u8 → u32  | `movzx eax, al` | `movzbl %al, %eax`        |
| u8 → u64  | `movzx rax, al` | `movzbq %al, %rax`        |
| u16 → u32 | `movzx eax, ax` | `movzwl %ax, %eax`        |
| u16 → u64 | `movzx rax, ax` | `movzwq %ax, %rax`        |
| u32 → u64 | `mov eax, eax`  | 32bitレジスタ書込みで上位32bitが0になる |

---

# 5. Type Conversion / Cast

## 5.1 Sign Extension

| 元   | 変換先 | Intel命令           |
| --- | --- | ----------------- |
| i8  | i32 | `movsx eax, al`   |
| i16 | i32 | `movsx eax, ax`   |
| i8  | i64 | `movsx rax, al`   |
| i16 | i64 | `movsx rax, ax`   |
| i32 | i64 | `movsxd rax, eax` |

---

## 5.2 Zero Extension

| 元   | 変換先 | Intel命令         |
| --- | --- | --------------- |
| u8  | u32 | `movzx eax, al` |
| u16 | u32 | `movzx eax, ax` |
| u8  | u64 | `movzx rax, al` |
| u16 | u64 | `movzx rax, ax` |
| u32 | u64 | `mov eax, eax`  |

x86-64では32bitレジスタへ書き込むと、対応する64bitレジスタの上位32bitが0になる。

```asm
mov eax, eax
```

実行後、

```text
RAX[63:32] = 0
```

となる。

---

## 5.3 Truncation

小さい整数型へ変換する場合、必要な下位bitだけを使用すればよい。

| 元       | 変換先     | 命令 |
| ------- | ------- | -- |
| i32/u32 | i8/u8   | 不要 |
| i32/u32 | i16/u16 | 不要 |
| i64/u64 | i32/u32 | 不要 |

---

## 5.4 Same-size Cast

signed / unsigned はCPU上では型情報を持たず、同じbit列。

| 変換        | 命令 |
| --------- | -- |
| i8 ↔ u8   | なし |
| i16 ↔ u16 | なし |
| i32 ↔ u32 | なし |
| i64 ↔ u64 | なし |

CPUが保持しているのはbit列であり、signed / unsigned の解釈は使用する命令側で決まる。

---

# 6. Floating Point

## 6.1 IEEE754 Type Summary

| C Type        | Size | IEEE754            | Precision | Exponent           | Fraction | Approx. Range | Register |
| ------------- | ---: | ------------------ | --------- | ------------------ | -------- | ------------- | -------- |
| `float`       |   4B | Binary32           | 約7桁       | 8bit / Bias 127    | 23bit    | ±3.4×10^38    | XMM      |
| `double`      |   8B | Binary64           | 約15〜16桁   | 11bit / Bias 1023  | 52bit    | ±1.8×10^308   | XMM      |
| `long double` |  16B | x87 80bit Extended | 約18〜19桁   | 15bit / Bias 16383 | 64bit    | ±1.2×10^4932  | x87      |

Linux x86-64では `long double` は80bit値を16byte領域に格納する。

---

## 6.2 IEEE754 Layout

| Type          | Sign | Exponent | Fraction |
| ------------- | ---: | -------: | -------: |
| `float`       |    1 |        8 |       23 |
| `double`      |    1 |       11 |       52 |
| `long double` |    1 |       15 |       64 |

通常の正規化数では、

```text
value = (-1)^Sign × 1.Fraction × 2^(Exponent - Bias)
```

* Sign: `0` = positive, `1` = negative
* Bias: exponent格納時のオフセット
* Fraction: 仮数部
* 正規化数では先頭の `1` は暗黙的

---

## 6.3 Floating-Point Registers

整数式の結果:

```text
RAX
```

`float` / `double`式の結果:

```text
XMM0
```

例:

```c
int x = 3;
```

```text
rax = 3
```

```c
float x = 3.0f;
```

```text
xmm0 = 3.0f
```

---

## 6.4 Floating-Point Instructions

| 型             | Load / Store   | Add     | Sub     | Mul     | Div     |
| ------------- | -------------- | ------- | ------- | ------- | ------- |
| `float`       | `movss`        | `addss` | `subss` | `mulss` | `divss` |
| `double`      | `movsd`        | `addsd` | `subsd` | `mulsd` | `divsd` |
| `long double` | `fld` / `fstp` | `fadd`  | `fsub`  | `fmul`  | `fdiv`  |

```text
ss = Scalar Single-Precision = float
sd = Scalar Double-Precision = double
```

Scalar:

```text
1つの値を演算
```

Packed:

```text
複数値をSIMD演算
```

---

## 6.5 Intel / AT&T Floating-Point Instructions

| 変換・操作        | Intel                 | AT&T                     |
| ------------ | --------------------- | ------------------------ |
| float load   | `movss xmm0, [rax]`   | `movss (%rax), %xmm0`    |
| float store  | `movss [rdi], xmm0`   | `movss %xmm0, (%rdi)`    |
| double load  | `movsd xmm0, [rax]`   | `movsd (%rax), %xmm0`    |
| double store | `movsd [rdi], xmm0`   | `movsd %xmm0, (%rdi)`    |
| i32 → f32    | `cvtsi2ss xmm0, eax`  | `cvtsi2ssl %eax, %xmm0`  |
| i64 → f32    | `cvtsi2ss xmm0, rax`  | `cvtsi2ssq %rax, %xmm0`  |
| i32 → f64    | `cvtsi2sd xmm0, eax`  | `cvtsi2sdl %eax, %xmm0`  |
| i64 → f64    | `cvtsi2sd xmm0, rax`  | `cvtsi2sdq %rax, %xmm0`  |
| f32 → i32    | `cvttss2si eax, xmm0` | `cvttss2sil %xmm0, %eax` |
| f32 → i64    | `cvttss2si rax, xmm0` | `cvttss2siq %xmm0, %rax` |
| f64 → i32    | `cvttsd2si eax, xmm0` | `cvttsd2sil %xmm0, %eax` |
| f64 → i64    | `cvttsd2si rax, xmm0` | `cvttsd2siq %xmm0, %rax` |
| f32 → f64    | `cvtss2sd xmm0, xmm0` | `cvtss2sd %xmm0, %xmm0`  |
| f64 → f32    | `cvtsd2ss xmm0, xmm0` | `cvtsd2ss %xmm0, %xmm0`  |

---

## 6.6 Floating-Point Literals

| Literal         | Type          |
| --------------- | ------------- |
| `1.0f`          | `float`       |
| `1.0`           | `double`      |
| `1.0l` / `1.0L` | `long double` |

---

## 6.7 Floating-Point Cast

### Signed Integer → Floating Point

| Conversion | Intel syntax         |
| ---------- | -------------------- |
| i32 → f32  | `cvtsi2ss xmm0, eax` |
| i32 → f64  | `cvtsi2sd xmm0, eax` |
| i64 → f32  | `cvtsi2ss xmm0, rax` |
| i64 → f64  | `cvtsi2sd xmm0, rax` |

`si` = Signed Integer

---

### Floating Point → Signed Integer

| Conversion | Intel syntax          |
| ---------- | --------------------- |
| f32 → i32  | `cvttss2si eax, xmm0` |
| f32 → i64  | `cvttss2si rax, xmm0` |
| f64 → i32  | `cvttsd2si eax, xmm0` |
| f64 → i64  | `cvttsd2si rax, xmm0` |

`cvtt` の `tt` は truncation。

Cの浮動小数点 → 整数castはゼロ方向へ切り捨てる。

```text
3.9  → 3
-3.9 → -3
```

---

### float ↔ double

| Conversion | Intel syntax          |
| ---------- | --------------------- |
| f32 → f64  | `cvtss2sd xmm0, xmm0` |
| f64 → f32  | `cvtsd2ss xmm0, xmm0` |

---

## 6.8 Cast Mnemonic

```text
cvt  = convert
cvtt = convert with truncation

si = signed integer
ss = scalar single precision = float
sd = scalar double precision = double
```

例:

```text
cvtsi2ss
```

```text
Convert Signed Integer
to Scalar Single
```

つまり、

```text
signed integer → float
```

---

## 6.9 unsigned 32bit → Floating Point

`cvtsi2ss` / `cvtsi2sd` はsigned整数を変換する命令。

ただし `u32` の全範囲は `i64` に収まる。

```text
u32:
0 ～ 4,294,967,295

i64:
-9,223,372,036,854,775,808
～
 9,223,372,036,854,775,807
```

そのため、まず64bitへゼロ拡張する。

```asm
mov eax, eax
cvtsi2ss xmm0, rax
```

`u32 → f64`:

```asm
mov eax, eax
cvtsi2sd xmm0, rax
```

コメント例:

```c
// Use a signed 64-bit conversion because the entire u32 range fits in i64.
```

---

## 6.10 Floating Point → unsigned 32bit

`u32` の全範囲は `i64` に収まるため、signed 64bit整数を中継できる。

`f32 → u32`:

```asm
cvttss2si rax, xmm0
```

`f64 → u32`:

```asm
cvttsd2si rax, xmm0
```

概念:

```text
floating point
↓
signed i64
↓
下位32bitをu32として使用
```

---

## 6.11 unsigned 64bit → double

SSE2には、

```text
u64 → double
```

を直接行う命令がない。

`cvtsi2sd` は入力をsigned 64bit整数として解釈する。

そのため、

```text
0 ～ 2^63-1
```

なら直接変換できる。

```asm
cvtsi2sd xmm0, rax
```

しかし、

```text
2^63 ～ 2^64-1
```

ではbit63が1なので、同じbit列をsigned 64bitとして見ると負数になる。

そのまま `cvtsi2sd` には渡せない。

### 処理

```text
1. 元のu64値を2で割り、signed 64bitで表現できる範囲にする
2. 右シフトで失われた最下位bitを保持する
3. cvtsi2sdでdoubleへ変換する
4. 最後に2倍する
```

概念:

```c
half = (x >> 1) | (x & 1);
result = (double)half * 2;
```

アセンブリの概念:

```asm
test rax, rax
js .Llarge

cvtsi2sd xmm0, rax
jmp .Lend

.Llarge:
mov rdi, rax
and eax, 1
shr rdi, 1
or rdi, rax
cvtsi2sd xmm0, rdi
addsd xmm0, xmm0

.Lend:
```

コメント例:

```c
// cvtsi2sd accepts only signed 64-bit integers.
// Shift the value right by one so it fits in the signed range.
// Preserve the discarded low bit for correct rounding,
// then multiply the converted value by two.
```

### `test rax, rax`

```asm
test rax, rax
js .Llarge
```

ここでは負数判定そのものが目的ではない。

```text
bit63 = 1
↓
u64として見ると値が2^63以上
↓
cvtsi2sdでは直接処理できない
```

ことを判定している。

---

## 6.12 Floating-Point Precision

`float` は24bit程度の有効精度を持つため、

```text
2^24
```

までの整数を正確に表現できる。

`double` は53bit程度の有効精度を持つため、

```text
2^53
```

までの整数を正確に表現できる。

それを超えると、

```text
隣接した整数をすべて個別には表現できない
```

ため、丸めが発生する。

---

# 7. C Grammar

## Grammar Symbols

| 記号         | 意味      | 例                       |
| ---------- | ------- | ----------------------- |
| `num`      | 数値リテラル  | `123`, `1.5`            |
| `ident`    | 識別子     | 変数名、関数名                 |
| `declspec` | 型指定部分   | `int`, `char`, `struct` |
| `str`      | 文字列リテラル | `"abc"`                 |
| `ε`        | 空       | 要素なし                    |

---

## Initializer

```text
string-initializer ::= string-literal

array-initializer1 ::= "{" initializer ("," initializer)* "}"

array-initializer2 ::= initializer ("," initializer)*

struct-initializer1 ::= "{" initializer ("," initializer)* "}"

struct-initializer2 ::= initializer ("," initializer)*

union-initializer ::= "{" initializer ("," dummy-initializer)* "}"
                    | initializer

initializer ::= string-initializer
              | array-initializer
              | struct-initializer
              | union-initializer
              | assign
```

---

## Program / Declaration

```text
function ::= declspec declarator (stmt? | ";")

parse_typedef ::= declarator ";"

program ::= (declspec (parse_typedef | function | global_variable))*

struct-member ::= (declspec declarator ("," declarator)* ";")*

struct-union-decl ::= "struct" ident? ("{" struct-member "}")?

struct-decl ::= struct-union-decl

union-decl ::= struct-union-decl

enum-specifier ::= "enum" ident? "{" enum-list? "}"
                 | "enum" ident ("{" enum-list? "}")?

enum-list ::= ident ("=" num)? ("," ident ("=" num)?)*

declspec ::= ("_Bool"
            | "void"
            | "char"
            | "short"
            | "int"
            | "long"
            | struct-decl
            | union-decl
            | enum-specifier
            | "_Alignas"
            | "signed"
            | "unsigned"
            | "const"
            | "volatile"
            | "auto"
            | "register"
            | "restrict"
            | "__restrict"
            | "__restrict__"
            | "__Noreturn"
            | "typedef"
            | "static"
            | "extern"
            | typedef-name)+

declaration ::= (declarator ("=" expr)?
                 ("," declarator ("=" expr)?)*)? ";"
```

---

## Declarator

```text
pointer ::= ("*" (
              "const"
            | "volatile"
            | "auto"
            | "restrict"
            | "__restrict"
            | "__restrict__"
            | "__Noreturn"
          )*)*

declarator ::= pointer ("(" declarator ")" | ident) type-suffix

array-dimensions ::= ("static"
                    | "restrict"
                    | "__restrict"
                    | "__restrict__")*
                    const-expr? "]" type-suffix

type-suffix ::= "(" func-params ")"
              | array-dimensions
              | ε

func-params ::= "void"
              | (declspec declarator
                ("," declspec declarator)*
                ("," "...")?)?
```

---

## Statement

```text
compound-stmt ::= declspec
                  (parse_typedef
                  | function
                  | extern
                  | global_variable)*

stmt ::= compound-stmt
       | "return" expr? ";"
       | "if" "(" expr ")" stmt ("else" stmt)?
       | "while" "(" expr ")" stmt
       | "do" stmt "while" "(" expr ")" ";"
       | "for" "(" expr? ";" expr? ";" expr? ")" stmt
       | "switch" "(" expr ")" stmt
       | "case" num ":" stmt
       | "default" ":" stmt
       | "goto" ident ";"
       | "break" ";"
       | "continue" ";"
       | ident ":" stmt
       | expr-stmt

expr-stmt ::= ";"
            | expr ";"
```

---

## Expression

```text
expr ::= assign ("," expr)*

assign ::= conditional (assign-op assign)?

assign-op ::= "="
            | "+="
            | "-="
            | "*="
            | "/="
            | "%="
            | "|="
            | "^="
            | "&="

conditional ::= logicalor ("?" expr ":" conditional)?

logicalor ::= logicaland ("||" logicaland)*

logicaland ::= bitor ("&&" bitor)*

bitor ::= bitxor ("|" bitxor)*

bitxor ::= bitand ("^" bitand)*

bitand ::= equality ("&" equality)*

equality ::= relational (("==" | "!=") relational)*

relational ::= shift (("<" | "<=" | ">" | ">=") shift)*

shift ::= add (("<<" | ">>") add)*

add ::= mul (("+" | "-") mul)*

mul ::= cast (("*" | "/" | "%") cast)*
```

---

## Cast / Unary

```text
cast ::= "(" typename ")" cast
       | unary

abstract-declarator ::= pointer
                        ("(" abstract-declarator ")")?
                        type-suffix

typename ::= declspec abstract-declarator

unary ::= "sizeof" "(" typename ")"
        | "sizeof" cast
        | "_Alignof" "(" typename ")"
        | "_Alignof" cast
        | ("+" | "-" | "*" | "&" | "!") cast
        | ("++" | "--") unary
        | postfix
```

---

## Postfix / Primary

```text
postfix ::= "(" type-name ")" "{" initializer-list "}"
          | primary (
              "[" expr "]"
            | "." ident
            | "->" ident
            | "++"
            | "--"
          )*

primary ::= "(" "{" stmt+ "}" ")"
          | "(" expr ")"
          | ident ("(" assign ("," assign)* ")")?
          | str
          | num
```

---

# 8. Translation Phases Memo

Cのソースコードは概念的に複数の翻訳フェーズを通る。

重要な部分:

```text
Phase 1
  ソース文字の処理

Phase 2
  backslash + newline による行継続

Phase 3
  preprocessing tokenへの分割
  コメント処理

Phase 4
  #include
  #define
  #if / #ifdef / #ifndef
  マクロ展開

Phase 5
  文字・文字列リテラルの文字処理

Phase 6
  隣接文字列リテラルの連結

Phase 7
  Cのトークンとして構文解析・意味解析
```

隣接文字列:

```c
"abc"
"def"
```

は、

```c
"abcdef"
```

として扱う。

重要なのは、文字列連結はマクロ展開後に行うこと。

```c
#define STR "abc"

STR "def"
```

処理:

```text
STR "def"
↓ macro expansion
"abc" "def"
↓ adjacent string literal concatenation
"abcdef"
```

そのため、コンパイラ内部では概念的に、

```text
tokenize
↓
preprocess
↓
join_adjacent_string_literals
↓
parse
```

の順番にする。

翻訳フェーズと実装上の関数分割は必ずしも1対1で一致させる必要はない。
重要なのは、C規格上必要な処理順を守ること。

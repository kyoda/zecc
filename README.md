# zecc

**Zephyr C Compiler**

A small self-hosting C compiler created while studying the book *[低レイヤを知りたい人のためのCコンパイラ作成入門](https://www.sigbus.info/compilerbook)* by @rui314.

The implementation is inspired by and references parts of [chibicc](https://github.com/rui314/chibicc), a minimal C compiler written in C, but it has evolved independently with its own design and implementation.

## Overview

**zecc** is a self-hosting C compiler that implements a subset of the C language, including lexical analysis, parsing, semantic analysis, and x86-64 code generation.

It targets the System V AMD64 ABI and currently generates Intel-syntax x86-64 assembly.

## Features

- Handwritten recursive descent parser
- Supports:
  - Integer types and pointer types
  - Arithmetic and logical expressions
  - Function definitions and calls
  - Local and global variables
  - `if`, `for`, `while`, `return`, `switch`, `goto`, `break`, `continue`
  - Structs, unions, enums (partially supported)
  - Arrays and compound literals
- Generates x86-64 assembly code
- Designed for educational clarity

## Project Structure

```text
.
├── main.c             # Entry point and compiler driver
├── parse.c            # Parser and AST construction (BNF-based)
├── codegen.c          # x86-64 code generation
├── tokenize.c         # Tokenizer / lexer
├── type.c             # Type system and semantic analysis
├── test/              # Test cases for the compiler
├── Makefile           # Build system

```

## Usage Example

Compile a C file to assembly:

```sh
./zecc test/hello.c > hello.s
```

Assemble and run using GCC:

```sh
gcc -o hello hello.s
./hello
```

Or run the test suite:

```sh
make test
```

## Build

```sh
make
```

## Test

```sh
make test
```

Test using GCC :

```sh
make cctest
```

## License
This project is released under the MIT License.
It includes references to chibicc, which is also licensed under MIT.
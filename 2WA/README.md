<p align="center">
  <img width="900" height="200" alt="2WA Banner" src="https://github.com/PoTiwi/WebAssembler/blob/main/ico/2WA-bg-banner.png" />
</p>

<hr>

# 2WA

**2WA** (*short for “To Web Assembler”*) is a transpiler project that converts supported programming languages into WebAssembler instructions for a lightweight virtual machine runtime.

The project focuses on making multiple programming languages work inside a shared browser-oriented runtime environment through a common instruction system.

2WA is still heavily in development. Some language implementations already exist, while others are currently planned for future support.

---

# Supported Languages

## 🐣 In development, use with caution
- [C](#c)  
  C support is currently being developed and is not ready for production use yet.

- [C++](#c-1)  
  C++ support is currently experimental and many language features are still incomplete.

---

## 🥚 Yet to be developed
- [JavaScript](#javascript)  
  Planned JavaScript transpiler support for the 2WA runtime.

- [Python](#python)  
  Planned Python transpiler support for the 2WA runtime.

- [Lua](#lua)  
  Planned Lua transpiler support for lightweight scripting support.

- [(AArch64) Assembly](#aarch64-assembly)  
  Planned low-level AArch64 Assembly support.

---

# Languages

## C

The C implementation is one of the primary language targets currently being worked on for 2WA.

Its goal is to translate C source code into near-native WebAssembler instructions that can run inside the 2WA runtime environment. The implementation is still incomplete, so missing functionality and unexpected behavior should be expected.

Current work mainly focuses on:
- Parsing C syntax
- Translating functions into runtime instructions
- Runtime compatibility
- Memory handling and execution behavior

### Compiler
- [c2wa](https://github.com/PoTiwi/WebAssembler/tree/main/2WA/C)

---

## C++

The C++ implementation builds on top of the existing C backend while attempting to support more advanced language features and abstractions.

Because of the complexity of C++, development is slower and significantly more experimental compared to the C implementation. Many language features are currently unsupported or only partially implemented.

Current work mainly focuses on:
- Basic C++ syntax support
- Shared backend integration with the C transpiler
- Runtime instruction generation
- Early-stage object handling

### Compiler
- [cpp2wa](https://github.com/PoTiwi/WebAssembler/tree/main/2WA/C++)

---

## JavaScript

JavaScript support has not been developed yet.

The planned implementation will focus on translating JavaScript code into instructions compatible with the 2WA runtime environment while keeping execution lightweight and portable.

### Planned Compiler
- [js2wa](https://github.com/PoTiwi/WebAssembler/tree/main/2WA/JavaScript)

---

## Python

Python support has not been developed yet.

The Python implementation is planned as a future language target for the 2WA transpiler system.

### Planned Compiler
- [py2wa](https://github.com/PoTiwi/WebAssembler/tree/main/2WA/Python)

---

## Lua

Lua support has not been developed yet.

Lua is planned as a lightweight scripting language target intended for simple runtime integration and embedded scripting support.

### Planned Compiler
- [lua2wa](https://github.com/PoTiwi/WebAssembler/tree/main/2WA/Lua)

---

## AArch64 Assembly

AArch64 Assembly support has not been developed yet.

This implementation is planned to allow low-level instruction translation into WebAssembler instructions for the 2WA runtime environment.

### Planned Compiler
- [a64asm2wa](https://github.com/PoTiwi/WebAssembler/tree/main/2WA/A64Assembly)

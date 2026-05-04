<img width="900" height="200" alt="wl-banner" src="https://github.com/PoTiwi/WebAssembler/blob/main/ico/wl-bg-banner.png" /> 

A compiler that translates custom AArch64-inspired assembly source files into text-based bytecode files for use with a compatible virtual machine.

---

## Table of Contents

- [Overview](#overview)
- [Building](#building)
- [Usage](#usage)
- [Source File Format](#source-file-format)
  - [Comments](#comments)
  - [Labels](#labels)
  - [Registers](#registers)
  - [Immediates](#immediates)
  - [Strings](#strings)
- [Instruction Reference](#instruction-reference)
  - [Custom Instructions](#custom-instructions)
  - [Data Movement](#data-movement)
  - [Arithmetic](#arithmetic)
  - [Logical and Bitwise](#logical-and-bitwise)
  - [Shift and Rotate](#shift-and-rotate)
  - [Load and Store](#load-and-store)
  - [Branch and Control Flow](#branch-and-control-flow)
  - [Comparison](#comparison)
  - [Conditional Select](#conditional-select)
  - [Multiply and Divide Extended](#multiply-and-divide-extended)
  - [Bit Field Operations](#bit-field-operations)
  - [Sign and Zero Extension](#sign-and-zero-extension)
  - [Floating Point](#floating-point)
  - [Atomic and Exclusive Access](#atomic-and-exclusive-access)
  - [System Instructions](#system-instructions)
  - [Pseudo-Directives](#pseudo-directives)
  - [Stack Pseudo-Instructions](#stack-pseudo-instructions)
- [Opcode Table](#opcode-table)
- [Bytecode Output Format](#bytecode-output-format)
- [Examples](#examples)
- [Limits and Constraints](#limits-and-constraints)
- [Error Reference](#error-reference)

---

## Overview

WebAssembler reads a `.iwa` assembly source file line by line, resolves label references, and emits a `.wassm` text file containing one encoded instruction per line. The bytecode format is plain text and is designed to be parsed by a companion virtual machine.

The instruction set is modeled on AArch64 (ARM64) but includes a large set of custom high-level instructions for string manipulation, I/O, math utilities, and control flow aliases that make it suitable for interpreted runtimes.

---

## Building

The assembler is written in standard C99. Compile it with any C compiler:

```sh
gcc -O2 -o assembler assembler.c
```

Or with clang:

```sh
clang -O2 -o assembler assembler.c
```

No external dependencies are required.

---

## Usage

```sh
./assembler <input.iwa> <output.wassm>
```

**Arguments:**

| Argument | Description |
|---|---|
| `input.iwa` | Path to the assembly source file |
| `output.wassm` | Path where the bytecode output will be written |

**Example:**

```sh
./assembler hello.iwa hello.wassm
```

On success the assembler prints the number of instructions compiled and the output path:

```
Compiled 12 instruction(s) -> hello.wassm
```

On failure an error message is printed to stderr and the process exits with a non-zero status.

---

## Source File Format

### Comments

Comments begin with a semicolon (`;`) and extend to the end of the line. They may appear on their own line or after an instruction.

```asm
; This is a full-line comment
mov x0, #42        ; This is an inline comment
```

### Labels

A label is defined by writing an identifier followed immediately by a colon. Labels are resolved at compile time and replaced with the bytecode line index of the instruction that follows them.

```asm
loop:
    inc x0
    cmp x0, #10
    jlt loop
```

A label may appear on the same line as an instruction:

```asm
start: mov x0, #0
```

Label names are case-sensitive. They may contain letters, digits, underscores, and dots, but must not look like a register name or a numeric literal.

### Registers

The assembler recognizes the following register families:

| Notation | Family | Numbers | Description |
|---|---|---|---|
| `x0`-`x30` | General purpose 64-bit | 0-30 | Standard integer registers |
| `w0`-`w30` | General purpose 32-bit | 0-30 | Lower 32-bit view of x registers |
| `sp` | Stack pointer | - | Stack pointer (x31) |
| `lr` | Link register | - | Return address (x30) |
| `pc` | Program counter | - | Program counter |
| `xzr` | Zero register (64-bit) | - | Always reads as zero |
| `wzr` | Zero register (32-bit) | - | Always reads as zero |
| `fp` | Frame pointer | - | Alias for x29 |
| `ip0` | Intra-procedure scratch | - | Alias for x16 |
| `ip1` | Intra-procedure scratch | - | Alias for x17 |
| `d0`-`d31` | Float 64-bit | 0-31 | Double-precision floating point |
| `s0`-`s31` | Float 32-bit | 0-31 | Single-precision floating point |
| `q0`-`q31` | 128-bit | 0-31 | SIMD/vector 128-bit |
| `v0`-`v31` | Vector | 0-31 | Vector registers |

Register names are case-insensitive in the source.

### Immediates

Immediate values may be written in the following forms:

| Form | Example | Description |
|---|---|---|
| Decimal | `#42` | Plain decimal integer |
| Hexadecimal | `#0xFF` | Hex with `0x` or `0X` prefix |
| Without hash | `42` | Hash prefix is optional in most positions |

Negative values are supported:

```asm
mov x0, #-1
add x1, x1, #-8
```

### Strings

String literals are enclosed in double quotes and may contain the following escape sequences:

| Sequence | Character |
|---|---|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\0` | Null byte |
| `\\` | Backslash |
| `\"` | Double quote |

```asm
lds x0, "Hello, World!\n"
```

---

## Instruction Reference

Operand notation used throughout this section:

| Notation | Meaning |
|---|---|
| `Rd` | Destination register |
| `Rn` | Source register 1 |
| `Rm` | Source register 2 |
| `Ra` | Accumulator or additional source register |
| `#imm` | Immediate value |
| `label` | A label name defined elsewhere in the source |
| `[Rn]` | Memory address held in register Rn |
| `[Rn, #off]` | Memory address at Rn plus offset |
| `cond` | Condition code string (see condition codes below) |

**Condition codes:**

| Code | Meaning |
|---|---|
| `eq` | Equal |
| `ne` | Not equal |
| `lt` | Signed less than |
| `le` | Signed less than or equal |
| `gt` | Signed greater than |
| `ge` | Signed greater than or equal |
| `lo` / `cc` | Unsigned lower |
| `ls` | Unsigned lower or same |
| `hi` | Unsigned higher |
| `hs` / `cs` | Unsigned higher or same |
| `mi` | Minus / negative |
| `pl` | Plus / non-negative |
| `vs` | Overflow set |
| `vc` | Overflow clear |
| `al` | Always |

---

### Custom Instructions

These are high-level instructions added beyond the standard AArch64 set.

---

#### lds

Load a string literal into a register.

```
lds Rd, "string"
```

```asm
lds x0, "Hello, World!\n"
```

---

#### puts

Print the string held in a register to stdout.

```
puts Rn
```

```asm
puts x0
```

---

#### geti

Read an integer from stdin and store it in a register.

```
geti Rd
```

```asm
geti x0
```

---

#### gets

Read a string from stdin and store it in a register.

```
gets Rd
```

```asm
gets x0
```

---

#### dmp

Print the value of a register (debug dump).

```
dmp Rn
```

```asm
dmp x0
```

---

#### halt

Stop execution immediately.

```
halt
```

---

#### rand

Fill a register with a random integer.

```
rand Rd
```

```asm
rand x0
```

---

#### time

Fill a register with the current Unix timestamp in milliseconds.

```
time Rd
```

```asm
time x0
```

---

#### clrr

Set a register to zero.

```
clrr Rd
```

```asm
clrr x0
```

---

#### inc

Increment a register by 1.

```
inc Rd
```

```asm
inc x0
```

---

#### dec

Decrement a register by 1.

```
dec Rd
```

```asm
dec x0
```

---

#### abs

Replace a register's value with its absolute value.

```
abs Rd
```

```asm
abs x0
```

---

#### max2

Store the maximum of two source registers into a destination register.

```
max2 Rd, Rn, Rm
```

```asm
max2 x2, x0, x1
```

---

#### min2

Store the minimum of two source registers into a destination register.

```
min2 Rd, Rn, Rm
```

```asm
min2 x2, x0, x1
```

---

#### mod

Compute `Rn % Rm` and store the result in `Rd`.

```
mod Rd, Rn, Rm
```

```asm
mod x2, x0, x1
```

---

#### not

Bitwise NOT of a register in place.

```
not Rd
```

```asm
not x0
```

---

#### shl

Shift a register left by an immediate amount.

```
shl Rd, #imm
```

```asm
shl x0, #3
```

---

#### shr

Shift a register right (logical) by an immediate amount.

```
shr Rd, #imm
```

```asm
shr x0, #1
```

---

#### swp

Swap the values of two registers.

```
swp Rn, Rm
```

```asm
swp x0, x1
```

---

#### memcpy

Copy data from one register to another for a given length (all operands are registers).

```
memcpy Rd, Rn, Rm
```

---

#### memset

Fill memory with a value for a given length.

```
memset Rd, Rn, Rm
```

---

#### strcpy

Copy a string from one register to another.

```
strcpy Rd, Rn
```

```asm
strcpy x1, x0
```

---

#### strcat

Concatenate the string in `Rm` onto the string in `Rd`.

```
strcat Rd, Rm
```

```asm
strcat x0, x1
```

---

#### strcmp

Compare two strings. The result is stored in the destination register (0 if equal, non-zero otherwise).

```
strcmp Rd, Rn, Rm
```

```asm
strcmp x2, x0, x1
```

---

#### strlen

Compute the length of the string in `Rn` and store it in `Rd`. Alias for `adl`.

```
strlen Rd, Rn
```

```asm
strlen x1, x0
```

---

#### adl

Identical to `strlen`. Computes the length of the string in `Rn` into `Rd`.

```
adl Rd, Rn
```

---

#### itoa

Convert the integer in `Rn` to an ASCII string and store it in `Rd`.

```
itoa Rd, Rn
```

```asm
itoa x1, x0
```

---

#### atoi

Parse the ASCII string in `Rn` as an integer and store the result in `Rd`.

```
atoi Rd, Rn
```

```asm
atoi x1, x0
```

---

### Data Movement

---

#### mov

Move a register or immediate value into a destination register.

```
mov Rd, Rn
mov Rd, #imm
```

```asm
mov x0, x1
mov x0, #100
```

---

#### movz

Move a zero-extended immediate into a register, optionally shifted.

```
movz Rd, #imm [, lsl #shift]
```

```asm
movz x0, #0xFF00, lsl #16
```

---

#### movn

Move the bitwise inverse of an immediate into a register, optionally shifted.

```
movn Rd, #imm [, lsl #shift]
```

---

#### movk

Insert a 16-bit immediate into a specific position of a register without affecting other bits.

```
movk Rd, #imm [, lsl #shift]
```

---

#### mvn

Bitwise NOT of a source register into a destination.

```
mvn Rd, Rn
```

---

### Arithmetic

---

#### add / adds

Add two registers or a register and an immediate. `adds` sets condition flags.

```
add  Rd, Rn, Rm [, shift #amt]
add  Rd, Rn, #imm
adds Rd, Rn, Rm [, shift #amt]
adds Rd, Rn, #imm
```

Supported shift types: `lsl`, `lsr`, `asr`.

```asm
add  x2, x0, x1
add  x0, x0, #8
adds x2, x0, x1, lsl #2
```

---

#### sub / subs

Subtract. `subs` sets condition flags.

```
sub  Rd, Rn, Rm [, shift #amt]
sub  Rd, Rn, #imm
subs Rd, Rn, Rm [, shift #amt]
subs Rd, Rn, #imm
```

---

#### neg / negs

Negate a register. `negs` sets condition flags.

```
neg  Rd, Rn
negs Rd, Rn
```

---

#### adc / adcs

Add with carry. `adcs` sets condition flags.

```
adc  Rd, Rn, Rm
adcs Rd, Rn, Rm
```

---

#### sbc / sbcs

Subtract with carry. `sbcs` sets condition flags.

```
sbc  Rd, Rn, Rm
sbcs Rd, Rn, Rm
```

---

#### mul

Multiply two registers.

```
mul Rd, Rn, Rm
```

---

#### udiv

Unsigned divide.

```
udiv Rd, Rn, Rm
```

---

#### sdiv

Signed divide.

```
sdiv Rd, Rn, Rm
```

---

#### madd

Multiply-add: `Rd = Rn * Rm + Ra`.

```
madd Rd, Rn, Rm, Ra
```

---

#### msub

Multiply-subtract: `Rd = Ra - Rn * Rm`.

```
msub Rd, Rn, Rm, Ra
```

---

#### mneg

Multiply-negate: `Rd = -(Rn * Rm)`.

```
mneg Rd, Rn, Rm
```

---

#### smull / umull

Signed/unsigned 32x32 multiply producing a 64-bit result.

```
smull Rd, Rn, Rm
umull Rd, Rn, Rm
```

---

#### smulh / umulh

Signed/unsigned 64x64 multiply, returning the high 64 bits.

```
smulh Rd, Rn, Rm
umulh Rd, Rn, Rm
```

---

#### smaddl / umaddl / smsubl / umsubl

Long multiply-add/subtract variants.

```
smaddl Rd, Rn, Rm, Ra
umaddl Rd, Rn, Rm, Ra
smsubl Rd, Rn, Rm, Ra
umsubl Rd, Rn, Rm, Ra
```

---

### Logical and Bitwise

---

#### and / ands

Bitwise AND. `ands` sets condition flags.

```
and  Rd, Rn, Rm
and  Rd, Rn, #imm
ands Rd, Rn, Rm
ands Rd, Rn, #imm
```

---

#### orr

Bitwise OR.

```
orr Rd, Rn, Rm
orr Rd, Rn, #imm
```

---

#### orn

Bitwise OR NOT: `Rd = Rn | ~Rm`.

```
orn Rd, Rn, Rm
```

---

#### eor

Bitwise exclusive OR.

```
eor Rd, Rn, Rm
eor Rd, Rn, #imm
```

---

#### eon

Bitwise exclusive OR NOT: `Rd = Rn ^ ~Rm`.

```
eon Rd, Rn, Rm
```

---

#### bic / bics

Bit clear: `Rd = Rn & ~Rm`. `bics` sets condition flags.

```
bic  Rd, Rn, Rm
bics Rd, Rn, Rm
```

---

### Shift and Rotate

---

#### lsl

Logical shift left.

```
lsl Rd, Rn, Rm
lsl Rd, Rn, #imm
```

---

#### lsr

Logical shift right.

```
lsr Rd, Rn, Rm
lsr Rd, Rn, #imm
```

---

#### asr

Arithmetic shift right (sign-extending).

```
asr Rd, Rn, Rm
asr Rd, Rn, #imm
```

---

#### ror

Rotate right.

```
ror Rd, Rn, Rm
ror Rd, Rn, #imm
```

---

#### extr

Extract a bitfield spanning two registers: `Rd = {Rn, Rm}[lsb+63:lsb]`.

```
extr Rd, Rn, Rm, #lsb
```

---

### Load and Store

---

#### ldr

Load a 64-bit value from memory or from a register.

```
ldr Rd, [Rn]
ldr Rd, [Rn, #off]
ldr Rd, Rn
```

---

#### str

Store a 64-bit value to memory.

```
str Rn, [Rm]
str Rn, [Rm, #off]
```

---

#### ldrb / strb

Load/store an unsigned byte.

```
ldrb Rd, [Rn, #off]
strb Rn, [Rm, #off]
```

---

#### ldrh / strh

Load/store an unsigned halfword (16 bits).

```
ldrh Rd, [Rn, #off]
strh Rn, [Rm, #off]
```

---

#### ldrsb

Load a signed byte (sign-extended to 64 bits).

```
ldrsb Rd, [Rn, #off]
```

---

#### ldrsh

Load a signed halfword (sign-extended to 64 bits).

```
ldrsh Rd, [Rn, #off]
```

---

#### ldrsw

Load a signed word (sign-extended to 64 bits).

```
ldrsw Rd, [Rn, #off]
```

---

#### ldp

Load pair of registers.

```
ldp Rd1, Rd2, [Rn, #off]
```

```asm
ldp x0, x1, [sp, #16]
```

---

#### stp

Store pair of registers.

```
stp Rn1, Rn2, [Rm, #off]
```

```asm
stp x0, x1, [sp, #-16]
```

---

#### adr

Compute the address of a label or immediate offset relative to the current PC and store in a register.

```
adr Rd, label
adr Rd, #imm
```

---

#### adrp

Compute a page-aligned address.

```
adrp Rd, label
adrp Rd, #imm
```

---

### Branch and Control Flow

---

#### b

Unconditional branch to a label.

```
b label
```

---

#### bl

Branch with link (call): branch to label and store return address in `lr`.

```
bl label
```

---

#### br

Branch to the address held in a register.

```
br Rn
```

---

#### blr

Branch with link to the address held in a register.

```
blr Rn
```

---

#### ret

Return from a subroutine. Branches to the address in `lr` by default, or to the specified register.

```
ret
ret Rn
```

---

#### cbz

Branch to a label if a register is zero.

```
cbz Rn, label
```

---

#### cbnz

Branch to a label if a register is non-zero.

```
cbnz Rn, label
```

---

#### tbz

Branch if a specific bit of a register is zero.

```
tbz Rn, #bit, label
```

---

#### tbnz

Branch if a specific bit of a register is non-zero.

```
tbnz Rn, #bit, label
```

---

#### Conditional branches (b.cond)

Branch to a label based on condition flags. All forms below are accepted:

```
b.eq label     beq label
b.ne label     bne label
b.lt label     blt label
b.le label     ble label
b.gt label     bgt label
b.ge label     bge label
b.lo label     blo label
b.ls label     bls label
b.hi label     bhi label
b.hs label     bhs label
b.mi label     bmi label
b.pl label     bpl label
b.vs label     bvs label
b.vc label     bvc label
b.al label     bal label
```

---

#### Control flow aliases (custom)

These are shorthand aliases for common branch patterns.

| Instruction | Equivalent |
|---|---|
| `jmp label` | `b label` |
| `call label` | `bl label` |
| `jeq label` | `b.eq label` |
| `jne label` | `b.ne label` |
| `jlt label` | `b.lt label` |
| `jle label` | `b.le label` |
| `jgt label` | `b.gt label` |
| `jge label` | `b.ge label` |

```asm
call my_function
jmp  done
jeq  equal_branch
```

---

### Comparison

---

#### cmp

Compare two values by subtracting and setting flags (result discarded).

```
cmp Rn, Rm
cmp Rn, #imm
```

---

#### cmn

Compare negative: add and set flags (result discarded).

```
cmn Rn, Rm
cmn Rn, #imm
```

---

#### tst

Bitwise AND and set flags (result discarded).

```
tst Rn, Rm
tst Rn, #imm
```

---

#### ccmp

Conditional compare: if the named condition holds, compute `Rn - Rm` and set flags; otherwise set flags from the immediate NZCV value.

```
ccmp Rn, Rm,   #nzcv, cond
ccmp Rn, #imm, #nzcv, cond
```

---

#### ccmn

Conditional compare negative.

```
ccmn Rn, Rm,   #nzcv, cond
ccmn Rn, #imm, #nzcv, cond
```

---

### Conditional Select

---

#### csel

Select between two registers based on a condition: `Rd = (cond) ? Rn : Rm`.

```
csel Rd, Rn, Rm, cond
```

---

#### csinc

Conditional select increment: `Rd = (cond) ? Rn : Rm + 1`.

```
csinc Rd, Rn, Rm, cond
```

---

#### csinv

Conditional select invert: `Rd = (cond) ? Rn : ~Rm`.

```
csinv Rd, Rn, Rm, cond
```

---

#### csneg

Conditional select negate: `Rd = (cond) ? Rn : -Rm`.

```
csneg Rd, Rn, Rm, cond
```

---

#### cset

Set register to 1 if condition is true, else 0.

```
cset Rd, cond
```

---

#### csetm

Set register to all-ones if condition is true, else 0.

```
csetm Rd, cond
```

---

#### cinc

Conditional increment: `Rd = (cond) ? Rn + 1 : Rn`.

```
cinc Rd, Rn, cond
```

---

#### cinv

Conditional invert: `Rd = (cond) ? ~Rn : Rn`.

```
cinv Rd, Rn, cond
```

---

#### cneg

Conditional negate: `Rd = (cond) ? -Rn : Rn`.

```
cneg Rd, Rn, cond
```

---

### Multiply and Divide Extended

See [Arithmetic](#arithmetic) for `madd`, `msub`, `mneg`, `smull`, `umull`, `smulh`, `umulh`, `smaddl`, `umaddl`, `smsubl`, `umsubl`.

---

### Bit Field Operations

---

#### sbfm / bfm / ubfm

Signed/neutral/unsigned bit field move.

```
sbfm Rd, Rn, #immr, #imms
bfm  Rd, Rn, #immr, #imms
ubfm Rd, Rn, #immr, #imms
```

---

#### sbfx / ubfx

Extract a signed/unsigned bit field.

```
sbfx Rd, Rn, #lsb, #width
ubfx Rd, Rn, #lsb, #width
```

---

#### sbfiz / ubfiz

Signed/unsigned bit field insert in zeros.

```
sbfiz Rd, Rn, #lsb, #width
ubfiz Rd, Rn, #lsb, #width
```

---

#### bfxil

Bit field extract and insert low.

```
bfxil Rd, Rn, #lsb, #width
```

---

#### bfi

Bit field insert.

```
bfi Rd, Rn, #lsb, #width
```

---

#### clz

Count leading zeros.

```
clz Rd, Rn
```

---

#### cls

Count leading sign bits.

```
cls Rd, Rn
```

---

#### rbit

Reverse bit order.

```
rbit Rd, Rn
```

---

#### rev / rev16 / rev32 / rev64

Reverse byte order within elements of different widths.

```
rev   Rd, Rn
rev16 Rd, Rn
rev32 Rd, Rn
rev64 Rd, Rn
```

---

### Sign and Zero Extension

---

#### sxtb / sxth / sxtw

Sign-extend byte, halfword, or word to 64 bits.

```
sxtb Rd, Rn
sxth Rd, Rn
sxtw Rd, Rn
```

---

#### uxtb / uxth

Zero-extend byte or halfword to 64 bits.

```
uxtb Rd, Rn
uxth Rd, Rn
```

---

### Floating Point

---

#### fmov

Move between floating-point registers or load a floating-point immediate.

```
fmov Rd, Rn
fmov Rd, #imm
```

---

#### fadd / fsub / fmul / fdiv

Floating-point arithmetic.

```
fadd Rd, Rn, Rm
fsub Rd, Rn, Rm
fmul Rd, Rn, Rm
fdiv Rd, Rn, Rm
```

---

#### fabs / fneg / fsqrt

Floating-point absolute value, negate, or square root.

```
fabs  Rd, Rn
fneg  Rd, Rn
fsqrt Rd, Rn
```

---

#### fmin / fmax / fminnm / fmaxnm

Floating-point min/max (with and without NaN propagation).

```
fmin   Rd, Rn, Rm
fmax   Rd, Rn, Rm
fminnm Rd, Rn, Rm
fmaxnm Rd, Rn, Rm
```

---

#### fcmp / fcmpe

Floating-point compare (sets flags). `fcmpe` signals on quiet NaN.

```
fcmp  Rn, Rm
fcmp  Rn, #0.0
fcmpe Rn, Rm
```

---

#### fccmp / fccmpe

Conditional floating-point compare.

```
fccmp  Rn, Rm, #nzcv, cond
fccmpe Rn, Rm, #nzcv, cond
```

---

#### fcsel

Floating-point conditional select.

```
fcsel Rd, Rn, Rm, cond
```

---

#### fmadd / fmsub / fnmadd / fnmsub

Floating-point fused multiply-add/subtract variants.

```
fmadd  Rd, Rn, Rm, Ra
fmsub  Rd, Rn, Rm, Ra
fnmadd Rd, Rn, Rm, Ra
fnmsub Rd, Rn, Rm, Ra
```

---

#### Floating-point conversion instructions

| Instruction | Description |
|---|---|
| `fcvt Rd, Rn` | Convert between float precisions |
| `fcvtas Rd, Rn` | Convert to signed integer, round to nearest (ties to away) |
| `fcvtau Rd, Rn` | Convert to unsigned integer, round to nearest (ties to away) |
| `fcvtms Rd, Rn` | Convert to signed integer, round toward minus infinity |
| `fcvtmu Rd, Rn` | Convert to unsigned integer, round toward minus infinity |
| `fcvtns Rd, Rn` | Convert to signed integer, round to nearest (ties to even) |
| `fcvtnu Rd, Rn` | Convert to unsigned integer, round to nearest (ties to even) |
| `fcvtps Rd, Rn` | Convert to signed integer, round toward plus infinity |
| `fcvtpu Rd, Rn` | Convert to unsigned integer, round toward plus infinity |
| `fcvtzs Rd, Rn` | Convert to signed integer, round toward zero |
| `fcvtzu Rd, Rn` | Convert to unsigned integer, round toward zero |
| `scvtf  Rd, Rn` | Convert signed integer to float |
| `ucvtf  Rd, Rn` | Convert unsigned integer to float |

---

### Atomic and Exclusive Access

---

#### ldar / ldarb / ldarh

Load-acquire (full word / byte / halfword) from the address in a register.

```
ldar  Rd, [Rn]
ldarb Rd, [Rn]
ldarh Rd, [Rn]
```

---

#### ldaxr / ldaxrb / ldaxrh

Load-acquire exclusive.

```
ldaxr  Rd, [Rn]
ldaxrb Rd, [Rn]
ldaxrh Rd, [Rn]
```

---

#### ldxr / ldxrb / ldxrh

Load exclusive.

```
ldxr  Rd, [Rn]
ldxrb Rd, [Rn]
ldxrh Rd, [Rn]
```

---

#### stlr / stlrb / stlrh

Store-release.

```
stlr  Rn, [Rm]
stlrb Rn, [Rm]
stlrh Rn, [Rm]
```

---

#### stlxr / stlxrb / stlxrh

Store-release exclusive. The status register `Rd` is set to 0 on success, 1 on failure.

```
stlxr  Rd, Rn, [Rm]
stlxrb Rd, Rn, [Rm]
stlxrh Rd, Rn, [Rm]
```

---

#### stxr / stxrb / stxrh

Store exclusive.

```
stxr  Rd, Rn, [Rm]
stxrb Rd, Rn, [Rm]
stxrh Rd, Rn, [Rm]
```

---

#### ldxp / ldaxp

Load exclusive pair.

```
ldxp  Rd1, Rd2, [Rn]
ldaxp Rd1, Rd2, [Rn]
```

---

#### stxp / stlxp

Store exclusive pair.

```
stxp  Rd, Rn1, Rn2, [Rm]
stlxp Rd, Rn1, Rn2, [Rm]
```

---

### System Instructions

---

#### nop

No operation.

```
nop
```

---

#### svc

Supervisor call.

```
svc #imm
```

---

#### hlt

Halt with an immediate code.

```
hlt #imm
```

---

#### brk

Breakpoint.

```
brk #imm
```

---

#### wfe / wfi / sev / sevl

Wait for event, wait for interrupt, send event, send event local.

```
wfe
wfi
sev
sevl
```

---

#### isb / dsb / dmb / clrex / yield / eret / drps

Barrier and miscellaneous system instructions.

```
isb
dsb
dmb
clrex
yield
eret
drps
```

---

#### mrs

Move from system register to general register.

```
mrs Rd, sysreg
```

```asm
mrs x0, nzcv
```

---

#### msr

Move from general register to system register.

```
msr sysreg, Rn
```

```asm
msr nzcv, x0
```

---

#### sys / sysl

Generic system instruction.

```
sys  op1, Cn, Cm, op2 [, Rt]
sysl Rt, op1, Cn, Cm, op2
```

---

#### ic / dc / at / tlbi

Cache, address translation, and TLB invalidation operations.

```
ic   op
dc   op, Rn
at   op, Rn
tlbi op [, Rn]
```

---

#### prfm / prfum

Prefetch memory.

```
prfm  type, [Rn, #off]
prfum type, [Rn, #off]
```

---

#### ldraa / ldrab

Load register with pointer authentication.

```
ldraa Rd, [Rn, #off]
ldrab Rd, [Rn, #off]
```

---

### Pseudo-Directives

These directives do not emit a runtime instruction but instead embed data or control alignment in the bytecode stream.

---

#### .word

Embed a 32-bit immediate value.

```
.word #value
```

```asm
.word #0xDEADBEEF
```

---

#### .dword

Embed a 64-bit immediate value.

```
.dword #value
```

---

#### .byte

Embed a single byte value.

```
.byte #value
```

---

#### .space

Emit N zero bytes.

```
.space #N
```

---

#### .ascii

Embed a raw ASCII string (no null terminator).

```
.ascii "text"
```

---

#### .asciz

Embed a null-terminated ASCII string.

```
.asciz "text"
```

---

#### .align

Emit NOP instructions to align to a boundary.

```
.align #N
```

The default alignment boundary is 4 if no argument is given.

---

### Stack Pseudo-Instructions

---

#### push

Push a register onto the stack.

```
push Rn
```

---

#### pop

Pop a value from the stack into a register.

```
pop Rd
```

---

## Opcode Table

The following table lists every opcode number used in the bytecode output. The VM must implement these with the same numbering.

| Opcode | Mnemonic | Opcode | Mnemonic |
|---|---|---|---|
| 0 | lds | 1 | adl |
| 2 | mov | 3 | ldr |
| 4 | str | 5 | add |
| 6 | sub | 7 | mul |
| 8 | udiv | 9 | sdiv |
| 10 | and | 11 | orr |
| 12 | eor | 13 | mvn |
| 14 | lsl | 15 | lsr |
| 16 | asr | 17 | ror |
| 18 | b | 19 | bl |
| 20 | br | 21 | blr |
| 22 | ret | 23 | cbz |
| 24 | cbnz | 25 | tbz |
| 26 | tbnz | 27 | b.eq |
| 28 | b.ne | 29 | b.lt |
| 30 | b.le | 31 | b.gt |
| 32 | b.ge | 33 | b.lo |
| 34 | b.ls | 35 | b.hi |
| 36 | b.hs | 37 | b.mi |
| 38 | b.pl | 39 | b.vs |
| 40 | b.vc | 41 | b.al |
| 42 | cmp | 43 | cmn |
| 44 | tst | 45 | neg |
| 46 | negs | 47 | sbc |
| 48 | sbcs | 49 | adc |
| 50 | adcs | 51 | adds |
| 52 | subs | 53 | madd |
| 54 | msub | 55 | mneg |
| 56 | smull | 57 | umull |
| 58 | smulh | 59 | umulh |
| 60 | smaddl | 61 | umaddl |
| 62 | smsubl | 63 | umsubl |
| 64 | ldrb | 65 | ldrh |
| 66 | ldrsb | 67 | ldrsh |
| 68 | ldrsw | 69 | strb |
| 70 | strh | 71 | ldp |
| 72 | stp | 73 | adrp |
| 74 | adr | 75 | nop |
| 76 | svc | 77 | hlt |
| 78 | brk | 79 | wfe |
| 80 | wfi | 81 | sev |
| 82 | sevl | 83 | isb |
| 84 | dsb | 85 | dmb |
| 86 | clrex | 87 | yield |
| 88 | eret | 89 | drps |
| 90 | mrs | 91 | msr |
| 92 | sys | 93 | sysl |
| 94 | ic | 95 | dc |
| 96 | at | 97 | tlbi |
| 98 | clz | 99 | cls |
| 100 | rbit | 101 | rev |
| 102 | rev16 | 103 | rev32 |
| 104 | rev64 | 105 | extr |
| 106 | sbfm | 107 | bfm |
| 108 | ubfm | 109 | sbfx |
| 110 | sbfiz | 111 | bfxil |
| 112 | bfi | 113 | ubfx |
| 114 | ubfiz | 115 | sxtb |
| 116 | sxth | 117 | sxtw |
| 118 | uxtb | 119 | uxth |
| 120 | ands | 121 | bics |
| 122 | bic | 123 | eon |
| 124 | orn | 125 | movz |
| 126 | movn | 127 | movk |
| 128 | fmov | 129 | fadd |
| 130 | fsub | 131 | fmul |
| 132 | fdiv | 133 | fabs |
| 134 | fneg | 135 | fsqrt |
| 136 | fcmp | 137 | fcmpe |
| 138 | fccmp | 139 | fccmpe |
| 140 | fcsel | 141 | fcvt |
| 142 | fcvtas | 143 | fcvtau |
| 144 | fcvtms | 145 | fcvtmu |
| 146 | fcvtns | 147 | fcvtnu |
| 148 | fcvtps | 149 | fcvtpu |
| 150 | fcvtzs | 151 | fcvtzu |
| 152 | scvtf | 153 | ucvtf |
| 154 | fmadd | 155 | fmsub |
| 156 | fnmadd | 157 | fnmsub |
| 158 | fminnm | 159 | fmaxnm |
| 160 | fmin | 161 | fmax |
| 162 | csel | 163 | csinc |
| 164 | csinv | 165 | csneg |
| 166 | cset | 167 | csetm |
| 168 | cinc | 169 | cinv |
| 170 | cneg | 171 | ccmp |
| 172 | ccmn | 173 | ldar |
| 174 | ldarb | 175 | ldarh |
| 176 | ldaxr | 177 | ldaxrb |
| 178 | ldaxrh | 179 | ldxr |
| 180 | ldxrb | 181 | ldxrh |
| 182 | stlr | 183 | stlrb |
| 184 | stlrh | 185 | stlxr |
| 186 | stlxrb | 187 | stlxrh |
| 188 | stxr | 189 | stxrb |
| 190 | stxrh | 191 | ldxp |
| 192 | ldaxp | 193 | stxp |
| 194 | stlxp | 195 | prfm |
| 196 | prfum | 197 | ldraa |
| 198 | ldrab | 199 | ldr_literal |
| 200 | push | 201 | pop |
| 203 | swp | 204 | dmp |
| 205 | halt | 206 | puts |
| 207 | geti | 208 | gets |
| 209 | rand | 210 | time |
| 211 | clrr | 212 | inc |
| 213 | dec | 214 | abs |
| 215 | max2 | 216 | min2 |
| 217 | mod | 218 | not |
| 219 | shl | 220 | shr |
| 221 | memcpy | 222 | memset |
| 223 | strcpy | 224 | strcat |
| 225 | strcmp | 226 | strlen |
| 227 | itoa | 228 | atoi |
| 230 | call | 231 | jmp |
| 232 | jeq | 233 | jne |
| 234 | jlt | 235 | jle |
| 236 | jgt | 237 | jge |
| 238 | .align | 239 | .word |
| 240 | .dword | 241 | .byte |
| 242 | .space | 243 | .ascii |
| 244 | .asciz | | |

Note: opcode 229 is the label pseudo-instruction and is never written to the bytecode output.

---

## Bytecode Output Format

The `.wassm` file produced by the assembler is a plain text file. It begins with a header followed by one encoded instruction per line.

**Header format:**

```
; WebAssembler Bytecode v1.0
; Source: hello.iwa
; Lines: 12
;;;
```

**Instruction line format:**

Each instruction line starts with its numeric opcode, followed by space-separated operand tokens. Operands are encoded as follows:

- Registers are written as `family:number`, e.g. `0:0` for `x0`, `1:1` for `w1`, `2:31` for `sp`.
- Immediates are written as plain decimal integers.
- Mode flags such as `R` (register), `I` (immediate), or `M` (memory) may appear between operands to indicate the addressing mode.
- Strings have spaces encoded as `\~`, newlines as `\n`, tabs as `\t`, and backslashes as `\\`.
- Label references are resolved at compile time and replaced with the integer line index of the target instruction.

**Example output for a simple program:**

```
; WebAssembler Bytecode v1.0
; Source: hello.iwa
; Lines: 4
;;;
0 0:0 Hello,\~World!\n
206 0:0
205
```

---

## Examples

### Hello, World

```asm
; hello.iwa
; Print a greeting and exit.

    lds  x0, "Hello, World!\n"
    puts x0
    halt
```

Compile and run:

```sh
./assembler hello.iwa hello.wassm
node vm.js hello.wassm
```

---

### Count to 10

```asm
; count.iwa
; Count from 1 to 10 and print each number.

    mov  x0, #1

loop:
    itoa x1, x0
    puts x1
    inc  x0
    cmp  x0, #11
    jlt  loop

    halt
```

---

### Sum of integers using a subroutine

```asm
; sum.iwa
; Read two integers from stdin and print their sum.

    lds  x9, "Enter first number: "
    puts x9
    geti x0

    lds  x9, "Enter second number: "
    puts x9
    geti x1

    call add_two
    itoa x2, x0
    puts x2
    halt

add_two:
    add  x0, x0, x1
    ret
```

---

### Factorial

```asm
; factorial.iwa
; Compute factorial of a number read from stdin.

    lds  x9, "Enter n: "
    puts x9
    geti x0

    mov  x1, #1
    call fact
    itoa x2, x1
    puts x2
    halt

fact:
    cmp  x0, #1
    jle  fact_done
    mul  x1, x1, x0
    dec  x0
    jmp  fact
fact_done:
    ret
```

---

### String reversal with strcmp

```asm
; strops.iwa

    lds   x0, "hello"
    lds   x1, "hello"
    lds   x2, "world"

    strcmp x3, x0, x1      ; x3 = 0 (equal)
    strcmp x4, x0, x2      ; x4 != 0 (not equal)

    dmp x3
    dmp x4
    halt
```

---

## Limits and Constraints

| Item | Limit |
|---|---|
| Maximum source line length | 4096 characters |
| Maximum tokens per line | 64 |
| Maximum labels | 4096 |
| Maximum fixup (label reference) entries | 8192 |
| Maximum output instructions | 65536 |
| Maximum token length | 512 characters |
| Register numbers (general) | x0-x30, w0-w30 |
| Register numbers (float) | d0-d31, s0-s31, q0-q31, v0-v31 |

Exceeding any of these limits will cause the assembler to print an error and exit.

---

## Error Reference

| Message | Cause |
|---|---|
| `Line N: bad register 'X'` | The token is not a valid register name |
| `Line N: unknown mnemonic 'X'` | The instruction name is not recognized |
| `Line N: lds needs 2 operands` | Instruction has fewer operands than required |
| `Error: undefined label 'X'` | A branch or call refers to a label that was never defined |
| `Error: too many labels` | The label table is full (limit: 4096) |
| `Error: too many fixups` | The fixup table is full (limit: 8192) |
| `Error: too many instructions` | The output buffer is full (limit: 65536 lines) |
| `Error: cannot open 'X'` | The input or output file could not be opened |
| `Internal error: FIXUP marker missing` | Compiler internal consistency failure |

All errors are printed to stderr. The assembler exits with status 1 on any error.

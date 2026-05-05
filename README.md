<img width="900" height="200" alt="wl-banner" src="https://github.com/PoTiwi/wlbi/blob/main/ico/wl-bg-banner.png" /> 

<hr>
A compiler that translates custom AArch64-inspired assembly into plain-text bytecode, which can then be run by the JavaScript VM — either in a browser or in Node.js.

The whole pipeline looks like this:

```
a.iwa  →  [assembler]  →  a.wassm  →  [wlbi.js]  →  output
```

---

## Table of Contents

- [Quick Start](#quick-start)
  - [Step 1 — Write some assembly](#step-1--write-some-assembly)
  - [Step 2 — Compile it](#step-2--compile-it)
  - [Step 3 — Run it in the browser](#step-3--run-it-in-the-browser)
  - [Step 3 (alternative) — Run it in Node.js](#step-3-alternative--run-it-in-nodejs)
- [Building the Assembler](#building-the-assembler)
- [JavaScript VM Reference](#javascript-vm-reference)
  - [Loading bytecode](#loading-bytecode)
  - [I/O hooks](#io-hooks)
  - [Running the program](#running-the-program)
  - [Full browser example](#full-browser-example)
  - [Full Node.js example](#full-nodejs-example)
  - [Performance tuning](#performance-tuning)
- [Assembly Language Guide](#assembly-language-guide)
  - [Comments](#comments)
  - [Labels](#labels)
  - [Registers](#registers)
  - [Immediates](#immediates)
  - [Strings](#strings)
- [Instruction Reference](#instruction-reference)
  - [Printing and I/O](#printing-and-io)
  - [Data Movement](#data-movement)
  - [Arithmetic](#arithmetic)
  - [Logical and Bitwise](#logical-and-bitwise)
  - [Shifts and Rotates](#shifts-and-rotates)
  - [Loads and Stores](#loads-and-stores)
  - [Branches and Control Flow](#branches-and-control-flow)
  - [Comparison](#comparison)
  - [Conditional Select](#conditional-select)
  - [Multiply Extended](#multiply-extended)
  - [Bit Field Operations](#bit-field-operations)
  - [Sign and Zero Extension](#sign-and-zero-extension)
  - [Floating Point](#floating-point)
  - [String and Memory Utilities](#string-and-memory-utilities)
  - [Register Utilities](#register-utilities)
  - [Atomic and Exclusive Access](#atomic-and-exclusive-access)
  - [System Instructions](#system-instructions)
  - [Stack](#stack)
  - [Data Directives](#data-directives)
- [Opcode Table](#opcode-table)
- [Bytecode Format](#bytecode-format)
- [Limits](#limits)
- [Error Reference](#error-reference)

---

## Quick Start

### Step 1 — Write some assembly

Create a file called `hello.iwa`:

```asm
; hello.iwa — classic greeting

    lds  x0, "Hello, World!\n"
    puts x0
    halt
```

### Step 2 — Compile it

```sh
./wa-c hello.iwa hello.wassm
```

You should see:

```
Compiled 3 instruction(s) -> hello.wassm
```

### Step 3 — Run it in the browser

Drop `wlbi.js` into your project. Then in your HTML:

```html
<script src="wlbi.js"></script>
<script>
  wlbi.onOutput = (text) => console.log(text);

  wlbi.init("hello.wassm", () => {
    wlbi.execute();
  });
</script>
```

### Step 3 (alternative) — Run it in Node.js

```js
const wlbi = require('./wlbi.js');
const readline = require('readline');

// Wire up I/O
wlbi.onOutput = (text) => process.stdout.write(text);
wlbi.onError  = (text) => process.stderr.write(text);
wlbi.onInput  = () => new Promise(resolve => {
  const rl = readline.createInterface({ input: process.stdin });
  rl.once('line', line => { rl.close(); resolve(line); });
});

wlbi.init('hello.wassm', () => {
  wlbi.execute();
});
```

---

## Building the Assembler

The assembler is a single C99 source file with no external dependencies.

```sh
gcc  -O2 -o assembler assembler.c
# or
clang -O2 -o assembler assembler.c
```

Usage:

```sh
./assembler <input.iwa> <output.wassm>
```

---

## JavaScript VM Reference

`wlbi.js` exposes a single global object (or CommonJS module export) called `wlbi`. Everything is async under the hood — programs that need input (`geti`, `gets`) suspend and wait for a Promise to resolve before continuing.

### Loading bytecode

There are three ways to get bytecode into the VM:

```js
// From a URL (uses fetch — works in the browser and Node ≥18)
wlbi.init("program.wassm", callback);

// From a raw string (useful for embedding bytecode inline)
wlbi.initFromString(bytecodeSrc, callback);

// From a File or Blob object (browser file input, drag-and-drop, etc.)
wlbi.initFromFile(fileObject, callback);
```

All three return a `Promise` and also accept an optional callback — use whichever style you prefer:

```js
// Promise style
await wlbi.init("program.wassm");
await wlbi.execute();

// Callback style
wlbi.init("program.wassm", () => wlbi.execute());
```

### I/O hooks

Set these **before** calling `execute()`. They are plain properties on the `wlbi` object.

```js
// Called whenever the program prints something (puts, itoa+puts, etc.)
wlbi.onOutput = (text) => {
  document.getElementById("output").textContent += text;
};

// Called for debug dumps (dmp instruction) and VM error messages
wlbi.onError = (text) => {
  console.error(text);
};

// Called whenever the program reads input (geti, gets).
// Must return a Promise<string>.
wlbi.onInput = () => {
  return new Promise(resolve => {
    // Show your own input UI, then resolve with the user's string
    showInputDialog().then(resolve);
  });
};
```

If you don't set `onInput`, the VM falls back to the browser's built-in `prompt()` dialog.

### Running the program

```js
// Basic — run to completion
await wlbi.execute();

// With a custom yield threshold (default is 50 000)
// Lower = more responsive UI during tight loops; Higher = faster execution
await wlbi.execute(10_000);
```

`execute()` returns a Promise that resolves when the program reaches `halt` or runs off the end of the bytecode.

### Full browser example

This wires up a simple terminal-style UI: a scrolling output area and an input field that the program can read from.

```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>wlbi</title>
</head>
<body>
  <pre id="output"></pre>
  <input id="inputBox" type="text" placeholder="Type input here and press Enter">

  <script src="wlbi.js"></script>
  <script>
    const output   = document.getElementById('output');
    const inputBox = document.getElementById('inputBox');

    // Queue of pending input resolvers
    const inputQueue = [];

    wlbi.onOutput = (text) => { output.textContent += text; };
    wlbi.onError  = (text) => { output.textContent += '[ERR] ' + text + '\n'; };

    // When the program asks for input, push a resolver onto the queue.
    // It resolves when the user presses Enter.
    wlbi.onInput = () => new Promise(resolve => {
      inputQueue.push(resolve);
      inputBox.focus();
    });

    inputBox.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && inputQueue.length > 0) {
        const resolve = inputQueue.shift();
        const value   = inputBox.value;
        inputBox.value = '';
        output.textContent += value + '\n'; // echo
        resolve(value);
      }
    });

    // Load and run a .wassm file dropped onto the page
    document.addEventListener('dragover', e => e.preventDefault());
    document.addEventListener('drop', async (e) => {
      e.preventDefault();
      const file = e.dataTransfer.files[0];
      if (!file) return;
      await wlbi.initFromFile(file);
      output.textContent = '';
      wlbi.execute();
    });
  </script>
</body>
</html>
```

### Full Node.js example

```js
const wlbi = require('./wlbi.js');
const readline     = require('readline');

const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
const ask = (prompt) => new Promise(resolve => rl.question(prompt, resolve));

wlbi.onOutput = (text) => process.stdout.write(text);
wlbi.onError  = (text) => process.stderr.write('[ERR] ' + text + '\n');
wlbi.onInput  = ()     => ask('');

const file = process.argv[2];
if (!file) { console.error('Usage: node run.js <program.wassm>'); process.exit(1); }

wlbi.init(file, async () => {
  await wlbi.execute();
  rl.close();
});
```

Save as `run.js`, then:

```sh
node run.js hello.wassm
```

### Performance tuning

The VM yields to the browser's event loop every N instructions so the page stays responsive. The default (`50 000`) is a good balance, but you can tune it:

```js
// More responsive during heavy loops (slower overall)
wlbi.execute(5_000);

// Maximum throughput for batch/non-interactive programs
wlbi.execute(500_000);
```

---

## Assembly Language Guide

### Comments

Comments start with `;` and run to the end of the line. They can appear on their own line or after an instruction.

```asm
; full-line comment
mov x0, #42    ; inline comment
```

### Labels

A label is a name followed by a colon. It marks the position of the next instruction and can be used as a branch target. Labels are resolved at compile time — the VM only ever sees line numbers.

```asm
loop:
    inc  x0
    cmp  x0, #10
    jlt  loop       ; jumps back to the inc above
```

A label can share a line with an instruction:

```asm
start: mov x0, #0
```

Label names are case-sensitive and can contain letters, digits, underscores (`_`), and dots (`.`).

### Registers

| Name | Description |
|---|---|
| `x0`–`x30` | 64-bit general-purpose registers |
| `w0`–`w30` | 32-bit view of the same registers (reads/writes zero-extend) |
| `sp` | Stack pointer |
| `lr` | Link register — holds the return address after `bl`/`call` |
| `fp` | Frame pointer (alias for `x29`) |
| `xzr` / `wzr` | Zero register — always reads as 0, writes are discarded |
| `ip0` / `ip1` | Scratch registers (aliases for `x16`/`x17`) |
| `d0`–`d31` | 64-bit floating-point (double) |
| `s0`–`s31` | 32-bit floating-point (single) |

Register names are case-insensitive (`X0` and `x0` are the same).

### Immediates

```asm
mov x0, #42        ; decimal
mov x0, #0xFF      ; hexadecimal
mov x0, #-1        ; negative
add x0, x0, #8     ; the # is optional in most places
```

### Strings

String literals go inside double quotes. Supported escape sequences:

| Sequence | Meaning |
|---|---|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\0` | Null byte |
| `\\` | Literal backslash |
| `\"` | Literal double-quote |

```asm
lds x0, "Hello!\n"
```

---

## Instruction Reference

Notation used below:

| Symbol | Meaning |
|---|---|
| `Rd` | Destination register |
| `Rn`, `Rm`, `Ra` | Source registers |
| `#imm` | Immediate value |
| `label` | A label defined somewhere in the source |
| `[Rn]` | Memory at the address stored in `Rn` |
| `[Rn, #off]` | Memory at `Rn + offset` |
| `cond` | Condition code (see table below) |

**Condition codes:**

| Code | Meaning | Code | Meaning |
|---|---|---|---|
| `eq` | Equal | `ne` | Not equal |
| `lt` | Signed less than | `le` | Signed less than or equal |
| `gt` | Signed greater than | `ge` | Signed greater than or equal |
| `lo` / `cc` | Unsigned lower | `ls` | Unsigned lower or same |
| `hi` | Unsigned higher | `hs` / `cs` | Unsigned higher or same |
| `mi` | Negative | `pl` | Non-negative |
| `vs` | Overflow | `vc` | No overflow |
| `al` | Always | | |

---

### Printing and I/O

| Instruction | What it does |
|---|---|
| `lds Rd, "text"` | Load a string literal into a register |
| `puts Rn` | Print the string in `Rn` to stdout |
| `geti Rd` | Read an integer from stdin into `Rd` |
| `gets Rd` | Read a line of text from stdin into `Rd` |
| `dmp Rn` | Debug-print the integer and string value of `Rn` to stderr |

```asm
lds  x0, "What is your name? "
puts x0
gets x1          ; read a line into x1
lds  x0, "Hello, "
puts x0
puts x1
```

---

### Data Movement

| Instruction | What it does |
|---|---|
| `mov Rd, Rn` | Copy a register |
| `mov Rd, #imm` | Load an immediate |
| `movz Rd, #imm [, lsl #shift]` | Load immediate, zero other bits |
| `movn Rd, #imm [, lsl #shift]` | Load bitwise-NOT of immediate |
| `movk Rd, #imm [, lsl #shift]` | Insert 16-bit immediate without touching other bits |
| `mvn  Rd, Rn` | Bitwise NOT |

---

### Arithmetic

| Instruction | What it does |
|---|---|
| `add  Rd, Rn, Rm` | `Rd = Rn + Rm` |
| `add  Rd, Rn, #imm` | `Rd = Rn + imm` |
| `adds Rd, Rn, Rm` | Same, and sets flags |
| `sub  Rd, Rn, Rm` | `Rd = Rn - Rm` |
| `subs Rd, Rn, Rm` | Same, and sets flags |
| `neg  Rd, Rn` | `Rd = -Rn` |
| `negs Rd, Rn` | Same, and sets flags |
| `mul  Rd, Rn, Rm` | `Rd = Rn * Rm` |
| `udiv Rd, Rn, Rm` | Unsigned integer divide |
| `sdiv Rd, Rn, Rm` | Signed integer divide |
| `madd Rd, Rn, Rm, Ra` | `Rd = Rn * Rm + Ra` |
| `msub Rd, Rn, Rm, Ra` | `Rd = Ra - Rn * Rm` |
| `mneg Rd, Rn, Rm` | `Rd = -(Rn * Rm)` |
| `adc  Rd, Rn, Rm` | Add with carry |
| `sbc  Rd, Rn, Rm` | Subtract with carry |

Shifts are supported as a third operand on `add`/`sub`:

```asm
add x2, x0, x1, lsl #2    ; x2 = x0 + (x1 << 2)
```

---

### Logical and Bitwise

| Instruction | What it does |
|---|---|
| `and  Rd, Rn, Rm/#imm` | Bitwise AND |
| `ands Rd, Rn, Rm/#imm` | AND and set flags |
| `orr  Rd, Rn, Rm/#imm` | Bitwise OR |
| `orn  Rd, Rn, Rm` | `Rd = Rn \| ~Rm` |
| `eor  Rd, Rn, Rm/#imm` | Bitwise XOR |
| `eon  Rd, Rn, Rm` | `Rd = Rn ^ ~Rm` |
| `bic  Rd, Rn, Rm` | `Rd = Rn & ~Rm` (bit clear) |
| `bics Rd, Rn, Rm` | Same, and sets flags |
| `not  Rd` | Bitwise NOT in place |

---

### Shifts and Rotates

| Instruction | What it does |
|---|---|
| `lsl Rd, Rn, Rm/#imm` | Logical shift left |
| `lsr Rd, Rn, Rm/#imm` | Logical shift right (zero fills) |
| `asr Rd, Rn, Rm/#imm` | Arithmetic shift right (sign extends) |
| `ror Rd, Rn, Rm/#imm` | Rotate right |
| `shl Rd, #imm` | Shift `Rd` left by immediate, in place |
| `shr Rd, #imm` | Shift `Rd` right (logical) by immediate, in place |
| `extr Rd, Rn, Rm, #lsb` | Extract bitfield spanning two registers |

---

### Loads and Stores

```asm
ldr x0, [x1]           ; load 64-bit word from address in x1
ldr x0, [x1, #8]       ; load from x1 + 8
str x0, [x1]           ; store 64-bit word to address in x1
str x0, [x1, #8]       ; store to x1 + 8

ldrb x0, [x1]          ; load unsigned byte
strb x0, [x1]          ; store byte
ldrh x0, [x1]          ; load unsigned halfword (16-bit)
strh x0, [x1]          ; store halfword
ldrsb x0, [x1]         ; load signed byte (sign-extended to 64-bit)
ldrsh x0, [x1]         ; load signed halfword
ldrsw x0, [x1]         ; load signed word (32-bit, sign-extended)

ldp x0, x1, [x2, #16]  ; load pair: x0 ← [x2+16], x1 ← [x2+24]
stp x0, x1, [x2, #-16] ; store pair: [x2-16] ← x0, [x2-8] ← x1

adr  x0, label          ; x0 = address (line number) of label
adrp x0, label          ; same, page-aligned
```

---

### Branches and Control Flow

**Unconditional:**

```asm
b    label      ; jump
jmp  label      ; alias for b
bl   label      ; call (saves return address to lr)
call label      ; alias for bl
br   x0         ; jump to address in x0
blr  x0         ; call address in x0
ret             ; return (jumps to lr)
ret  x0         ; return to address in x0
```

**Conditional branches — set flags first with `cmp`, `adds`, `subs`, etc.:**

```asm
cmp  x0, #10
jlt  loop       ; jump if x0 < 10  (signed)
jge  done       ; jump if x0 >= 10 (signed)
```

Full list of conditional branch mnemonics:

| Mnemonic | Alternate | Condition |
|---|---|---|
| `b.eq` / `beq` / `jeq` | — | Equal |
| `b.ne` / `bne` / `jne` | — | Not equal |
| `b.lt` / `blt` / `jlt` | — | Signed less than |
| `b.le` / `ble` / `jle` | — | Signed ≤ |
| `b.gt` / `bgt` / `jgt` | — | Signed greater than |
| `b.ge` / `bge` / `jge` | — | Signed ≥ |
| `b.lo` / `blo` | — | Unsigned lower |
| `b.ls` / `bls` | — | Unsigned lower or same |
| `b.hi` / `bhi` | — | Unsigned higher |
| `b.hs` / `bhs` | — | Unsigned higher or same |
| `b.mi` / `bmi` | — | Negative |
| `b.pl` / `bpl` | — | Non-negative |
| `b.vs` / `bvs` | — | Overflow |
| `b.vc` / `bvc` | — | No overflow |
| `b.al` / `bal` | — | Always |

**Register-based conditional branches:**

```asm
cbz  x0, label    ; branch if x0 == 0
cbnz x0, label    ; branch if x0 != 0
tbz  x0, #3, label  ; branch if bit 3 of x0 is 0
tbnz x0, #3, label  ; branch if bit 3 of x0 is 1
```

---

### Comparison

These set the flags but don't write a result register.

```asm
cmp x0, x1       ; sets flags for x0 - x1
cmp x0, #42      ; sets flags for x0 - 42
cmn x0, x1       ; sets flags for x0 + x1
tst x0, x1       ; sets flags for x0 & x1
```

---

### Conditional Select

These let you pick between two values without a branch.

```asm
csel  x0, x1, x2, eq   ; x0 = (eq) ? x1 : x2
csinc x0, x1, x2, eq   ; x0 = (eq) ? x1 : x2 + 1
csinv x0, x1, x2, eq   ; x0 = (eq) ? x1 : ~x2
csneg x0, x1, x2, eq   ; x0 = (eq) ? x1 : -x2
cset  x0, eq            ; x0 = (eq) ? 1 : 0
csetm x0, eq            ; x0 = (eq) ? -1 : 0
cinc  x0, x1, eq        ; x0 = (eq) ? x1+1 : x1
cinv  x0, x1, eq        ; x0 = (eq) ? ~x1 : x1
cneg  x0, x1, eq        ; x0 = (eq) ? -x1 : x1
```

---

### Multiply Extended

```asm
smull x0, w1, w2    ; x0 = (int32)w1 * (int32)w2  (64-bit result)
umull x0, w1, w2    ; x0 = (uint32)w1 * (uint32)w2
smulh x0, x1, x2   ; x0 = high 64 bits of signed 128-bit product
umulh x0, x1, x2   ; x0 = high 64 bits of unsigned 128-bit product
smaddl x0, w1, w2, x3   ; x0 = (int32)w1 * (int32)w2 + x3
umaddl x0, w1, w2, x3   ; unsigned equivalent
smsubl x0, w1, w2, x3   ; x0 = x3 - (int32)w1 * (int32)w2
umsubl x0, w1, w2, x3   ; unsigned equivalent
```

---

### Bit Field Operations

```asm
clz   x1, x0          ; count leading zeros
cls   x1, x0          ; count leading sign bits
rbit  x1, x0          ; reverse all bits
rev   x1, x0          ; reverse bytes (64-bit)
rev16 x1, x0          ; reverse bytes within each 16-bit lane
rev32 x1, x0          ; reverse bytes within each 32-bit half
rev64 x1, x0          ; reverse bytes (same as rev)

ubfx  x1, x0, #lsb, #width  ; extract unsigned bit field
sbfx  x1, x0, #lsb, #width  ; extract signed bit field (sign-extended)
ubfiz x1, x0, #lsb, #width  ; insert unsigned field into zeros
sbfiz x1, x0, #lsb, #width  ; insert signed field into zeros
bfi   x1, x0, #lsb, #width  ; insert bit field (other bits unchanged)
bfxil x1, x0, #lsb, #width  ; extract and insert low
bfm   x1, x0, #immr, #imms  ; raw bit field move (signed)
sbfm  x1, x0, #immr, #imms  ; signed bit field move
ubfm  x1, x0, #immr, #imms  ; unsigned bit field move
```

---

### Sign and Zero Extension

```asm
sxtb x1, x0    ; sign-extend byte  (8-bit → 64-bit)
sxth x1, x0    ; sign-extend halfword (16-bit → 64-bit)
sxtw x1, x0    ; sign-extend word (32-bit → 64-bit)
uxtb x1, x0    ; zero-extend byte
uxth x1, x0    ; zero-extend halfword
```

---

### Floating Point

```asm
fmov d0, d1           ; copy float register
fmov d0, #3.14        ; load float immediate

fadd d2, d0, d1       ; d2 = d0 + d1
fsub d2, d0, d1       ; d2 = d0 - d1
fmul d2, d0, d1       ; d2 = d0 * d1
fdiv d2, d0, d1       ; d2 = d0 / d1
fabs d1, d0           ; d1 = |d0|
fneg d1, d0           ; d1 = -d0
fsqrt d1, d0          ; d1 = sqrt(d0)
fmin d2, d0, d1       ; d2 = min(d0, d1)
fmax d2, d0, d1       ; d2 = max(d0, d1)

fcmp d0, d1           ; compare (sets flags)
fcmp d0, #0.0         ; compare with zero

fcsel d0, d1, d2, gt  ; d0 = (gt) ? d1 : d2

fmadd d0, d1, d2, d3  ; d0 = d1*d2 + d3
fmsub d0, d1, d2, d3  ; d0 = -(d1*d2) + d3

; Conversion
scvtf d0, x0          ; integer → float (signed)
ucvtf d0, x0          ; integer → float (unsigned)
fcvtzs x0, d0         ; float → integer (truncate toward zero, signed)
fcvtzu x0, d0         ; float → integer (truncate toward zero, unsigned)
fcvtms x0, d0         ; float → integer (floor)
fcvtps x0, d0         ; float → integer (ceiling)
fcvtns x0, d0         ; float → integer (round to nearest)
```

---

### String and Memory Utilities

These are high-level custom instructions that operate on the string value attached to a register.

| Instruction | What it does |
|---|---|
| `strlen Rd, Rn` | `Rd = length of string in Rn` |
| `adl    Rd, Rn` | Same as `strlen` |
| `strcpy Rd, Rn` | Copy string from `Rn` into `Rd` |
| `strcat Rd, Rn` | Append string in `Rn` onto string in `Rd` |
| `strcmp Rd, Rn, Rm` | `Rd = 0` if equal, `< 0` if `Rn < Rm`, `> 0` if `Rn > Rm`; also sets Z/N flags |
| `itoa   Rd, Rn` | Convert integer in `Rn` to decimal string, store in `Rd` |
| `atoi   Rd, Rn` | Parse decimal string in `Rn` as integer, store in `Rd` |
| `memcpy Rd, Rn, Rm` | Copy `Rm` bytes from `Rn` to `Rd` |
| `memset Rd, Rn, Rm` | Fill `Rm` bytes at `Rd` with low byte of `Rn` |

```asm
; Print an integer
itoa x1, x0
puts x1

; Concatenate two strings
lds  x0, "Hello, "
lds  x1, "World!"
strcat x0, x1
puts x0

; Compare strings
strcmp x3, x0, x1
jeq   they_are_equal
```

---

### Register Utilities

| Instruction | What it does |
|---|---|
| `clrr Rd` | Set `Rd` to zero |
| `inc  Rd` | `Rd = Rd + 1` |
| `dec  Rd` | `Rd = Rd - 1` |
| `abs  Rd` | `Rd = \|Rd\|` |
| `not  Rd` | `Rd = ~Rd` (bitwise NOT, in place) |
| `swp  Rn, Rm` | Swap values of two registers |
| `max2 Rd, Rn, Rm` | `Rd = max(Rn, Rm)` |
| `min2 Rd, Rn, Rm` | `Rd = min(Rn, Rm)` |
| `mod  Rd, Rn, Rm` | `Rd = Rn % Rm` |
| `rand Rd` | Fill `Rd` with a random integer |
| `time Rd` | Fill `Rd` with current time in milliseconds |
| `halt` | Stop the program |

---

### Atomic and Exclusive Access

These simulate atomic memory operations (sequentially consistent in the single-threaded VM).

```asm
ldar  x0, [x1]        ; load-acquire
ldaxr x0, [x1]        ; load-acquire exclusive
ldxr  x0, [x1]        ; load exclusive
stlr  x0, [x1]        ; store-release
stlxr x2, x0, [x1]   ; store-release exclusive (x2 = 0 on success)
stxr  x2, x0, [x1]   ; store exclusive
ldarb / ldarh / ldaxrb / ldaxrh / ldxrb / ldxrh   ; byte/halfword variants
stlrb / stlrh / stlxrb / stlxrh / stxrb / stxrh  ; byte/halfword variants
ldxp  x0, x1, [x2]   ; load exclusive pair
stxp  x3, x0, x1, [x2]  ; store exclusive pair
```

---

### System Instructions

```asm
nop               ; do nothing
svc #0            ; supervisor call (syscall)
hlt #0            ; halt with code
brk #0            ; breakpoint
wfe / wfi         ; wait for event / interrupt (no-op in VM)
sev / sevl        ; send event / send event local (no-op in VM)
isb / dsb / dmb   ; barriers (no-op in VM)
clrex / yield / eret / drps   ; (no-op in VM)
mrs x0, nzcv      ; read system register → always returns 0
msr nzcv, x0      ; write system register (no-op in VM)
```

---

### Stack

The VM provides a simple built-in value stack separate from memory.

```asm
push x0    ; push x0 onto the stack
pop  x0    ; pop top of stack into x0
```

Subroutine calls (`bl` / `call`) use the VM's internal call stack, not the value stack, so you don't need to manage return addresses manually.

---

### Data Directives

These embed data into the bytecode stream. The VM steps over them at runtime.

```asm
.word  #0xDEADBEEF   ; embed a 32-bit value
.dword #0x123456789  ; embed a 64-bit value
.byte  #255          ; embed a single byte
.space #64           ; emit 64 zero bytes
.ascii "hello"       ; embed raw string (no null terminator)
.asciz "hello"       ; embed null-terminated string
.align #4            ; pad with NOPs to the next 4-instruction boundary
```

---

## Opcode Table

Every bytecode opcode, for reference. You only need this if you're writing tooling that processes `.wassm` files directly.

| # | Mnemonic | # | Mnemonic | # | Mnemonic | # | Mnemonic |
|---|---|---|---|---|---|---|---|
| 0 | lds | 1 | adl | 2 | mov | 3 | ldr |
| 4 | str | 5 | add | 6 | sub | 7 | mul |
| 8 | udiv | 9 | sdiv | 10 | and | 11 | orr |
| 12 | eor | 13 | mvn | 14 | lsl | 15 | lsr |
| 16 | asr | 17 | ror | 18 | b | 19 | bl |
| 20 | br | 21 | blr | 22 | ret | 23 | cbz |
| 24 | cbnz | 25 | tbz | 26 | tbnz | 27 | b.eq |
| 28 | b.ne | 29 | b.lt | 30 | b.le | 31 | b.gt |
| 32 | b.ge | 33 | b.lo | 34 | b.ls | 35 | b.hi |
| 36 | b.hs | 37 | b.mi | 38 | b.pl | 39 | b.vs |
| 40 | b.vc | 41 | b.al | 42 | cmp | 43 | cmn |
| 44 | tst | 45 | neg | 46 | negs | 47 | sbc |
| 48 | sbcs | 49 | adc | 50 | adcs | 51 | adds |
| 52 | subs | 53 | madd | 54 | msub | 55 | mneg |
| 56 | smull | 57 | umull | 58 | smulh | 59 | umulh |
| 60 | smaddl | 61 | umaddl | 62 | smsubl | 63 | umsubl |
| 64 | ldrb | 65 | ldrh | 66 | ldrsb | 67 | ldrsh |
| 68 | ldrsw | 69 | strb | 70 | strh | 71 | ldp |
| 72 | stp | 73 | adrp | 74 | adr | 75 | nop |
| 76 | svc | 77 | hlt | 78 | brk | 79 | wfe |
| 80 | wfi | 81 | sev | 82 | sevl | 83 | isb |
| 84 | dsb | 85 | dmb | 86 | clrex | 87 | yield |
| 88 | eret | 89 | drps | 90 | mrs | 91 | msr |
| 92 | sys | 93 | sysl | 94 | ic | 95 | dc |
| 96 | at | 97 | tlbi | 98 | clz | 99 | cls |
| 100 | rbit | 101 | rev | 102 | rev16 | 103 | rev32 |
| 104 | rev64 | 105 | extr | 106 | sbfm | 107 | bfm |
| 108 | ubfm | 109 | sbfx | 110 | sbfiz | 111 | bfxil |
| 112 | bfi | 113 | ubfx | 114 | ubfiz | 115 | sxtb |
| 116 | sxth | 117 | sxtw | 118 | uxtb | 119 | uxth |
| 120 | ands | 121 | bics | 122 | bic | 123 | eon |
| 124 | orn | 125 | movz | 126 | movn | 127 | movk |
| 128 | fmov | 129 | fadd | 130 | fsub | 131 | fmul |
| 132 | fdiv | 133 | fabs | 134 | fneg | 135 | fsqrt |
| 136 | fcmp | 137 | fcmpe | 138 | fccmp | 139 | fccmpe |
| 140 | fcsel | 141 | fcvt | 142 | fcvtas | 143 | fcvtau |
| 144 | fcvtms | 145 | fcvtmu | 146 | fcvtns | 147 | fcvtnu |
| 148 | fcvtps | 149 | fcvtpu | 150 | fcvtzs | 151 | fcvtzu |
| 152 | scvtf | 153 | ucvtf | 154 | fmadd | 155 | fmsub |
| 156 | fnmadd | 157 | fnmsub | 158 | fminnm | 159 | fmaxnm |
| 160 | fmin | 161 | fmax | 162 | csel | 163 | csinc |
| 164 | csinv | 165 | csneg | 166 | cset | 167 | csetm |
| 168 | cinc | 169 | cinv | 170 | cneg | 171 | ccmp |
| 172 | ccmn | 173 | ldar | 174 | ldarb | 175 | ldarh |
| 176 | ldaxr | 177 | ldaxrb | 178 | ldaxrh | 179 | ldxr |
| 180 | ldxrb | 181 | ldxrh | 182 | stlr | 183 | stlrb |
| 184 | stlrh | 185 | stlxr | 186 | stlxrb | 187 | stlxrh |
| 188 | stxr | 189 | stxrb | 190 | stxrh | 191 | ldxp |
| 192 | ldaxp | 193 | stxp | 194 | stlxp | 195 | prfm |
| 196 | prfum | 197 | ldraa | 198 | ldrab | 199 | ldr_literal |
| 200 | push | 201 | pop | 203 | swp | 204 | dmp |
| 205 | halt | 206 | puts | 207 | geti | 208 | gets |
| 209 | rand | 210 | time | 211 | clrr | 212 | inc |
| 213 | dec | 214 | abs | 215 | max2 | 216 | min2 |
| 217 | mod | 218 | not | 219 | shl | 220 | shr |
| 221 | memcpy | 222 | memset | 223 | strcpy | 224 | strcat |
| 225 | strcmp | 226 | strlen | 227 | itoa | 228 | atoi |
| 230 | call | 231 | jmp | 232 | jeq | 233 | jne |
| 234 | jlt | 235 | jle | 236 | jgt | 237 | jge |
| 238 | .align | 239 | .word | 240 | .dword | 241 | .byte |
| 242 | .space | 243 | .ascii | 244 | .asciz | | |

Opcode 229 is a label marker used internally by the assembler and is never written to the output file.

---

## Bytecode Format

`.wassm` files are plain text — one instruction per line. You can read them with any text editor.

**Header:**

```
; wlbi Bytecode v1.0
; Source: hello.iwa
; Lines: 3
;;;
```

**Instruction lines:**

Each line starts with the opcode number followed by space-separated operand tokens:

- Registers: `family:number` — e.g. `0:0` = `x0`, `1:1` = `w1`, `2:0` = `sp`
- Immediates: plain decimal integers
- Mode flags: `R` (register operand), `I` (immediate), `M` (memory)
- Strings: spaces encoded as `\~`, all other escapes (`\n`, `\t`, `\\`) are kept as-is
- Labels: resolved to the integer line index of the target instruction

**Example — the "Hello, World" program above:**

```
; wlbi Bytecode v1.0
; Source: hello.iwa
; Lines: 3
;;;
0 0:0 Hello,\~World!\n
206 0:0
205
```

---

## Limits

| Item | Limit |
|---|---|
| Source line length | 4 096 characters |
| Tokens per line | 64 |
| Labels | 4 096 |
| Label forward references | 8 192 |
| Output instructions | 65 536 |
| Token length | 512 characters |
| General registers | x0–x30, w0–w30 |
| Float registers | d0–d31, s0–s31, q0–q31, v0–v31 |

---

## Error Reference

| Message | Cause |
|---|---|
| `Line N: bad register 'X'` | Token is not a valid register name |
| `Line N: unknown mnemonic 'X'` | Instruction name not recognized |
| `Line N: lds needs 2 operands` | Too few operands for the instruction |
| `Error: undefined label 'X'` | A branch or call targets a label that was never defined |
| `Error: too many labels` | Label table full (limit: 4 096) |
| `Error: too many fixups` | Forward-reference table full (limit: 8 192) |
| `Error: too many instructions` | Output buffer full (limit: 65 536 lines) |
| `Error: cannot open 'X'` | Input or output file could not be opened |
| `Internal error: FIXUP marker missing` | Compiler internal consistency failure |

All errors go to stderr. The assembler exits with status 1 on any error.

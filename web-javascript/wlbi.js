'use strict';

(function (root, factory) {
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = factory();
  } else {
    root.webassembler = factory();
  }
}(typeof globalThis !== 'undefined' ? globalThis : this, function () {

  const OP = Object.freeze({
    LDS:0, ADL:1, MOV:2, LDR:3, STR:4, ADD:5, SUB:6, MUL:7,
    UDIV:8, SDIV:9, AND:10, ORR:11, EOR:12, MVN:13, LSL:14, LSR:15,
    ASR:16, ROR:17, B:18, BL:19, BR:20, BLR:21, RET:22, CBZ:23, CBNZ:24,
    TBZ:25, TBNZ:26, BEQ:27, BNE:28, BLT:29, BLE:30, BGT:31, BGE:32,
    BLO:33, BLS:34, BHI:35, BHS:36, BMI:37, BPL:38, BVS:39, BVC:40,
    BAL:41, CMP:42, CMN:43, TST:44, NEG:45, NEGS:46, SBC:47, SBCS:48,
    ADC:49, ADCS:50, ADDS:51, SUBS:52, MADD:53, MSUB:54, MNEG:55,
    SMULL:56, UMULL:57, SMULH:58, UMULH:59, SMADDL:60, UMADDL:61,
    SMSUBL:62, UMSUBL:63, LDRB:64, LDRH:65, LDRSB:66, LDRSH:67,
    LDRSW:68, STRB:69, STRH:70, LDP:71, STP:72, ADRP:73, ADR:74,
    NOP:75, SVC:76, HLT:77, BRK:78, WFE:79, WFI:80, SEV:81, SEVL:82,
    ISB:83, DSB:84, DMB:85, CLREX:86, YIELD:87, ERET:88, DRPS:89,
    MRS:90, MSR:91, SYS:92, SYSL:93, IC:94, DC:95, AT:96, TLBI:97,
    CLZ:98, CLS:99, RBIT:100, REV:101, REV16:102, REV32:103, REV64:104,
    EXTR:105, SBFM:106, BFM:107, UBFM:108, SBFX:109, SBFIZ:110,
    BFXIL:111, BFI:112, UBFX:113, UBFIZ:114, SXTB:115, SXTH:116,
    SXTW:117, UXTB:118, UXTH:119, ANDS:120, BICS:121, BIC:122,
    EON:123, ORN:124, MOVZ:125, MOVN:126, MOVK:127, FMOV:128,
    FADD:129, FSUB:130, FMUL:131, FDIV:132, FABS:133, FNEG:134,
    FSQRT:135, FCMP:136, FCMPE:137, FCCMP:138, FCCMPE:139, FCSEL:140,
    FCVT:141, FCVTAS:142, FCVTAU:143, FCVTMS:144, FCVTMU:145,
    FCVTNS:146, FCVTNU:147, FCVTPS:148, FCVTPU:149, FCVTZS:150,
    FCVTZU:151, SCVTF:152, UCVTF:153, FMADD:154, FMSUB:155,
    FNMADD:156, FNMSUB:157, FMINNM:158, FMAXNM:159, FMIN:160, FMAX:161,
    CSEL:162, CSINC:163, CSINV:164, CSNEG:165, CSET:166, CSETM:167,
    CINC:168, CINV:169, CNEG:170, CCMP:171, CCMN:172, LDAR:173,
    LDARB:174, LDARH:175, LDAXR:176, LDAXRB:177, LDAXRH:178, LDXR:179,
    LDXRB:180, LDXRH:181, STLR:182, STLRB:183, STLRH:184, STLXR:185,
    STLXRB:186, STLXRH:187, STXR:188, STXRB:189, STXRH:190, LDXP:191,
    LDAXP:192, STXP:193, STLXP:194, PRFM:195, PRFUM:196, LDRAA:197,
    LDRAB:198, LDR_LIT:199, PUSH:200, POP:201,
    SWP:203, DMP:204, HALT:205, PUTS:206, GETI:207, GETS:208,
    RAND:209, TIME:210, CLRR:211, INC:212, DEC:213, ABS:214,
    MAX2:215, MIN2:216, MOD:217, NOT:218, SHL:219, SHR:220,
    MEMCPY:221, MEMSET:222, STRCPY:223, STRCAT_OP:224, STRCMP_OP:225,
    STRLEN:226, ITOA:227, ATOI:228,
    CALL:230, JMP:231, JEQ:232, JNE:233, JLT:234, JLE:235, JGT:236,
    JGE:237, ALIGN:238, WORD:239, DWORD:240, BYTE_D:241, SPACE:242,
    ASCII:243, ASCIZ:244,
  });

  /* =========================================================
   * Constants
   * ========================================================= */
  const U64_MAX  = (1n << 64n) - 1n;
  const U64_MASK = U64_MAX;
  const I64_MIN  = -(1n << 63n);
  const I64_MAX  = (1n << 63n) - 1n;

  function unescapeStr(s) {
    // Fast path: skip the loop entirely when there are no escapes
    if (!s.includes('\\')) return s;

    let out = '';
    let i = 0;
    const len = s.length;
    while (i < len) {
      if (s[i] === '\\' && i + 1 < len) {
        i++;
        switch (s[i]) {
          case '~':  out += ' ';    break;  // space encoding
          case 'n':  out += '\n';   break;
          case 't':  out += '\t';   break;
          case 'r':  out += '\r';   break;
          case '0':  out += '\0';   break;
          case 'a':  out += '\x07'; break;  // BEL
          case 'b':  out += '\x08'; break;  // BS
          case '\\': out += '\\';   break;
          default:   out += s[i];   break;
        }
      } else {
        out += s[i];
      }
      i++;
    }
    return out;
  }

  function evalCond(cond, flags) {
    const { N, Z, C, V } = flags;
    switch ((cond || '').toLowerCase()) {
      case 'eq':            return Z;
      case 'ne':            return !Z;
      case 'cs': case 'hs': return C;
      case 'cc': case 'lo': return !C;
      case 'mi':            return N;
      case 'pl':            return !N;
      case 'vs':            return V;
      case 'vc':            return !V;
      case 'hi':            return C && !Z;
      case 'ls':            return !C || Z;
      case 'ge':            return N === V;
      case 'lt':            return N !== V;
      case 'gt':            return !Z && (N === V);
      case 'le':            return Z || (N !== V);
      case 'al': case 'nv': return true;
      default:              return false;
    }
  }

  function setNZ(flags, result) {
    const i64 = BigInt.asIntN(64, result);
    flags.N = i64 < 0n;
    flags.Z = i64 === 0n;
  }

  /**
   * Performs addition with full flag update (N, Z, C, V).
   *
   * Carry:    computed in unsigned domain — set when the true sum overflows 64 bits.
   * Overflow: set when two same-sign operands produce an opposite-sign result.
   *           Note: overflow uses the original `b`, not `b + cin`, to correctly
   *           model the AArch64 two's-complement overflow rule.
   *
   * @param {object} flags - NZCV flags object, mutated in place
   * @param {bigint} a     - first operand (any 64-bit pattern)
   * @param {bigint} b     - second operand (any 64-bit pattern)
   * @param {bigint} cin   - carry-in (0n or 1n)
   * @returns {bigint}       unsigned 64-bit result
   */
  function addWithFlags(flags, a, b, cin = 0n) {
    const ua = BigInt.asUintN(64, a);
    const ub = BigInt.asUintN(64, b);
    const ur = ua + ub + cin;

    const u64 = ur & U64_MASK;
    const i64 = BigInt.asIntN(64, u64);

    flags.N = i64 < 0n;
    flags.Z = u64 === 0n;
    flags.C = ur > U64_MASK;          // unsigned overflow → carry

    // Signed overflow: same-sign inputs, different-sign output
    const sa = BigInt.asIntN(64, a) < 0n;
    const sb = BigInt.asIntN(64, b) < 0n;  // original b, not b+cin
    const sr = i64 < 0n;
    flags.V = (sa === sb) && (sa !== sr);

    return u64;
  }

  /**
   * Performs subtraction (a - b) via two's complement addition.
   * a - b  ≡  a + (~b) + 1
   * This correctly models AArch64's borrow/carry convention where
   * C=1 means no borrow (carry is the logical inverse of borrow).
   */
  function subWithFlags(flags, a, b) {
    return addWithFlags(flags, a, ~b & U64_MASK, 1n);
  }

  /**
   * Applies a named shift/rotate to a 64-bit value.
   * Returns the unshifted value when type is absent/none or amount is 0.
   */
  function applyShift(val, type, amt) {
    if (!type || type === 'none' || amt === 0n) return val;
    const v = BigInt.asUintN(64, val);
    switch ((type || '').toLowerCase()) {
      case 'lsl': return v << amt;
      case 'lsr': return v >> amt;
      case 'asr': return BigInt.asIntN(64, val) >> amt;
      case 'ror': {
        const s = amt % 64n;
        return s === 0n ? v : (v >> s) | (v << (64n - s));
      }
      default: return val;
    }
  }

  /* =========================================================
   * Bit-manipulation helpers
   * ========================================================= */
  /** Count leading zeros in a 64-bit unsigned value. */
  function clz64(v) {
    v = BigInt.asUintN(64, v);
    if (v === 0n) return 64n;
    let n = 0n;
    for (let bit = 63n; bit >= 0n; bit--) {
      if ((v >> bit) & 1n) break;
      n++;
    }
    return n;
  }

  /** Count leading sign bits (bits matching the MSB), excluding MSB itself. */
  function cls64(v) {
    const i64 = BigInt.asIntN(64, v);
    const sign = i64 < 0n ? 1n : 0n;
    let n = 0n;
    for (let bit = 62n; bit >= 0n; bit--) {
      if (((i64 >> bit) & 1n) === sign) n++;
      else break;
    }
    return n;
  }

  /** Reverse the order of all 64 bits. */
  function rbit64(v) {
    v = BigInt.asUintN(64, v);
    let r = 0n;
    for (let i = 0n; i < 64n; i++) { r = (r << 1n) | (v & 1n); v >>= 1n; }
    return r;
  }

  /** Reverse byte order of a 64-bit value. */
  function rev64(v) {
    v = BigInt.asUintN(64, v);
    let r = 0n;
    for (let i = 0n; i < 8n; i++) { r = (r << 8n) | (v & 0xffn); v >>= 8n; }
    return r;
  }

  /** Reverse bytes within each 32-bit half of a 64-bit value. */
  function rev32in64(v) {
    const lo = rev64(v & 0xFFFFFFFFn) >> 32n;
    const hi = rev64((v >> 32n) & 0xFFFFFFFFn) >> 32n;
    return (hi << 32n) | lo;
  }

  /** Reverse bytes within each 16-bit quarter of a 64-bit value. */
  function rev16in64(v) {
    let r = 0n;
    for (let i = 0n; i < 4n; i++) {
      const seg = (v >> (i * 16n)) & 0xFFFFn;
      r |= (((seg & 0xFFn) << 8n) | ((seg >> 8n) & 0xFFn)) << (i * 16n);
    }
    return r;
  }

  /* =========================================================
   * Register file + Memory
   * ========================================================= */
  /**
   * Register encoding scheme (family:number):
   *   0  = x0–x30  (64-bit general purpose, signed store)
   *   1  = w0–w30  (32-bit, zero-extended on write)
   *   2  = sp      (stack pointer)
   *   3  = lr/x30  (link register)
   *   4  = xzr     (zero register, reads 0, writes discarded)
   *   5,6 = reserved zeros
   *   7  = fp/x29  (frame pointer alias)
   *   8  = ip0/x16
   *   9  = ip1/x17
   *   10 = d0–d31  (64-bit float)
   *   11 = s0–s31  (32-bit float, stored as f64 with fround)
   */
  class Registers {
    constructor() {
      this.x   = new Array(31).fill(0n);  // x0–x30
      this.d   = new Array(32).fill(0.0); // d0–d31 / s0–s31
      this.sp  = 0n;
      this.lr  = 0n;
      // NZCV flags
      this.N   = false;
      this.Z   = false;
      this.C   = false;
      this.V   = false;
      // VM-level pseudo stack (PUSH/POP opcodes)
      this.stack   = [];
      // Byte-addressed memory as a Map<string, bigint> (8-byte aligned words)
      this.mem     = new Map();
      // Per-register string values (for high-level string instructions)
      this.strings = new Map();
    }

    /**
     * Parse a register encoding string "family:number" into its components.
     * Returns [family, number], caching the split to avoid repeated string ops.
     */
    _parseEnc(enc) {
      const colon = enc.indexOf(':');
      if (colon < 0) return [0, 0];
      return [Number(enc.charCodeAt(0) - 48), Number(enc.slice(colon + 1))];
      // Note: family is a single digit (0–9), so charCodeAt is safe here.
      // Fall back to Number() for robustness:
    }

    readInt(enc) {
      const colon = enc.indexOf(':');
      if (colon < 0) return 0n;
      const fam = Number(enc.slice(0, colon));
      const num = Number(enc.slice(colon + 1));
      switch (fam) {
        case 0: return this.x[num] ?? 0n;
        case 1: return BigInt.asUintN(32, this.x[num] ?? 0n);
        case 2: return this.sp;
        case 3: return this.lr;
        case 4: case 5: case 6: return 0n;
        case 7: return this.x[29] ?? 0n;
        case 8: return this.x[16] ?? 0n;
        case 9: return this.x[17] ?? 0n;
        default: return 0n;
      }
    }

    writeInt(enc, val) {
      // Normalise value to bigint early to avoid scattered coercions below
      if (typeof val === 'number') val = BigInt(Math.trunc(val));
      else if (typeof val !== 'bigint') val = 0n;

      const colon = enc.indexOf(':');
      if (colon < 0) return;
      const fam = Number(enc.slice(0, colon));
      const num = Number(enc.slice(colon + 1));
      switch (fam) {
        case 0: this.x[num]  = BigInt.asIntN(64, val);   break;
        case 1: this.x[num]  = BigInt.asUintN(32, val);  break;  // zero-extend
        case 2: this.sp      = val;                       break;
        case 3: this.lr      = val;                       break;
        case 4: case 5: case 6:                           break;  // discard
        case 7: this.x[29]   = val;                       break;
        case 8: this.x[16]   = val;                       break;
        case 9: this.x[17]   = val;                       break;
        default:                                           break;
      }
    }

    readFloat(enc) {
      const colon = enc.indexOf(':');
      const fam = colon < 0 ? 10 : Number(enc.slice(0, colon));
      const num = colon < 0 ?  0 : Number(enc.slice(colon + 1));
      if (fam === 10) return this.d[num] ?? 0.0;
      if (fam === 11) return Math.fround(this.d[num] ?? 0.0);
      return 0.0;
    }

    writeFloat(enc, val) {
      const colon = enc.indexOf(':');
      const fam = colon < 0 ? 10 : Number(enc.slice(0, colon));
      const num = colon < 0 ?  0 : Number(enc.slice(colon + 1));
      if (fam === 10) { this.d[num] = val;               return; }
      if (fam === 11) { this.d[num] = Math.fround(val);  return; }
    }

    /** Returns true when the register encoding denotes a floating-point register. */
    isFloat(enc) {
      const colon = enc.indexOf(':');
      if (colon < 0) return false;
      const fam = Number(enc.slice(0, colon));
      return fam === 10 || fam === 11 || fam === 12 || fam === 13;
    }

    readStr(enc)     { return this.strings.get(enc) ?? null; }
    writeStr(enc, s) {
      if (s === null) this.strings.delete(enc);
      else            this.strings.set(enc, s);
    }

    /* ── Memory access ───────────────────────────────────────── */

    /**
     * Read a 64-bit word from byte address `addr` (must be 8-byte aligned).
     * Unwritten locations read as 0.
     */
    memRead(addr) {
      return this.mem.get(String(addr)) ?? 0n;
    }

    /**
     * Write a 64-bit word to byte address `addr` (must be 8-byte aligned).
     */
    memWrite(addr, val) {
      this.mem.set(String(addr), BigInt.asIntN(64, val));
    }

    /**
     * Read a single byte from an arbitrary byte address.
     * Locates the containing 8-byte word and extracts the correct byte.
     */
    memReadByte(addr) {
      const base = (addr / 8n) * 8n;
      const off  = Number(addr % 8n);
      return (this.memRead(base) >> BigInt(off * 8)) & 0xffn;
    }

    /**
     * Write a single byte to an arbitrary byte address.
     * Performs a read-modify-write on the containing 8-byte word.
     */
    memWriteByte(addr, val) {
      const base  = (addr / 8n) * 8n;
      const shift = BigInt(Number(addr % 8n) * 8);
      const word  = this.memRead(base);
      this.memWrite(base, (word & ~(0xffn << shift)) | ((val & 0xffn) << shift));
    }

    /**
     * Read a 16-bit little-endian halfword from an arbitrary byte address.
     */
    memReadHalf(addr) {
      return this.memReadByte(addr) | (this.memReadByte(addr + 1n) << 8n);
    }

    /**
     * Write a 16-bit little-endian halfword to an arbitrary byte address.
     */
    memWriteHalf(addr, val) {
      this.memWriteByte(addr,       val & 0xffn);
      this.memWriteByte(addr + 1n, (val >> 8n) & 0xffn);
    }
  }

  /* =========================================================
   * Parse .wassm source → array of token arrays
   * ========================================================= */
  /**
   * Splits .wassm bytecode source into an array of instruction token arrays.
   * Blank lines and lines starting with ';' are skipped entirely.
   * Each instruction is pre-split on spaces so the VM never calls split() at runtime.
   *
   * @param {string} src - raw .wassm source text
   * @returns {string[][]} array of token arrays, one per instruction
   */
  function parseWassm(src) {
    const lines = src.split('\n');
    const code  = [];
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line || line.charCodeAt(0) === 59 /* ';' */) continue;
      const tokens = line.split(' ');
      if (tokens.length > 0 && tokens[0] !== '') code.push(tokens);
    }
    return code;
  }

  /* =========================================================
   * Async VM
   * ========================================================= */
  /**
   * The virtual machine.  One VM instance is created per `execute()` call
   * and is not reused — state is completely isolated between runs.
   */
  class VM {
    /**
     * @param {string[][]} code     - parsed instruction token arrays
     * @param {object}     apiHooks - { onOutput, onError, onInput }
     */
    constructor(code, apiHooks) {
      this.code      = code;
      this.pc        = 0;
      this.regs      = new Registers();
      this.running   = true;
      this.callStack = [];
      this.api       = apiHooks;
    }

    emit(text)    { this.api.onOutput(text); }
    emitErr(text) { this.api.onError(text);  }

    async readLine() {
      return this.api.onInput ? await this.api.onInput() : '';
    }

    /**
     * Run the program to completion (or until HALT).
     * Yields to the event loop every `yieldEvery` steps to keep the browser responsive.
     *
     * @param {number} [yieldEvery=50000] - steps between event-loop yields
     * @returns {Promise<void>}
     */
    async run(yieldEvery = 50_000) {
      yieldEvery = Math.max(1, Math.min(1_000_000, yieldEvery | 0));
      let steps = 0;
      const code = this.code;
      while (this.running && this.pc >= 0 && this.pc < code.length) {
        const instr = code[this.pc];
        if (!instr || instr.length === 0) { this.pc++; continue; }
        await this.step(instr);
        if ((++steps & (yieldEvery - 1)) === 0) {
          // Only yield when yieldEvery is a power of two; otherwise use modulo.
          // For safety we always use setTimeout to allow cancellation.
          await new Promise(r => setTimeout(r, 0));
        }
      }
    }

    /* -------------------------------------------------------
     * Instruction dispatch
     * ------------------------------------------------------- */
    async step(tok) {
      const op   = Number(tok[0]);
      const regs = this.regs;

      switch (op) {

        /* ── String load ─────────────────────────────────────── */
        case OP.LDS: {
          const str = unescapeStr(tok[2] || '');
          regs.writeStr(tok[1], str);
          regs.writeInt(tok[1], BigInt(str.length));
          this.pc++; break;
        }

        /* ── Address-of-label / string-length ────────────────── */
        case OP.ADL:
        case OP.STRLEN: {
          const s = regs.readStr(tok[2] || '');
          regs.writeInt(tok[1], s !== null ? BigInt(s.length) : 0n);
          this.pc++; break;
        }

        /* ── Move ─────────────────────────────────────────────── */
        case OP.MOV: {
          if (tok[2] === 'R') {
            regs.writeInt(tok[1], regs.readInt(tok[3]));
            regs.writeStr(tok[1], regs.readStr(tok[3]));
          } else {
            regs.writeInt(tok[1], BigInt(tok[3]));
            regs.writeStr(tok[1], null);
          }
          this.pc++; break;
        }

        case OP.MOVZ: {
          const imm = BigInt(tok[2]), shift = BigInt(tok[3]);
          regs.writeInt(tok[1], (imm & 0xFFFFn) << shift);
          this.pc++; break;
        }
        case OP.MOVN: {
          const imm = BigInt(tok[2]), shift = BigInt(tok[3]);
          regs.writeInt(tok[1], ~((imm & 0xFFFFn) << shift));
          this.pc++; break;
        }
        case OP.MOVK: {
          const imm = BigInt(tok[2]), shift = BigInt(tok[3]);
          const cur = regs.readInt(tok[1]);
          const mask = 0xFFFFn << shift;
          regs.writeInt(tok[1], (cur & ~mask) | ((imm & 0xFFFFn) << shift));
          this.pc++; break;
        }
        case OP.MVN:
          regs.writeInt(tok[1], ~regs.readInt(tok[2]));
          this.pc++; break;

        /* ── Load / Store ─────────────────────────────────────── */
        case OP.LDR: {
          if (tok[2] === 'R') {
            // High-level register-to-register load (copies string handle too)
            regs.writeInt(tok[1], regs.readInt(tok[3]));
            regs.writeStr(tok[1], regs.readStr(tok[3]));
          } else {
            // Memory load: tok[3] = base register, tok[4] = byte offset
            const addr = regs.readInt(tok[3]) + BigInt(tok[4] || 0);
            regs.writeInt(tok[1], regs.memRead(addr));
          }
          this.pc++; break;
        }
        case OP.STR:
          regs.memWrite(regs.readInt(tok[2]) + BigInt(tok[3] || 0), regs.readInt(tok[1]));
          this.pc++; break;

        case OP.LDRB:
          regs.writeInt(tok[1], regs.memReadByte(regs.readInt(tok[2]) + BigInt(tok[3] || 0)));
          this.pc++; break;

        case OP.LDRH:
          regs.writeInt(tok[1], regs.memReadHalf(regs.readInt(tok[2]) + BigInt(tok[3] || 0)));
          this.pc++; break;

        case OP.LDRSB:
          regs.writeInt(tok[1], BigInt.asIntN(8,
            regs.memReadByte(regs.readInt(tok[2]) + BigInt(tok[3] || 0))));
          this.pc++; break;

        case OP.LDRSH:
          regs.writeInt(tok[1], BigInt.asIntN(16,
            regs.memReadHalf(regs.readInt(tok[2]) + BigInt(tok[3] || 0))));
          this.pc++; break;

        case OP.LDRSW: {
          const addr = regs.readInt(tok[2]) + BigInt(tok[3] || 0);
          let w = 0n;
          for (let i = 0n; i < 4n; i++) w |= regs.memReadByte(addr + i) << (i * 8n);
          regs.writeInt(tok[1], BigInt.asIntN(32, w));
          this.pc++; break;
        }

        case OP.STRB:
          regs.memWriteByte(regs.readInt(tok[2]) + BigInt(tok[3] || 0),
            regs.readInt(tok[1]) & 0xffn);
          this.pc++; break;

        case OP.STRH:
          regs.memWriteHalf(regs.readInt(tok[2]) + BigInt(tok[3] || 0), regs.readInt(tok[1]));
          this.pc++; break;

        case OP.LDP: {
          const base = regs.readInt(tok[3]), off = BigInt(tok[4] || 0);
          regs.writeInt(tok[1], regs.memRead(base + off));
          regs.writeInt(tok[2], regs.memRead(base + off + 8n));
          this.pc++; break;
        }
        case OP.STP: {
          const base = regs.readInt(tok[3]), off = BigInt(tok[4] || 0);
          regs.memWrite(base + off,      regs.readInt(tok[1]));
          regs.memWrite(base + off + 8n, regs.readInt(tok[2]));
          this.pc++; break;
        }

        /* ── Arithmetic ───────────────────────────────────────── */
        case OP.ADD:
        case OP.ADDS: {
          const a = regs.readInt(tok[2]);
          const b = tok[3] === 'R'
            ? applyShift(regs.readInt(tok[4]), tok[5], BigInt(tok[6] || 0))
            : BigInt(tok[4]);
          if (op === OP.ADDS) {
            regs.writeInt(tok[1], addWithFlags(regs, a, b));
          } else {
            regs.writeInt(tok[1], a + b);
          }
          this.pc++; break;
        }

        case OP.SUB:
        case OP.SUBS: {
          const a = regs.readInt(tok[2]);
          const b = tok[3] === 'R'
            ? applyShift(regs.readInt(tok[4]), tok[5], BigInt(tok[6] || 0))
            : BigInt(tok[4]);
          if (op === OP.SUBS) {
            regs.writeInt(tok[1], subWithFlags(regs, a, b));
          } else {
            regs.writeInt(tok[1], a - b);
          }
          this.pc++; break;
        }

        case OP.MUL:
          regs.writeInt(tok[1], regs.readInt(tok[2]) * regs.readInt(tok[3]));
          this.pc++; break;

        case OP.UDIV: {
          const a = BigInt.asUintN(64, regs.readInt(tok[2]));
          const b = BigInt.asUintN(64, regs.readInt(tok[3]));
          regs.writeInt(tok[1], b === 0n ? 0n : a / b);
          this.pc++; break;
        }
        case OP.SDIV: {
          const a = BigInt.asIntN(64, regs.readInt(tok[2]));
          const b = BigInt.asIntN(64, regs.readInt(tok[3]));
          regs.writeInt(tok[1], b === 0n ? 0n : a / b);
          this.pc++; break;
        }

        /* ── Logical ──────────────────────────────────────────── */
        case OP.AND:
        case OP.ANDS: {
          const a = regs.readInt(tok[2]);
          const b = tok[3] === 'R' ? regs.readInt(tok[4]) : BigInt(tok[4]);
          const res = a & b;
          regs.writeInt(tok[1], res);
          if (op === OP.ANDS) setNZ(regs, res);
          this.pc++; break;
        }
        case OP.ORR: {
          const b = tok[3] === 'R' ? regs.readInt(tok[4]) : BigInt(tok[4]);
          regs.writeInt(tok[1], regs.readInt(tok[2]) | b);
          this.pc++; break;
        }
        case OP.EOR: {
          const b = tok[3] === 'R' ? regs.readInt(tok[4]) : BigInt(tok[4]);
          regs.writeInt(tok[1], regs.readInt(tok[2]) ^ b);
          this.pc++; break;
        }
        case OP.EON:  regs.writeInt(tok[1], regs.readInt(tok[2]) ^ ~regs.readInt(tok[3])); this.pc++; break;
        case OP.ORN:  regs.writeInt(tok[1], regs.readInt(tok[2]) | ~regs.readInt(tok[3])); this.pc++; break;
        case OP.BIC:  regs.writeInt(tok[1], regs.readInt(tok[2]) & ~regs.readInt(tok[3])); this.pc++; break;
        case OP.BICS: {
          const res = regs.readInt(tok[2]) & ~regs.readInt(tok[3]);
          regs.writeInt(tok[1], res);
          setNZ(regs, res);
          this.pc++; break;
        }

        /* ── Shifts ───────────────────────────────────────────── */
        case OP.LSL: {
          const sh = tok[3] === 'R' ? (regs.readInt(tok[4]) & 63n) : BigInt(tok[4]);
          regs.writeInt(tok[1], BigInt.asUintN(64, regs.readInt(tok[2])) << sh);
          this.pc++; break;
        }
        case OP.LSR: {
          const sh = tok[3] === 'R' ? (regs.readInt(tok[4]) & 63n) : BigInt(tok[4]);
          regs.writeInt(tok[1], BigInt.asUintN(64, regs.readInt(tok[2])) >> sh);
          this.pc++; break;
        }
        case OP.ASR: {
          const sh = tok[3] === 'R' ? (regs.readInt(tok[4]) & 63n) : BigInt(tok[4]);
          regs.writeInt(tok[1], BigInt.asIntN(64, regs.readInt(tok[2])) >> sh);
          this.pc++; break;
        }
        case OP.ROR: {
          const a = BigInt.asUintN(64, regs.readInt(tok[2]));
          const s = (tok[3] === 'R'
            ? regs.readInt(tok[4]) & 63n
            : BigInt(tok[4])) % 64n;
          regs.writeInt(tok[1], s === 0n ? a : (a >> s) | (a << (64n - s)));
          this.pc++; break;
        }

        /* ── Compare / Test ───────────────────────────────────── */
        case OP.CMP: {
          const b = tok[2] === 'R' ? regs.readInt(tok[3]) : BigInt(tok[3]);
          subWithFlags(regs, regs.readInt(tok[1]), b);
          this.pc++; break;
        }
        case OP.CMN: {
          const b = tok[2] === 'R' ? regs.readInt(tok[3]) : BigInt(tok[3]);
          addWithFlags(regs, regs.readInt(tok[1]), b);
          this.pc++; break;
        }
        case OP.TST: {
          const b = tok[2] === 'R' ? regs.readInt(tok[3]) : BigInt(tok[3]);
          setNZ(regs, regs.readInt(tok[1]) & b);
          this.pc++; break;
        }

        /* ── Negate / Carry / Overflow ────────────────────────── */
        case OP.NEG:
          regs.writeInt(tok[1], -regs.readInt(tok[2]));
          this.pc++; break;
        case OP.NEGS: {
          const r = subWithFlags(regs, 0n, regs.readInt(tok[2]));
          regs.writeInt(tok[1], r);
          this.pc++; break;
        }
        case OP.ADC:
          regs.writeInt(tok[1],
            regs.readInt(tok[2]) + regs.readInt(tok[3]) + (regs.C ? 1n : 0n));
          this.pc++; break;
        case OP.ADCS: {
          const r = addWithFlags(regs, regs.readInt(tok[2]), regs.readInt(tok[3]),
                                  regs.C ? 1n : 0n);
          regs.writeInt(tok[1], r);
          this.pc++; break;
        }
        case OP.SBC:
          regs.writeInt(tok[1],
            regs.readInt(tok[2]) - regs.readInt(tok[3]) - (regs.C ? 0n : 1n));
          this.pc++; break;
        case OP.SBCS: {
          // SBC: result = a - b - ~C = a + ~b + C
          const r = addWithFlags(regs, regs.readInt(tok[2]),
                                  ~regs.readInt(tok[3]) & U64_MASK,
                                  regs.C ? 1n : 0n);
          regs.writeInt(tok[1], r);
          this.pc++; break;
        }

        /* ── Multiply-accumulate ──────────────────────────────── */
        case OP.MADD:
          regs.writeInt(tok[1],
            regs.readInt(tok[2]) * regs.readInt(tok[3]) + regs.readInt(tok[4]));
          this.pc++; break;
        case OP.MSUB:
          regs.writeInt(tok[1],
            regs.readInt(tok[4]) - regs.readInt(tok[2]) * regs.readInt(tok[3]));
          this.pc++; break;
        case OP.MNEG:
          regs.writeInt(tok[1], -(regs.readInt(tok[2]) * regs.readInt(tok[3])));
          this.pc++; break;
        case OP.SMULL:
          regs.writeInt(tok[1],
            BigInt.asIntN(32, regs.readInt(tok[2])) * BigInt.asIntN(32, regs.readInt(tok[3])));
          this.pc++; break;
        case OP.UMULL:
          regs.writeInt(tok[1],
            BigInt.asUintN(32, regs.readInt(tok[2])) * BigInt.asUintN(32, regs.readInt(tok[3])));
          this.pc++; break;
        case OP.SMULH:
          regs.writeInt(tok[1],
            (BigInt.asIntN(64, regs.readInt(tok[2])) * BigInt.asIntN(64, regs.readInt(tok[3]))) >> 64n);
          this.pc++; break;
        case OP.UMULH:
          regs.writeInt(tok[1],
            (BigInt.asUintN(64, regs.readInt(tok[2])) * BigInt.asUintN(64, regs.readInt(tok[3]))) >> 64n);
          this.pc++; break;
        case OP.SMADDL:
          regs.writeInt(tok[1],
            BigInt.asIntN(32, regs.readInt(tok[2])) * BigInt.asIntN(32, regs.readInt(tok[3])) + regs.readInt(tok[4]));
          this.pc++; break;
        case OP.UMADDL:
          regs.writeInt(tok[1],
            BigInt.asUintN(32, regs.readInt(tok[2])) * BigInt.asUintN(32, regs.readInt(tok[3])) + regs.readInt(tok[4]));
          this.pc++; break;
        case OP.SMSUBL:
          regs.writeInt(tok[1],
            regs.readInt(tok[4]) - BigInt.asIntN(32, regs.readInt(tok[2])) * BigInt.asIntN(32, regs.readInt(tok[3])));
          this.pc++; break;
        case OP.UMSUBL:
          regs.writeInt(tok[1],
            regs.readInt(tok[4]) - BigInt.asUintN(32, regs.readInt(tok[2])) * BigInt.asUintN(32, regs.readInt(tok[3])));
          this.pc++; break;

        /* ── Branches ─────────────────────────────────────────── */
        case OP.B:   this.pc = Number(tok[1]); break;
        case OP.BL:
          regs.lr = BigInt(this.pc + 1);
          this.callStack.push(this.pc + 1);
          this.pc = Number(tok[1]);
          break;
        case OP.BR:  this.pc = Number(regs.readInt(tok[1])); break;
        case OP.BLR:
          regs.lr = BigInt(this.pc + 1);
          this.callStack.push(this.pc + 1);
          this.pc = Number(regs.readInt(tok[1]));
          break;
        case OP.RET: {
          // tok.length > 1 means an explicit return-address register was given
          const retAddr = tok.length > 1 ? regs.readInt(tok[1]) : regs.lr;
          if (this.callStack.length > 0) {
            this.pc = this.callStack.pop();
          } else {
            this.pc = Number(retAddr);
            if (this.pc < 0 || this.pc >= this.code.length) this.running = false;
          }
          break;
        }

        /* Conditional branches — all use the same pattern */
        case OP.BEQ: this.pc = regs.Z              ? Number(tok[1]) : this.pc + 1; break;
        case OP.BNE: this.pc = !regs.Z             ? Number(tok[1]) : this.pc + 1; break;
        case OP.BLT: this.pc = (regs.N !== regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.BLE: this.pc = (regs.Z || regs.N !== regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.BGT: this.pc = (!regs.Z && regs.N === regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.BGE: this.pc = (regs.N === regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.BLO: this.pc = !regs.C             ? Number(tok[1]) : this.pc + 1; break;
        case OP.BLS: this.pc = (!regs.C || regs.Z) ? Number(tok[1]) : this.pc + 1; break;
        case OP.BHI: this.pc = (regs.C && !regs.Z) ? Number(tok[1]) : this.pc + 1; break;
        case OP.BHS: this.pc = regs.C              ? Number(tok[1]) : this.pc + 1; break;
        case OP.BMI: this.pc = regs.N              ? Number(tok[1]) : this.pc + 1; break;
        case OP.BPL: this.pc = !regs.N             ? Number(tok[1]) : this.pc + 1; break;
        case OP.BVS: this.pc = regs.V              ? Number(tok[1]) : this.pc + 1; break;
        case OP.BVC: this.pc = !regs.V             ? Number(tok[1]) : this.pc + 1; break;
        case OP.BAL: this.pc = Number(tok[1]); break;

        case OP.CBZ:  this.pc = regs.readInt(tok[1]) === 0n ? Number(tok[2]) : this.pc + 1; break;
        case OP.CBNZ: this.pc = regs.readInt(tok[1]) !== 0n ? Number(tok[2]) : this.pc + 1; break;
        case OP.TBZ: {
          const b = BigInt(tok[2]);
          this.pc = ((regs.readInt(tok[1]) >> b) & 1n) === 0n ? Number(tok[3]) : this.pc + 1;
          break;
        }
        case OP.TBNZ: {
          const b = BigInt(tok[2]);
          this.pc = ((regs.readInt(tok[1]) >> b) & 1n) !== 0n ? Number(tok[3]) : this.pc + 1;
          break;
        }

        case OP.ADR:
        case OP.ADRP:
          regs.writeInt(tok[1], BigInt(Number(tok[2])));
          this.pc++; break;

        /* ── SVC (system call) ────────────────────────────────── */
        case OP.SVC: {
          const no = Number(regs.readInt('0:8'));  // syscall number in x8
          switch (no) {
            case 1:    // write (stdout)
            case 64: { // write (AArch64 Linux)
              const s = regs.readStr('0:1');
              this.emit(s !== null ? s : regs.readInt('0:1').toString());
              break;
            }
            case 63: { // read (AArch64 Linux)
              const line = await this.readLine();
              regs.writeStr('0:0', line);
              regs.writeInt('0:0', BigInt(line.length));
              break;
            }
            case 60:   // exit
            case 93:   // exit_group
              this.running = false;
              break;
            default:
              this.emitErr(`[SVC] Unhandled syscall ${no}`);
              break;
          }
          this.pc++; break;
        }

        /* ── Halt / Break ─────────────────────────────────────── */
        case OP.HLT:
        case OP.BRK:
          this.running = false;
          this.pc++; break;

        /* ── No-ops / privileged stubs ────────────────────────── */
        case OP.NOP: case OP.WFE: case OP.WFI: case OP.SEV: case OP.SEVL:
        case OP.ISB: case OP.DSB: case OP.DMB: case OP.CLREX: case OP.YIELD:
        case OP.ERET: case OP.DRPS:
          this.pc++; break;

        case OP.MRS: regs.writeInt(tok[1], 0n); this.pc++; break;
        case OP.MSR: case OP.SYS: case OP.SYSL:
        case OP.IC:  case OP.DC:  case OP.AT:   case OP.TLBI:
          this.pc++; break;

        /* ── Bit ops ──────────────────────────────────────────── */
        case OP.CLZ:   regs.writeInt(tok[1], clz64(regs.readInt(tok[2]))); this.pc++; break;
        case OP.CLS:   regs.writeInt(tok[1], cls64(regs.readInt(tok[2]))); this.pc++; break;
        case OP.RBIT:  regs.writeInt(tok[1], rbit64(regs.readInt(tok[2]))); this.pc++; break;
        case OP.REV:   regs.writeInt(tok[1], rev64(regs.readInt(tok[2]))); this.pc++; break;
        case OP.REV16: regs.writeInt(tok[1], rev16in64(regs.readInt(tok[2]))); this.pc++; break;
        case OP.REV32: regs.writeInt(tok[1], rev32in64(regs.readInt(tok[2]))); this.pc++; break;
        case OP.REV64: regs.writeInt(tok[1], rev64(regs.readInt(tok[2]))); this.pc++; break;

        case OP.EXTR: {
          const n   = BigInt.asUintN(64, regs.readInt(tok[2]));
          const m   = BigInt.asUintN(64, regs.readInt(tok[3]));
          const lsb = BigInt(tok[4]);
          regs.writeInt(tok[1], ((n << 64n) | m) >> lsb);
          this.pc++; break;
        }

        /* ── Bitfield ─────────────────────────────────────────── */
        case OP.SBFM: {
          const src = regs.readInt(tok[2]), immR = BigInt(tok[3]), immS = BigInt(tok[4]);
          const width = immS + 1n;
          let field = (src >> immR) & ((1n << width) - 1n);
          if ((field >> (width - 1n)) & 1n) field |= ~((1n << width) - 1n);
          regs.writeInt(tok[1], field);
          this.pc++; break;
        }
        case OP.UBFM: {
          const src = regs.readInt(tok[2]), immR = BigInt(tok[3]), immS = BigInt(tok[4]);
          regs.writeInt(tok[1], (src >> immR) & ((1n << (immS + 1n)) - 1n));
          this.pc++; break;
        }
        case OP.BFM: {
          const src = regs.readInt(tok[2]), immR = BigInt(tok[3]), immS = BigInt(tok[4]);
          const width = immS + 1n, field = (src >> immR) & ((1n << width) - 1n);
          const mask = ((1n << width) - 1n) << immR;
          regs.writeInt(tok[1], (regs.readInt(tok[1]) & ~mask) | (field << immR));
          this.pc++; break;
        }
        case OP.SBFX: {
          const s = regs.readInt(tok[2]), lsb = BigInt(tok[3]), w = BigInt(tok[4]);
          let f = (s >> lsb) & ((1n << w) - 1n);
          if ((f >> (w - 1n)) & 1n) f |= ~((1n << w) - 1n);
          regs.writeInt(tok[1], f);
          this.pc++; break;
        }
        case OP.UBFX: {
          const s = regs.readInt(tok[2]), lsb = BigInt(tok[3]), w = BigInt(tok[4]);
          regs.writeInt(tok[1], (s >> lsb) & ((1n << w) - 1n));
          this.pc++; break;
        }
        case OP.SBFIZ: {
          const s = regs.readInt(tok[2]), lsb = BigInt(tok[3]), w = BigInt(tok[4]);
          let f = s & ((1n << w) - 1n);
          if ((f >> (w - 1n)) & 1n) f |= ~((1n << w) - 1n);
          regs.writeInt(tok[1], f << lsb);
          this.pc++; break;
        }
        case OP.UBFIZ: {
          const s = regs.readInt(tok[2]), lsb = BigInt(tok[3]), w = BigInt(tok[4]);
          regs.writeInt(tok[1], (s & ((1n << w) - 1n)) << lsb);
          this.pc++; break;
        }
        case OP.BFXIL: {
          const s = regs.readInt(tok[2]), lsb = BigInt(tok[3]), w = BigInt(tok[4]);
          const f = (s >> lsb) & ((1n << w) - 1n), mask = (1n << w) - 1n;
          regs.writeInt(tok[1], (regs.readInt(tok[1]) & ~mask) | f);
          this.pc++; break;
        }
        case OP.BFI: {
          const s = regs.readInt(tok[2]), lsb = BigInt(tok[3]), w = BigInt(tok[4]);
          const f = s & ((1n << w) - 1n), mask = ((1n << w) - 1n) << lsb;
          regs.writeInt(tok[1], (regs.readInt(tok[1]) & ~mask) | (f << lsb));
          this.pc++; break;
        }

        /* ── Sign/Zero extension ──────────────────────────────── */
        case OP.SXTB: regs.writeInt(tok[1], BigInt.asIntN(8,  regs.readInt(tok[2]))); this.pc++; break;
        case OP.SXTH: regs.writeInt(tok[1], BigInt.asIntN(16, regs.readInt(tok[2]))); this.pc++; break;
        case OP.SXTW: regs.writeInt(tok[1], BigInt.asIntN(32, regs.readInt(tok[2]))); this.pc++; break;
        case OP.UXTB: regs.writeInt(tok[1], BigInt.asUintN(8,  regs.readInt(tok[2]))); this.pc++; break;
        case OP.UXTH: regs.writeInt(tok[1], BigInt.asUintN(16, regs.readInt(tok[2]))); this.pc++; break;

        /* ── Floating-point ───────────────────────────────────── */
        case OP.FMOV: {
          if (tok[2] === 'R') {
            if (regs.isFloat(tok[3])) regs.writeFloat(tok[1], regs.readFloat(tok[3]));
            else                       regs.writeFloat(tok[1], Number(regs.readInt(tok[3])));
          } else {
            regs.writeFloat(tok[1], parseFloat(tok[3]));
          }
          this.pc++; break;
        }
        case OP.FADD:  regs.writeFloat(tok[1], regs.readFloat(tok[2]) + regs.readFloat(tok[3])); this.pc++; break;
        case OP.FSUB:  regs.writeFloat(tok[1], regs.readFloat(tok[2]) - regs.readFloat(tok[3])); this.pc++; break;
        case OP.FMUL:  regs.writeFloat(tok[1], regs.readFloat(tok[2]) * regs.readFloat(tok[3])); this.pc++; break;
        case OP.FDIV:  regs.writeFloat(tok[1], regs.readFloat(tok[2]) / regs.readFloat(tok[3])); this.pc++; break;
        case OP.FABS:  regs.writeFloat(tok[1], Math.abs(regs.readFloat(tok[2]))); this.pc++; break;
        case OP.FNEG:  regs.writeFloat(tok[1], -regs.readFloat(tok[2])); this.pc++; break;
        case OP.FSQRT: regs.writeFloat(tok[1], Math.sqrt(regs.readFloat(tok[2]))); this.pc++; break;

        case OP.FCMP:
        case OP.FCMPE: {
          const a = regs.readFloat(tok[1]);
          const b = tok[2] === 'R' ? regs.readFloat(tok[3]) : 0.0;
          const nan = isNaN(a) || isNaN(b);
          regs.Z = !nan && a === b;
          regs.N = !nan && a < b;
          regs.C = nan || a >= b;  // unordered sets C=1, V=1
          regs.V = nan;
          this.pc++; break;
        }
        case OP.FCCMP:
        case OP.FCCMPE: {
          if (evalCond(tok[4], regs)) {
            const a = regs.readFloat(tok[1]), b = regs.readFloat(tok[2]);
            const nan = isNaN(a) || isNaN(b);
            regs.Z = !nan && a === b;
            regs.N = !nan && a < b;
            regs.C = nan || a >= b;
            regs.V = nan;
          } else {
            const n = Number(tok[3]);
            regs.N = !!(n & 8); regs.Z = !!(n & 4); regs.C = !!(n & 2); regs.V = !!(n & 1);
          }
          this.pc++; break;
        }
        case OP.FCSEL:
          regs.writeFloat(tok[1],
            evalCond(tok[4], regs) ? regs.readFloat(tok[2]) : regs.readFloat(tok[3]));
          this.pc++; break;
        case OP.FCVT:  regs.writeFloat(tok[1], regs.readFloat(tok[2])); this.pc++; break;

        case OP.FCVTAS: regs.writeInt(tok[1], BigInt(Math.round(regs.readFloat(tok[2])))); this.pc++; break;
        case OP.FCVTAU: regs.writeInt(tok[1], BigInt(Math.abs(Math.round(regs.readFloat(tok[2]))))); this.pc++; break;
        case OP.FCVTMS: regs.writeInt(tok[1], BigInt(Math.floor(regs.readFloat(tok[2])))); this.pc++; break;
        case OP.FCVTMU: regs.writeInt(tok[1], BigInt(Math.abs(Math.floor(regs.readFloat(tok[2]))))); this.pc++; break;
        case OP.FCVTNS: regs.writeInt(tok[1], BigInt(Math.round(regs.readFloat(tok[2])))); this.pc++; break;
        case OP.FCVTNU: regs.writeInt(tok[1], BigInt(Math.abs(Math.round(regs.readFloat(tok[2]))))); this.pc++; break;
        case OP.FCVTPS: regs.writeInt(tok[1], BigInt(Math.ceil(regs.readFloat(tok[2])))); this.pc++; break;
        case OP.FCVTPU: regs.writeInt(tok[1], BigInt(Math.abs(Math.ceil(regs.readFloat(tok[2]))))); this.pc++; break;
        case OP.FCVTZS: regs.writeInt(tok[1], BigInt(Math.trunc(regs.readFloat(tok[2])))); this.pc++; break;
        case OP.FCVTZU: regs.writeInt(tok[1], BigInt(Math.abs(Math.trunc(regs.readFloat(tok[2]))))); this.pc++; break;
        case OP.SCVTF:  regs.writeFloat(tok[1], Number(BigInt.asIntN(64, regs.readInt(tok[2])))); this.pc++; break;
        case OP.UCVTF:  regs.writeFloat(tok[1], Number(BigInt.asUintN(64, regs.readInt(tok[2])))); this.pc++; break;

        case OP.FMADD:  regs.writeFloat(tok[1],  regs.readFloat(tok[2]) * regs.readFloat(tok[3]) + regs.readFloat(tok[4])); this.pc++; break;
        case OP.FMSUB:  regs.writeFloat(tok[1], -(regs.readFloat(tok[2]) * regs.readFloat(tok[3])) + regs.readFloat(tok[4])); this.pc++; break;
        case OP.FNMADD: regs.writeFloat(tok[1], -(regs.readFloat(tok[2]) * regs.readFloat(tok[3]) + regs.readFloat(tok[4]))); this.pc++; break;
        case OP.FNMSUB: regs.writeFloat(tok[1],  regs.readFloat(tok[2]) * regs.readFloat(tok[3]) - regs.readFloat(tok[4])); this.pc++; break;
        case OP.FMIN:   regs.writeFloat(tok[1], Math.min(regs.readFloat(tok[2]), regs.readFloat(tok[3]))); this.pc++; break;
        case OP.FMAX:   regs.writeFloat(tok[1], Math.max(regs.readFloat(tok[2]), regs.readFloat(tok[3]))); this.pc++; break;
        case OP.FMINNM: regs.writeFloat(tok[1], Math.min(regs.readFloat(tok[2]), regs.readFloat(tok[3]))); this.pc++; break;
        case OP.FMAXNM: regs.writeFloat(tok[1], Math.max(regs.readFloat(tok[2]), regs.readFloat(tok[3]))); this.pc++; break;

        /* ── Conditional select ───────────────────────────────── */
        case OP.CSEL:  regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : regs.readInt(tok[3])); this.pc++; break;
        case OP.CSINC: regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : regs.readInt(tok[3]) + 1n); this.pc++; break;
        case OP.CSINV: regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : ~regs.readInt(tok[3])); this.pc++; break;
        case OP.CSNEG: regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : -regs.readInt(tok[3])); this.pc++; break;
        case OP.CSET:  regs.writeInt(tok[1], evalCond(tok[2], regs) ? 1n : 0n); this.pc++; break;
        case OP.CSETM: regs.writeInt(tok[1], evalCond(tok[2], regs) ? -1n : 0n); this.pc++; break;
        case OP.CINC: { const v = regs.readInt(tok[2]); regs.writeInt(tok[1], evalCond(tok[3], regs) ? v + 1n : v); this.pc++; break; }
        case OP.CINV: { const v = regs.readInt(tok[2]); regs.writeInt(tok[1], evalCond(tok[3], regs) ? ~v : v); this.pc++; break; }
        case OP.CNEG: { const v = regs.readInt(tok[2]); regs.writeInt(tok[1], evalCond(tok[3], regs) ? -v : v); this.pc++; break; }

        /* ── CCMP / CCMN ──────────────────────────────────────── */
        case OP.CCMP: {
          if (evalCond(tok[4], regs)) {
            const b = tok[2] === 'R' ? regs.readInt(tok[3]) : BigInt(tok[3]);
            subWithFlags(regs, regs.readInt(tok[1]), b);
          } else {
            // Condition false: load NZCV from the immediate field (tok[4] is the condition;
            // the NZCV immediate is in tok[3] when operand 2 is an immediate)
            const n = Number(tok[3]);
            regs.N = !!(n & 8); regs.Z = !!(n & 4); regs.C = !!(n & 2); regs.V = !!(n & 1);
          }
          this.pc++; break;
        }
        case OP.CCMN: {
          if (evalCond(tok[4], regs)) {
            const b = tok[2] === 'R' ? regs.readInt(tok[3]) : BigInt(tok[3]);
            addWithFlags(regs, regs.readInt(tok[1]), b);
          } else {
            const n = Number(tok[3]);
            regs.N = !!(n & 8); regs.Z = !!(n & 4); regs.C = !!(n & 2); regs.V = !!(n & 1);
          }
          this.pc++; break;
        }

        /* ── Atomic / exclusive (simulated single-threaded) ───── */
        case OP.LDAR: case OP.LDAXR: case OP.LDXR:
          regs.writeInt(tok[1], regs.memRead(regs.readInt(tok[2]))); this.pc++; break;
        case OP.LDARB: case OP.LDAXRB: case OP.LDXRB:
          regs.writeInt(tok[1], regs.memReadByte(regs.readInt(tok[2]))); this.pc++; break;
        case OP.LDARH: case OP.LDAXRH: case OP.LDXRH:
          regs.writeInt(tok[1], regs.memReadHalf(regs.readInt(tok[2]))); this.pc++; break;

        case OP.STLR: case OP.STXR: case OP.STLXR: {
          const sidx = op === OP.STLR ? 1 : 2;
          const bidx = op === OP.STLR ? 2 : 3;
          regs.memWrite(regs.readInt(tok[bidx]), regs.readInt(tok[sidx]));
          if (op !== OP.STLR) regs.writeInt(tok[1], 0n);  // success status
          this.pc++; break;
        }
        case OP.STLRB: case OP.STXRB: case OP.STLXRB: {
          const sidx = op === OP.STLRB ? 1 : 2;
          const bidx = op === OP.STLRB ? 2 : 3;
          regs.memWriteByte(regs.readInt(tok[bidx]), regs.readInt(tok[sidx]) & 0xffn);
          if (op !== OP.STLRB) regs.writeInt(tok[1], 0n);
          this.pc++; break;
        }
        case OP.STLRH: case OP.STXRH: case OP.STLXRH: {
          const sidx = op === OP.STLRH ? 1 : 2;
          const bidx = op === OP.STLRH ? 2 : 3;
          regs.memWriteHalf(regs.readInt(tok[bidx]), regs.readInt(tok[sidx]));
          if (op !== OP.STLRH) regs.writeInt(tok[1], 0n);
          this.pc++; break;
        }
        case OP.LDXP: case OP.LDAXP: {
          const base = regs.readInt(tok[3]);
          regs.writeInt(tok[1], regs.memRead(base));
          regs.writeInt(tok[2], regs.memRead(base + 8n));
          this.pc++; break;
        }
        case OP.STXP: case OP.STLXP: {
          const base = regs.readInt(tok[4]);
          regs.memWrite(base,      regs.readInt(tok[2]));
          regs.memWrite(base + 8n, regs.readInt(tok[3]));
          regs.writeInt(tok[1], 0n);
          this.pc++; break;
        }

        case OP.PRFM: case OP.PRFUM: this.pc++; break;
        case OP.LDRAA: case OP.LDRAB:
          regs.writeInt(tok[1], regs.memRead(regs.readInt(tok[2]) + BigInt(tok[3] || 0)));
          this.pc++; break;

        /* ── Stack helpers (VM pseudo-ops) ────────────────────── */
        case OP.PUSH:
          regs.stack.push({ val: regs.readInt(tok[1]), str: regs.readStr(tok[1]) });
          this.pc++; break;
        case OP.POP: {
          const item = regs.stack.pop();
          if (item !== undefined) {
            regs.writeInt(tok[1], item.val);
            regs.writeStr(tok[1], item.str);  // null clears string — writeStr handles that
          }
          this.pc++; break;
        }

        /* ── Swap ─────────────────────────────────────────────── */
        case OP.SWP: {
          const a = regs.readInt(tok[1]), b = regs.readInt(tok[2]);
          const sa = regs.readStr(tok[1]), sb = regs.readStr(tok[2]);
          regs.writeInt(tok[1], b); regs.writeStr(tok[1], sb);
          regs.writeInt(tok[2], a); regs.writeStr(tok[2], sa);
          this.pc++; break;
        }

        /* ── Debug dump ───────────────────────────────────────── */
        case OP.DMP: {
          const v = regs.readInt(tok[1]), s = regs.readStr(tok[1]);
          this.emitErr(`[DMP ${tok[1]}] int=${v} str=${JSON.stringify(s)}`);
          this.pc++; break;
        }

        /* ── HALT ─────────────────────────────────────────────── */
        case OP.HALT:
          this.running = false;
          break;

        /* ── High-level I/O opcodes ───────────────────────────── */
        case OP.PUTS: {
          const s = regs.readStr(tok[1]);
          this.emit(s !== null ? s : regs.readInt(tok[1]).toString());
          this.pc++; break;
        }

        case OP.GETI: {
          const line    = await this.readLine();
          const trimmed = (line || '').trim();
          const parsed  = parseInt(trimmed, 10);
          regs.writeInt(tok[1], BigInt(isNaN(parsed) ? 0 : parsed));
          this.pc++; break;
        }

        case OP.GETS: {
          const line = await this.readLine();
          regs.writeStr(tok[1], line);
          regs.writeInt(tok[1], BigInt(line.length));
          this.pc++; break;
        }

        case OP.RAND:
          regs.writeInt(tok[1], BigInt(Math.floor(Math.random() * Number.MAX_SAFE_INTEGER)));
          this.pc++; break;
        case OP.TIME:
          regs.writeInt(tok[1], BigInt(Date.now()));
          this.pc++; break;

        /* ── Register utilities ───────────────────────────────── */
        case OP.CLRR:
          regs.writeInt(tok[1], 0n);
          regs.writeStr(tok[1], null);
          this.pc++; break;
        case OP.INC:  regs.writeInt(tok[1], regs.readInt(tok[1]) + 1n); this.pc++; break;
        case OP.DEC:  regs.writeInt(tok[1], regs.readInt(tok[1]) - 1n); this.pc++; break;
        case OP.ABS: {
          const v = BigInt.asIntN(64, regs.readInt(tok[1]));
          regs.writeInt(tok[1], v < 0n ? -v : v);
          this.pc++; break;
        }
        case OP.MAX2: {
          const a = regs.readInt(tok[2]), b = regs.readInt(tok[3]);
          regs.writeInt(tok[1], a > b ? a : b);
          this.pc++; break;
        }
        case OP.MIN2: {
          const a = regs.readInt(tok[2]), b = regs.readInt(tok[3]);
          regs.writeInt(tok[1], a < b ? a : b);
          this.pc++; break;
        }
        case OP.MOD: {
          // Truncated-division semantics, matching C's % operator
          const a = regs.readInt(tok[2]), b = regs.readInt(tok[3]);
          regs.writeInt(tok[1], b === 0n ? 0n : a % b);
          this.pc++; break;
        }
        case OP.NOT: regs.writeInt(tok[1], ~regs.readInt(tok[1])); this.pc++; break;
        case OP.SHL: regs.writeInt(tok[1], BigInt.asUintN(64, regs.readInt(tok[1])) << BigInt(tok[2])); this.pc++; break;
        case OP.SHR: regs.writeInt(tok[1], BigInt.asUintN(64, regs.readInt(tok[1])) >> BigInt(tok[2])); this.pc++; break;

        /* ── Memory / string ops ──────────────────────────────── */
        case OP.MEMCPY: {
          const len = Number(regs.readInt(tok[3]));
          const src = regs.readStr(tok[2]);
          if (src !== null) {
            regs.writeStr(tok[1], src.slice(0, len));
          } else {
            const bs = regs.readInt(tok[2]);
            const bd = regs.readInt(tok[1]);
            for (let i = 0n; i < BigInt(len); i++) {
              regs.memWriteByte(bd + i, regs.memReadByte(bs + i));
            }
          }
          this.pc++; break;
        }
        case OP.MEMSET: {
          const len  = Number(regs.readInt(tok[3]));
          const val  = regs.readInt(tok[2]) & 0xffn;
          const base = regs.readInt(tok[1]);
          for (let i = 0n; i < BigInt(len); i++) regs.memWriteByte(base + i, val);
          this.pc++; break;
        }
        case OP.STRCPY: {
          const s = regs.readStr(tok[2]) ?? '';
          regs.writeStr(tok[1], s);
          regs.writeInt(tok[1], BigInt(s.length));
          this.pc++; break;
        }
        case OP.STRCAT_OP: {
          const cat = (regs.readStr(tok[1]) ?? '') + (regs.readStr(tok[2]) ?? '');
          regs.writeStr(tok[1], cat);
          regs.writeInt(tok[1], BigInt(cat.length));
          this.pc++; break;
        }
        case OP.STRCMP_OP: {
          const a = regs.readStr(tok[2]) ?? '', b = regs.readStr(tok[3]) ?? '';
          const r = a < b ? -1n : a > b ? 1n : 0n;
          regs.writeInt(tok[1], r);
          regs.Z = r === 0n;
          regs.N = r < 0n;
          this.pc++; break;
        }
        case OP.ITOA: {
          const v = regs.readInt(tok[2]).toString();
          regs.writeStr(tok[1], v);
          regs.writeInt(tok[1], BigInt(v.length));
          this.pc++; break;
        }
        case OP.ATOI: {
          // Mirrors C atoi: parse leading integer, ignore trailing garbage
          const raw = (regs.readStr(tok[2]) ?? '0').trim();
          const m   = raw.match(/^-?\d+/);
          try {
            regs.writeInt(tok[1], BigInt(m ? m[0] : '0'));
          } catch {
            regs.writeInt(tok[1], 0n);
          }
          this.pc++; break;
        }

        /* ── High-level call/jump ─────────────────────────────── */
        case OP.CALL:
          regs.lr = BigInt(this.pc + 1);
          this.callStack.push(this.pc + 1);
          this.pc = Number(tok[1]);
          break;
        case OP.JMP: this.pc = Number(tok[1]); break;
        case OP.JEQ: this.pc = regs.Z              ? Number(tok[1]) : this.pc + 1; break;
        case OP.JNE: this.pc = !regs.Z             ? Number(tok[1]) : this.pc + 1; break;
        case OP.JLT: this.pc = (regs.N !== regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.JLE: this.pc = (regs.Z || regs.N !== regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.JGT: this.pc = (!regs.Z && regs.N === regs.V) ? Number(tok[1]) : this.pc + 1; break;
        case OP.JGE: this.pc = (regs.N === regs.V) ? Number(tok[1]) : this.pc + 1; break;

        /* ── Data directives (no-ops at runtime) ─────────────── */
        case OP.ALIGN: case OP.WORD: case OP.DWORD: case OP.BYTE_D:
        case OP.SPACE: case OP.ASCII: case OP.ASCIZ:
          this.pc++; break;

        case OP.LDR_LIT:
          regs.writeInt(tok[1], BigInt(tok[2] || 0));
          this.pc++; break;

        default:
          this.emitErr(`[VM] Unknown opcode ${op} at pc=${this.pc}`);
          this.pc++;
          break;
      }
    }
  }

  /* =========================================================
   * Public API
   * ========================================================= */
  const api = {
    _code: null,

    /**
     * Called with text written to stdout.
     * Override this to redirect output to your UI.
     * @param {string} text
     */
    onOutput(text) { console.log(text); },

    /**
     * Called with diagnostic/error text written to stderr.
     * Override this to redirect error output to your UI.
     * @param {string} text
     */
    onError(text) { console.error(text); },

    /**
     * Called whenever the program reads a line of input (GETI / GETS / SVC read).
     * Must return a Promise<string>.
     *
     * The default uses the browser's synchronous `prompt()`, wrapped in a Promise.
     * Replace this with your own async UI hook for a better user experience:
     *
     *   webassembler.onInput = () => new Promise(resolve => {
     *     // show an input field, call resolve(value) when submitted
     *   });
     *
     * @returns {Promise<string>}
     */
    onInput() {
      return Promise.resolve(
        typeof prompt === 'function' ? (prompt('Input:') ?? '') : ''
      );
    },

    /**
     * Load and compile bytecode from a URL via fetch.
     * @param {string}    url       - path or URL to the .wassm file
     * @param {Function} [callback] - called with no arguments when ready
     * @returns {Promise<void>}
     */
    init(url, callback) {
      return fetch(url)
        .then(r => {
          if (!r.ok) throw new Error(`HTTP ${r.status} fetching ${url}`);
          return r.text();
        })
        .then(src => {
          this._code = parseWassm(src);
          if (typeof callback === 'function') callback();
        });
    },

    /**
     * Load bytecode from a raw source string (no network request).
     * @param {string}    src       - .wassm source text
     * @param {Function} [callback] - called asynchronously when ready
     * @returns {Promise<void>}
     */
    initFromString(src, callback) {
      this._code = parseWassm(src);
      if (typeof callback === 'function') Promise.resolve().then(callback);
      return Promise.resolve();
    },

    /**
     * Load bytecode from a File or Blob (e.g. from a file-picker).
     * @param {File|Blob} file
     * @param {Function} [callback]
     * @returns {Promise<void>}
     */
    initFromFile(file, callback) {
      return file.text().then(src => {
        this._code = parseWassm(src);
        if (typeof callback === 'function') callback();
      });
    },

    /**
     * Execute the loaded bytecode.
     * A fresh VM instance is created for each call, so state never leaks between runs.
     *
     * @param {number} [yieldEvery=50000] - instructions between event-loop yields
     * @returns {Promise<void>} resolves when the program halts
     * @throws {Error} if no bytecode has been loaded
     */
    execute(yieldEvery = 50_000) {
      if (!this._code) {
        return Promise.reject(new Error('No bytecode loaded — call init() or initFromString() first.'));
      }
      const vm = new VM(this._code, {
        onOutput: (t) => this.onOutput(t),
        onError:  (t) => this.onError(t),
        onInput:  ()  => this.onInput(),
      });
      return vm.run(yieldEvery);
    },

    /**
     * Returns a copy of the opcode table for introspection or tooling.
     * @returns {object}
     */
    get opcodes() { return { ...OP }; },
  };

  return api;
}));

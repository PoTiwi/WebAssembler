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
    if (!s.includes('\\')) return s;
    let out = '';
    let i = 0;
    const len = s.length;
    while (i < len) {
      if (s[i] === '\\' && i + 1 < len) {
        i++;
        switch (s[i]) {
          case '~':  out += ' ';    break;
          case 'n':  out += '\n';   break;
          case 't':  out += '\t';   break;
          case 'r':  out += '\r';   break;
          case '0':  out += '\0';   break;
          case 'a':  out += '\x07'; break;
          case 'b':  out += '\x08'; break;
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

  function addWithFlags(flags, a, b, cin = 0n) {
    const ua = BigInt.asUintN(64, a);
    const ub = BigInt.asUintN(64, b);
    const ur = ua + ub + cin;
    const u64 = ur & U64_MASK;
    const i64 = BigInt.asIntN(64, u64);
    flags.N = i64 < 0n;
    flags.Z = u64 === 0n;
    flags.C = ur > U64_MASK;
    const sa = BigInt.asIntN(64, a) < 0n;
    const sb = BigInt.asIntN(64, b) < 0n;
    const sr = i64 < 0n;
    flags.V = (sa === sb) && (sa !== sr);
    return u64;
  }

  function subWithFlags(flags, a, b) {
    return addWithFlags(flags, a, ~b & U64_MASK, 1n);
  }

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

  function rbit64(v) {
    v = BigInt.asUintN(64, v);
    let r = 0n;
    for (let i = 0n; i < 64n; i++) { r = (r << 1n) | (v & 1n); v >>= 1n; }
    return r;
  }

  function rev64(v) {
    v = BigInt.asUintN(64, v);
    let r = 0n;
    for (let i = 0n; i < 8n; i++) { r = (r << 8n) | (v & 0xffn); v >>= 8n; }
    return r;
  }

  function rev32in64(v) {
    const lo = rev64(v & 0xFFFFFFFFn) >> 32n;
    const hi = rev64((v >> 32n) & 0xFFFFFFFFn) >> 32n;
    return (hi << 32n) | lo;
  }

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
  class Registers {
    constructor() {
      this.x   = new Array(31).fill(0n);
      this.d   = new Array(32).fill(0.0);
      this.sp  = 0n;
      this.lr  = 0n;
      this.N   = false;
      this.Z   = false;
      this.C   = false;
      this.V   = false;
      this.stack   = [];
      this.mem     = new Map();
      this.strings = new Map();
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
      if (typeof val === 'number') val = BigInt(Math.trunc(val));
      else if (typeof val !== 'bigint') val = 0n;
      const colon = enc.indexOf(':');
      if (colon < 0) return;
      const fam = Number(enc.slice(0, colon));
      const num = Number(enc.slice(colon + 1));
      switch (fam) {
        case 0: this.x[num]  = BigInt.asIntN(64, val);   break;
        case 1: this.x[num]  = BigInt.asUintN(32, val);  break;
        case 2: this.sp      = val;                       break;
        case 3: this.lr      = val;                       break;
        case 4: case 5: case 6:                           break;
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

    memRead(addr)         { return this.mem.get(String(addr)) ?? 0n; }
    memWrite(addr, val)   { this.mem.set(String(addr), BigInt.asIntN(64, val)); }

    memReadByte(addr) {
      const base = (addr / 8n) * 8n;
      const off  = Number(addr % 8n);
      return (this.memRead(base) >> BigInt(off * 8)) & 0xffn;
    }

    memWriteByte(addr, val) {
      const base  = (addr / 8n) * 8n;
      const shift = BigInt(Number(addr % 8n) * 8);
      const word  = this.memRead(base);
      this.memWrite(base, (word & ~(0xffn << shift)) | ((val & 0xffn) << shift));
    }

    memReadHalf(addr) {
      return this.memReadByte(addr) | (this.memReadByte(addr + 1n) << 8n);
    }

    memWriteHalf(addr, val) {
      this.memWriteByte(addr,       val & 0xffn);
      this.memWriteByte(addr + 1n, (val >> 8n) & 0xffn);
    }
  }

  /* =========================================================
   * Parser
   * ========================================================= */
  function parseWassm(src) {
    const lines = src.split('\n');
    const code  = [];
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line || line.charCodeAt(0) === 59) continue;
      const tokens = line.split(' ');
      if (tokens.length > 0 && tokens[0] !== '') code.push(tokens);
    }
    return code;
  }

  /* =========================================================
   * Async VM
   * ========================================================= */
  class VM {
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

    async run(yieldEvery = 50_000) {
      yieldEvery = Math.max(1, Math.min(1_000_000, yieldEvery | 0));
      let steps = 0;
      const code = this.code;
      while (this.running && this.pc >= 0 && this.pc < code.length) {
        const instr = code[this.pc];
        if (!instr || instr.length === 0) { this.pc++; continue; }
        await this.step(instr);
        if ((++steps & (yieldEvery - 1)) === 0) {
          await new Promise(r => setTimeout(r, 0));
        }
      }
    }

    async step(tok) {
      const op   = Number(tok[0]);
      const regs = this.regs;

      switch (op) {
        case OP.LDS: {
          const str = unescapeStr(tok[2] || '');
          regs.writeStr(tok[1], str);
          regs.writeInt(tok[1], BigInt(str.length));
          this.pc++; break;
        }
        case OP.ADL:
        case OP.STRLEN: {
          const s = regs.readStr(tok[2] || '');
          regs.writeInt(tok[1], s !== null ? BigInt(s.length) : 0n);
          this.pc++; break;
        }
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
        case OP.LDR: {
          if (tok[2] === 'R') {
            regs.writeInt(tok[1], regs.readInt(tok[3]));
            regs.writeStr(tok[1], regs.readStr(tok[3]));
          } else {
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
          const r = addWithFlags(regs, regs.readInt(tok[2]),
                                  ~regs.readInt(tok[3]) & U64_MASK,
                                  regs.C ? 1n : 0n);
          regs.writeInt(tok[1], r);
          this.pc++; break;
        }
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
          const retAddr = tok.length > 1 ? regs.readInt(tok[1]) : regs.lr;
          if (this.callStack.length > 0) {
            this.pc = this.callStack.pop();
          } else {
            this.pc = Number(retAddr);
            if (this.pc < 0 || this.pc >= this.code.length) this.running = false;
          }
          break;
        }
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
        case OP.SVC: {
          const no = Number(regs.readInt('0:8'));
          switch (no) {
            case 1:
            case 64: {
              const s = regs.readStr('0:1');
              this.emit(s !== null ? s : regs.readInt('0:1').toString());
              break;
            }
            case 63: {
              const line = await this.readLine();
              regs.writeStr('0:0', line);
              regs.writeInt('0:0', BigInt(line.length));
              break;
            }
            case 60:
            case 93:
              this.running = false;
              break;
            default:
              this.emitErr(`[SVC] Unhandled syscall ${no}`);
              break;
          }
          this.pc++; break;
        }
        case OP.HLT:
        case OP.BRK:
          this.running = false;
          this.pc++; break;
        case OP.NOP: case OP.WFE: case OP.WFI: case OP.SEV: case OP.SEVL:
        case OP.ISB: case OP.DSB: case OP.DMB: case OP.CLREX: case OP.YIELD:
        case OP.ERET: case OP.DRPS:
          this.pc++; break;
        case OP.MRS: regs.writeInt(tok[1], 0n); this.pc++; break;
        case OP.MSR: case OP.SYS: case OP.SYSL:
        case OP.IC:  case OP.DC:  case OP.AT:   case OP.TLBI:
          this.pc++; break;
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
        case OP.SXTB: regs.writeInt(tok[1], BigInt.asIntN(8,  regs.readInt(tok[2]))); this.pc++; break;
        case OP.SXTH: regs.writeInt(tok[1], BigInt.asIntN(16, regs.readInt(tok[2]))); this.pc++; break;
        case OP.SXTW: regs.writeInt(tok[1], BigInt.asIntN(32, regs.readInt(tok[2]))); this.pc++; break;
        case OP.UXTB: regs.writeInt(tok[1], BigInt.asUintN(8,  regs.readInt(tok[2]))); this.pc++; break;
        case OP.UXTH: regs.writeInt(tok[1], BigInt.asUintN(16, regs.readInt(tok[2]))); this.pc++; break;
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
          regs.C = nan || a >= b;
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
        case OP.CSEL:  regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : regs.readInt(tok[3])); this.pc++; break;
        case OP.CSINC: regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : regs.readInt(tok[3]) + 1n); this.pc++; break;
        case OP.CSINV: regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : ~regs.readInt(tok[3])); this.pc++; break;
        case OP.CSNEG: regs.writeInt(tok[1], evalCond(tok[4], regs) ? regs.readInt(tok[2]) : -regs.readInt(tok[3])); this.pc++; break;
        case OP.CSET:  regs.writeInt(tok[1], evalCond(tok[2], regs) ? 1n : 0n); this.pc++; break;
        case OP.CSETM: regs.writeInt(tok[1], evalCond(tok[2], regs) ? -1n : 0n); this.pc++; break;
        case OP.CINC: { const v = regs.readInt(tok[2]); regs.writeInt(tok[1], evalCond(tok[3], regs) ? v + 1n : v); this.pc++; break; }
        case OP.CINV: { const v = regs.readInt(tok[2]); regs.writeInt(tok[1], evalCond(tok[3], regs) ? ~v : v); this.pc++; break; }
        case OP.CNEG: { const v = regs.readInt(tok[2]); regs.writeInt(tok[1], evalCond(tok[3], regs) ? -v : v); this.pc++; break; }
        case OP.CCMP: {
          if (evalCond(tok[4], regs)) {
            const b = tok[2] === 'R' ? regs.readInt(tok[3]) : BigInt(tok[3]);
            subWithFlags(regs, regs.readInt(tok[1]), b);
          } else {
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
          if (op !== OP.STLR) regs.writeInt(tok[1], 0n);
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
        case OP.PUSH:
          regs.stack.push({ val: regs.readInt(tok[1]), str: regs.readStr(tok[1]) });
          this.pc++; break;
        case OP.POP: {
          const item = regs.stack.pop();
          if (item !== undefined) {
            regs.writeInt(tok[1], item.val);
            regs.writeStr(tok[1], item.str);
          }
          this.pc++; break;
        }
        case OP.SWP: {
          const a = regs.readInt(tok[1]), b = regs.readInt(tok[2]);
          const sa = regs.readStr(tok[1]), sb = regs.readStr(tok[2]);
          regs.writeInt(tok[1], b); regs.writeStr(tok[1], sb);
          regs.writeInt(tok[2], a); regs.writeStr(tok[2], sa);
          this.pc++; break;
        }
        case OP.DMP: {
          const v = regs.readInt(tok[1]), s = regs.readStr(tok[1]);
          this.emitErr(`[DMP ${tok[1]}] int=${v} str=${JSON.stringify(s)}`);
          this.pc++; break;
        }
        case OP.HALT:
          this.running = false;
          break;
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
          const a = regs.readInt(tok[2]), b = regs.readInt(tok[3]);
          regs.writeInt(tok[1], b === 0n ? 0n : a % b);
          this.pc++; break;
        }
        case OP.NOT: regs.writeInt(tok[1], ~regs.readInt(tok[1])); this.pc++; break;
        case OP.SHL: regs.writeInt(tok[1], BigInt.asUintN(64, regs.readInt(tok[1])) << BigInt(tok[2])); this.pc++; break;
        case OP.SHR: regs.writeInt(tok[1], BigInt.asUintN(64, regs.readInt(tok[1])) >> BigInt(tok[2])); this.pc++; break;
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
          const raw = (regs.readStr(tok[2]) ?? '0').trim();
          const m   = raw.match(/^-?\d+/);
          try {
            regs.writeInt(tok[1], BigInt(m ? m[0] : '0'));
          } catch {
            regs.writeInt(tok[1], 0n);
          }
          this.pc++; break;
        }
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
   * Internal helpers — encoding strings for the register file
   * ========================================================= */

  /**
   * Converts a user-facing register name into the internal "family:number" encoding.
   *
   * Supported names (case-insensitive):
   *   x0–x30       → "0:N"    (64-bit GPR)
   *   w0–w30       → "1:N"    (32-bit GPR, zero-extended)
   *   sp           → "2:0"    (stack pointer)
   *   lr / x30     → "3:0"    (link register, also aliased as x30)
   *   xzr / wzr    → "4:0"    (zero register)
   *   fp / x29     → "7:0"    (frame pointer)
   *   ip0 / x16    → "8:0"
   *   ip1 / x17    → "9:0"
   *   d0–d31       → "10:N"   (64-bit float)
   *   s0–s31       → "11:N"   (32-bit float)
   *   0–30 (bare)  → "0:N"    (shorthand for xN)
   *
   * @param {string|number} name
   * @returns {string} encoding
   * @throws {Error} on unrecognised register name
   */
  function encodeRegName(name) {
    if (typeof name === 'number') return `0:${name}`;
    const n = String(name).toLowerCase().trim();
    if (n === 'sp')  return '2:0';
    if (n === 'lr')  return '3:0';
    if (n === 'fp')  return '7:0';
    if (n === 'xzr' || n === 'wzr') return '4:0';
    if (n === 'ip0') return '8:0';
    if (n === 'ip1') return '9:0';
    const xm = n.match(/^x(\d+)$/);  if (xm) return `0:${xm[1]}`;
    const wm = n.match(/^w(\d+)$/);  if (wm) return `1:${wm[1]}`;
    const dm = n.match(/^d(\d+)$/);  if (dm) return `10:${dm[1]}`;
    const sm = n.match(/^s(\d+)$/);  if (sm) return `11:${sm[1]}`;
    const nm = n.match(/^\d+$/);     if (nm) return `0:${n}`;
    throw new Error(`[webassembler] Unknown register name: "${name}"`);
  }

  /* =========================================================
   * Public API
   * ========================================================= */

  /**
   * The live VM instance, set during execute() so that introspection
   * namespaces (reg, mem, flags, dbg) can reach it mid-execution or
   * after execution.
   */
  let _liveVM = null;

  function _requireVM() {
    if (!_liveVM) throw new Error('[webassembler] No active or completed VM — call execute() first.');
    return _liveVM;
  }

  /* ---------------------------------------------------------
   * webassembler.reg
   * Register introspection and manipulation.
   * Works during and after execute().
   * --------------------------------------------------------- */
  const reg = {
    /**
     * Fetch the integer value of a general-purpose register.
     * Returns a BigInt.
     *
     * @param {string|number} name  Register name or number (e.g. 0, "x0", "w3", "sp", "lr")
     * @returns {bigint}
     *
     * @example
     * webassembler.reg.fetch(0);        // x0
     * webassembler.reg.fetch('x5');     // x5
     * webassembler.reg.fetch('sp');     // stack pointer
     * webassembler.reg.fetch('lr');     // link register
     */
    fetch(name) {
      return _requireVM().regs.readInt(encodeRegName(name));
    },

    /**
     * Fetch the integer value of a register as a plain JS Number.
     * Loses precision for values outside ±2^53, but convenient for small integers.
     *
     * @param {string|number} name
     * @returns {number}
     */
    fetchNum(name) {
      return Number(_requireVM().regs.readInt(encodeRegName(name)));
    },

    /**
     * Fetch the floating-point value of a SIMD/FP register.
     *
     * @param {string|number} name  e.g. "d0", "s3", or a bare number (treated as dN)
     * @returns {number}
     *
     * @example
     * webassembler.reg.fetchFloat('d0');   // d0
     * webassembler.reg.fetchFloat('s2');   // s2 (single-precision)
     */
    fetchFloat(name) {
      const enc = typeof name === 'number' ? `10:${name}` : encodeRegName(name);
      return _requireVM().regs.readFloat(enc);
    },

    /**
     * Fetch the string value stored in a register (set by LDS / GETS / STRCPY etc.).
     * Returns null if no string is associated with that register.
     *
     * @param {string|number} name
     * @returns {string|null}
     */
    fetchStr(name) {
      return _requireVM().regs.readStr(encodeRegName(name));
    },

    /**
     * Write an integer value to a general-purpose register.
     *
     * @param {string|number} name
     * @param {bigint|number} value
     */
    set(name, value) {
      _requireVM().regs.writeInt(encodeRegName(name), typeof value === 'bigint' ? value : BigInt(Math.trunc(value)));
    },

    /**
     * Write a floating-point value to a SIMD/FP register.
     *
     * @param {string|number} name  e.g. "d1", "s0"
     * @param {number}        value
     */
    setFloat(name, value) {
      const enc = typeof name === 'number' ? `10:${name}` : encodeRegName(name);
      _requireVM().regs.writeFloat(enc, value);
    },

    /**
     * Write a string to a register's string slot (also updates the integer
     * slot to the string's length, mirroring what LDS does).
     *
     * @param {string|number} name
     * @param {string}        value
     */
    setStr(name, value) {
      const vm = _requireVM();
      const enc = encodeRegName(name);
      vm.regs.writeStr(enc, value);
      vm.regs.writeInt(enc, BigInt(value.length));
    },

    /**
     * Return a snapshot of all 31 general-purpose registers (x0–x30)
     * as an array of BigInts.
     *
     * @returns {bigint[]}
     */
    snapshot() {
      const regs = _requireVM().regs;
      return Array.from({ length: 31 }, (_, i) => regs.x[i] ?? 0n);
    },

    /**
     * Return a human-readable summary of all registers as a plain object.
     * Integer values are formatted as decimal strings (to preserve BigInt precision).
     * Registers with an associated string value include a `str` field.
     *
     * @returns {object}
     */
    dump() {
      const vm    = _requireVM();
      const regs  = vm.regs;
      const out   = { pc: vm.pc, sp: regs.sp.toString(), lr: regs.lr.toString(), gpr: {}, fpr: {} };
      for (let i = 0; i < 31; i++) {
        const enc = `0:${i}`;
        const entry = { value: (regs.x[i] ?? 0n).toString() };
        const s = regs.readStr(enc);
        if (s !== null) entry.str = s;
        out.gpr[`x${i}`] = entry;
      }
      for (let i = 0; i < 32; i++) {
        out.fpr[`d${i}`] = regs.d[i] ?? 0.0;
      }
      return out;
    },

    /**
     * Return the current program counter.
     * @returns {number}
     */
    pc() {
      return _requireVM().pc;
    },

    /**
     * Return the current stack pointer value.
     * @returns {bigint}
     */
    sp() {
      return _requireVM().regs.sp;
    },

    /**
     * Return the current link register value.
     * @returns {bigint}
     */
    lr() {
      return _requireVM().regs.lr;
    },

    /**
     * Return a copy of the VM-level pseudo stack (PUSH/POP items).
     * Each item is { val: bigint, str: string|null }.
     *
     * @returns {Array<{val: bigint, str: string|null}>}
     */
    stack() {
      return [..._requireVM().regs.stack];
    },

    /**
     * Return the call-return stack (raw PC indices).
     * @returns {number[]}
     */
    callStack() {
      return [..._requireVM().callStack];
    },

    /**
     * Resolve a user-friendly name to its internal encoding string.
     * Useful for low-level tooling that needs to call Registers methods directly.
     *
     * @param {string|number} name
     * @returns {string}
     */
    encode(name) {
      return encodeRegName(name);
    },
  };

  /* ---------------------------------------------------------
   * webassembler.mem
   * Memory introspection and direct read/write.
   * --------------------------------------------------------- */
  const mem = {
    /**
     * Read a 64-bit word from an 8-byte-aligned byte address.
     * Returns a BigInt (signed 64-bit).
     *
     * @param {bigint|number} addr  Must be 8-byte aligned.
     * @returns {bigint}
     */
    read(addr) {
      return _requireVM().regs.memRead(BigInt(addr));
    },

    /**
     * Read a single byte from any byte address.
     * @param {bigint|number} addr
     * @returns {bigint}  0n–255n
     */
    readByte(addr) {
      return _requireVM().regs.memReadByte(BigInt(addr));
    },

    /**
     * Read a 16-bit little-endian halfword.
     * @param {bigint|number} addr
     * @returns {bigint}
     */
    readHalf(addr) {
      return _requireVM().regs.memReadHalf(BigInt(addr));
    },

    /**
     * Read a 32-bit little-endian word from any byte address.
     * @param {bigint|number} addr
     * @returns {bigint}
     */
    readWord(addr) {
      const a = BigInt(addr);
      const regs = _requireVM().regs;
      let w = 0n;
      for (let i = 0n; i < 4n; i++) w |= regs.memReadByte(a + i) << (i * 8n);
      return w;
    },

    /**
     * Write a 64-bit value to an 8-byte-aligned address.
     * @param {bigint|number} addr
     * @param {bigint|number} value
     */
    write(addr, value) {
      _requireVM().regs.memWrite(BigInt(addr), BigInt(value));
    },

    /**
     * Write a single byte to any byte address.
     * @param {bigint|number} addr
     * @param {bigint|number} value  Only the low 8 bits are stored.
     */
    writeByte(addr, value) {
      _requireVM().regs.memWriteByte(BigInt(addr), BigInt(value));
    },

    /**
     * Write a 16-bit little-endian halfword.
     * @param {bigint|number} addr
     * @param {bigint|number} value
     */
    writeHalf(addr, value) {
      _requireVM().regs.memWriteHalf(BigInt(addr), BigInt(value));
    },

    /**
     * Read a null-terminated ASCII string from memory starting at `addr`.
     * Reads byte by byte until a 0x00 byte or `maxLen` bytes are consumed.
     *
     * @param {bigint|number} addr
     * @param {number}        [maxLen=1024]
     * @returns {string}
     */
    readCString(addr, maxLen = 1024) {
      const regs = _requireVM().regs;
      let a = BigInt(addr), out = '';
      for (let i = 0; i < maxLen; i++) {
        const b = Number(regs.memReadByte(a++));
        if (b === 0) break;
        out += String.fromCharCode(b);
      }
      return out;
    },

    /**
     * Write a JS string into memory as null-terminated ASCII, starting at `addr`.
     * @param {bigint|number} addr
     * @param {string}        str
     */
    writeCString(addr, str) {
      const regs = _requireVM().regs;
      let a = BigInt(addr);
      for (let i = 0; i < str.length; i++) {
        regs.memWriteByte(a++, BigInt(str.charCodeAt(i) & 0xff));
      }
      regs.memWriteByte(a, 0n);  // null terminator
    },

    /**
     * Read `count` consecutive 64-bit words starting at `addr` (8-byte steps).
     * @param {bigint|number} addr
     * @param {number}        count
     * @returns {bigint[]}
     */
    readWords(addr, count) {
      const regs = _requireVM().regs;
      const a = BigInt(addr);
      return Array.from({ length: count }, (_, i) => regs.memRead(a + BigInt(i) * 8n));
    },

    /**
     * Write an array of BigInt values as consecutive 64-bit words starting at `addr`.
     * @param {bigint|number} addr
     * @param {bigint[]}      values
     */
    writeWords(addr, values) {
      const regs = _requireVM().regs;
      let a = BigInt(addr);
      for (const v of values) { regs.memWrite(a, BigInt(v)); a += 8n; }
    },

    /**
     * Return a Map of all written memory locations.
     * Keys are byte-address strings, values are BigInts.
     * The returned Map is a shallow copy — mutations do not affect the VM.
     *
     * @returns {Map<string, bigint>}
     */
    dump() {
      return new Map(_requireVM().regs.mem);
    },

    /**
     * Return how many 64-bit words have been written to memory.
     * @returns {number}
     */
    size() {
      return _requireVM().regs.mem.size;
    },

    /**
     * Clear all memory (useful for test harnesses between runs on the same VM).
     */
    clear() {
      _requireVM().regs.mem.clear();
    },
  };

  /* ---------------------------------------------------------
   * webassembler.flags
   * NZCV flag read/write.
   * --------------------------------------------------------- */
  const flags = {
    /**
     * Return a snapshot of all four condition flags.
     * @returns {{ N: boolean, Z: boolean, C: boolean, V: boolean }}
     */
    get() {
      const r = _requireVM().regs;
      return { N: r.N, Z: r.Z, C: r.C, V: r.V };
    },

    /**
     * Overwrite one or more condition flags.
     * Only the keys you supply are changed.
     * @param {{ N?: boolean, Z?: boolean, C?: boolean, V?: boolean }} patch
     */
    set(patch) {
      const r = _requireVM().regs;
      if ('N' in patch) r.N = !!patch.N;
      if ('Z' in patch) r.Z = !!patch.Z;
      if ('C' in patch) r.C = !!patch.C;
      if ('V' in patch) r.V = !!patch.V;
    },

    /** @returns {boolean} Negative flag */
    N() { return _requireVM().regs.N; },
    /** @returns {boolean} Zero flag */
    Z() { return _requireVM().regs.Z; },
    /** @returns {boolean} Carry flag */
    C() { return _requireVM().regs.C; },
    /** @returns {boolean} Overflow flag */
    V() { return _requireVM().regs.V; },

    /**
     * Evaluate an AArch64 condition code against the current flags.
     * @param {string} cond  e.g. "eq", "ne", "lt", "ge", "hi", "lo" …
     * @returns {boolean}
     */
    eval(cond) {
      return evalCond(cond, _requireVM().regs);
    },

    /**
     * Return the 4-bit NZCV value packed as a number (bit 3=N, 2=Z, 1=C, 0=V).
     * @returns {number}
     */
    nzcv() {
      const r = _requireVM().regs;
      return (r.N ? 8 : 0) | (r.Z ? 4 : 0) | (r.C ? 2 : 0) | (r.V ? 1 : 0);
    },
  };

  /* ---------------------------------------------------------
   * webassembler.dbg
   * Debug and execution-tracing utilities.
   * --------------------------------------------------------- */
  const dbg = {
    /**
     * Return the instruction token array at a given PC index.
     * Returns null if the index is out of range.
     *
     * @param {number} [pc]  Defaults to current PC.
     * @returns {string[]|null}
     */
    instrAt(pc) {
      const vm = _requireVM();
      const idx = pc !== undefined ? pc : vm.pc;
      return vm.code[idx] ?? null;
    },

    /**
     * Return the total number of loaded instructions.
     * @returns {number}
     */
    codeLen() {
      return _requireVM().code.length;
    },

    /**
     * Return a slice of instructions around the current PC for context.
     * @param {number} [radius=3]  Lines before and after the current PC.
     * @returns {Array<{pc: number, tokens: string[], current: boolean}>}
     */
    context(radius = 3) {
      const vm  = _requireVM();
      const cur = vm.pc;
      const lo  = Math.max(0, cur - radius);
      const hi  = Math.min(vm.code.length - 1, cur + radius);
      return Array.from({ length: hi - lo + 1 }, (_, i) => ({
        pc:      lo + i,
        tokens:  vm.code[lo + i] ?? [],
        current: lo + i === cur,
      }));
    },

    /**
     * Return whether the VM is still running (i.e. has not halted).
     * @returns {boolean}
     */
    isRunning() {
      return _requireVM().running;
    },

    /**
     * Force-halt the running VM.  Useful from a timeout or external interrupt.
     */
    halt() {
      _requireVM().running = false;
    },

    /**
     * Execute exactly one instruction step from outside the VM loop.
     * Returns false if the VM is already stopped or out of bounds.
     *
     * @returns {Promise<boolean>}
     */
    async step() {
      const vm = _requireVM();
      if (!vm.running || vm.pc < 0 || vm.pc >= vm.code.length) return false;
      const instr = vm.code[vm.pc];
      if (instr && instr.length > 0) await vm.step(instr);
      return true;
    },

    /**
     * Produce a concise human-readable string describing the current VM state.
     * Useful for logging breakpoints.
     *
     * @returns {string}
     */
    stateStr() {
      const vm = _requireVM();
      const r  = vm.regs;
      const nzcv = `N=${+r.N} Z=${+r.Z} C=${+r.C} V=${+r.V}`;
      const gpr  = Array.from({ length: 8 }, (_, i) => `x${i}=${r.x[i] ?? 0n}`).join(' ');
      return `pc=${vm.pc} sp=${r.sp} lr=${r.lr} [${nzcv}] ${gpr}`;
    },

    /**
     * Attach a one-shot or persistent breakpoint callback that fires
     * before every instruction step.  The callback receives the VM instance
     * and the current token array.  Return `false` from the callback to
     * detach it automatically.
     *
     * Because the VM loop runs asynchronously this works by wrapping the
     * VM's internal step() method — call detach() on the returned handle
     * to remove the hook cleanly.
     *
     * @param {function(vm: VM, tokens: string[]): boolean|void} cb
     * @returns {{ detach: function }}
     */
    onStep(cb) {
      const vm       = _requireVM();
      const original = vm.step.bind(vm);
      vm.step = async function (tok) {
        const keep = cb(vm, tok);
        if (keep === false) vm.step = original;
        return original(tok);
      };
      return { detach() { vm.step = original; } };
    },
  };

  /* =========================================================
   * Public API object
   * ========================================================= */
  const api = {
    _code: null,

    // ── Introspection namespaces ──────────────────────────────
    /** Register introspection & manipulation. */
    reg,
    /** Memory introspection & manipulation. */
    mem,
    /** NZCV flag read/write & evaluation. */
    flags,
    /** Debug / step / tracing utilities. */
    dbg,

    // ── Callbacks ─────────────────────────────────────────────
    onOutput(text) { console.log(text); },
    onError(text)  { console.error(text); },
    onInput()      {
      return Promise.resolve(
        typeof prompt === 'function' ? (prompt('Input:') ?? '') : ''
      );
    },

    // ── Loaders ───────────────────────────────────────────────
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

    initFromString(src, callback) {
      this._code = parseWassm(src);
      if (typeof callback === 'function') Promise.resolve().then(callback);
      return Promise.resolve();
    },

    initFromFile(file, callback) {
      return file.text().then(src => {
        this._code = parseWassm(src);
        if (typeof callback === 'function') callback();
      });
    },

    // ── Execution ─────────────────────────────────────────────
    /**
     * Execute the loaded bytecode.
     * A fresh VM instance is created for each call, stored in _liveVM
     * so that the reg/mem/flags/dbg namespaces can reach it.
     *
     * @param {number} [yieldEvery=50000]
     * @returns {Promise<void>}
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
      _liveVM = vm;
      return vm.run(yieldEvery);
    },

    /** Returns a copy of the opcode table for introspection or tooling. */
    get opcodes() { return { ...OP }; },
  };

  return api;
}));

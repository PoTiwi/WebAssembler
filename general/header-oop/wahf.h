// WAHF.h

#ifndef WAHF_H
#define WAHF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/* ============================================================================
 * Configuration
 * ========================================================================== */
#define WAHF_MAX_LINE_LEN   1024
#define WAHF_INDENT         "\n"

/* ============================================================================
 * Register constants
 * ========================================================================== */
typedef enum {
    /* 64-bit general purpose */
    X0=0,X1,X2,X3,X4,X5,X6,X7,
    X8,X9,X10,X11,X12,X13,X14,X15,
    X16,X17,X18,X19,X20,X21,X22,X23,
    X24,X25,X26,X27,X28,X29,X30,

    /* 32-bit general purpose */
    W0=100,W1,W2,W3,W4,W5,W6,W7,
    W8,W9,W10,W11,W12,W13,W14,W15,
    W16,W17,W18,W19,W20,W21,W22,W23,
    W24,W25,W26,W27,W28,W29,W30,

    /* Special */
    WAHF_SP=200, WAHF_LR=201, WAHF_PC=202,
    WAHF_XZR=203, WAHF_WZR=204,
    WAHF_FP=205,   /* x29 alias */
    WAHF_IP0=206,  /* x16 alias */
    WAHF_IP1=207,  /* x17 alias */

    /* Float 64-bit */
    D0=300,D1,D2,D3,D4,D5,D6,D7,
    D8,D9,D10,D11,D12,D13,D14,D15,
    D16,D17,D18,D19,D20,D21,D22,D23,
    D24,D25,D26,D27,D28,D29,D30,D31,

    /* Float 32-bit */
    S0=400,S1,S2,S3,S4,S5,S6,S7,
    S8,S9,S10,S11,S12,S13,S14,S15,
    S16,S17,S18,S19,S20,S21,S22,S23,
    S24,S25,S26,S27,S28,S29,S30,S31,

    /* 128-bit SIMD */
    Q0=500,Q1,Q2,Q3,Q4,Q5,Q6,Q7,
    Q8,Q9,Q10,Q11,Q12,Q13,Q14,Q15,
    Q16,Q17,Q18,Q19,Q20,Q21,Q22,Q23,
    Q24,Q25,Q26,Q27,Q28,Q29,Q30,Q31,

    /* Vector */
    V0=600,V1,V2,V3,V4,V5,V6,V7,
    V8,V9,V10,V11,V12,V13,V14,V15,
    V16,V17,V18,V19,V20,V21,V22,V23,
    V24,V25,V26,V27,V28,V29,V30,V31,
} wahf_reg_t;

/* ============================================================================
 * Condition codes
 * ========================================================================== */
typedef enum {
    WAHF_EQ=0, WAHF_NE,
    WAHF_CS,   WAHF_HS=WAHF_CS,
    WAHF_CC,   WAHF_LO=WAHF_CC,
    WAHF_MI,   WAHF_PL,
    WAHF_VS,   WAHF_VC,
    WAHF_HI,   WAHF_LS,
    WAHF_GE,   WAHF_LT,
    WAHF_GT,   WAHF_LE,
    WAHF_AL,
} wahf_cond_t;

/* ============================================================================
 * Internal line buffer / program object
 * ========================================================================== */
typedef struct { char text[WAHF_MAX_LINE_LEN]; } wahf_line_t;

typedef struct {
    wahf_line_t *lines;
    int          count;
    int          capacity;
    char         source_name[256];
} wahf_program_t;

/* ============================================================================
 * Constructor / Destructor
 * ========================================================================== */
static inline wahf_program_t *wahf_new(void) {
    wahf_program_t *p = (wahf_program_t*)calloc(1, sizeof(wahf_program_t));
    if (!p) return NULL;
    p->capacity = 512;
    p->lines = (wahf_line_t*)malloc(sizeof(wahf_line_t) * (size_t)p->capacity);
    if (!p->lines) { free(p); return NULL; }
    p->count = 0;
    strncpy(p->source_name, "program", 255);
    return p;
}

static inline void wahf_free(wahf_program_t *p) {
    if (!p) return;
    free(p->lines);
    free(p);
}

static inline void wahf_set_name(wahf_program_t *p, const char *name) {
    if (p) strncpy(p->source_name, name, 255);
}

/* ============================================================================
 * Internal helpers
 * ========================================================================== */
static inline void wahf__grow(wahf_program_t *p) {
    if (p->count >= p->capacity) {
        p->capacity *= 2;
        p->lines = (wahf_line_t*)realloc(p->lines,
                        sizeof(wahf_line_t) * (size_t)p->capacity);
        assert(p->lines && "WAHF: out of memory");
    }
}

static inline void wahf__line(wahf_program_t *p, const char *fmt, ...) {
    wahf__grow(p);
    va_list ap; va_start(ap, fmt);
    vsnprintf(p->lines[p->count].text, WAHF_MAX_LINE_LEN, fmt, ap);
    va_end(ap);
    p->count++;
}

/* Register name → string */
static inline const char *wahf__regname(wahf_reg_t r) {
    static char bufs[16][32]; static int slot=0;
    char *buf = bufs[slot++ % 16];
    if (r >= V0)        { snprintf(buf,32,"v%d",(int)(r-V0));   return buf; }
    if (r >= Q0)        { snprintf(buf,32,"q%d",(int)(r-Q0));   return buf; }
    if (r >= S0)        { snprintf(buf,32,"s%d",(int)(r-S0));   return buf; }
    if (r >= D0)        { snprintf(buf,32,"d%d",(int)(r-D0));   return buf; }
    if (r==WAHF_IP1)    { snprintf(buf,32,"ip1");                return buf; }
    if (r==WAHF_IP0)    { snprintf(buf,32,"ip0");                return buf; }
    if (r==WAHF_FP)     { snprintf(buf,32,"fp");                 return buf; }
    if (r==WAHF_WZR)    { snprintf(buf,32,"wzr");                return buf; }
    if (r==WAHF_XZR)    { snprintf(buf,32,"xzr");                return buf; }
    if (r==WAHF_PC)     { snprintf(buf,32,"pc");                 return buf; }
    if (r==WAHF_LR)     { snprintf(buf,32,"lr");                 return buf; }
    if (r==WAHF_SP)     { snprintf(buf,32,"sp");                 return buf; }
    if (r >= W0)        { snprintf(buf,32,"w%d",(int)(r-W0));   return buf; }
    snprintf(buf,32,"x%d",(int)r);
    return buf;
}

/* Condition code → string */
static inline const char *wahf__condname(wahf_cond_t c) {
    switch(c) {
        case WAHF_EQ: return "eq"; case WAHF_NE: return "ne";
        case WAHF_CS: return "cs"; case WAHF_CC: return "cc";
        case WAHF_MI: return "mi"; case WAHF_PL: return "pl";
        case WAHF_VS: return "vs"; case WAHF_VC: return "vc";
        case WAHF_HI: return "hi"; case WAHF_LS: return "ls";
        case WAHF_GE: return "ge"; case WAHF_LT: return "lt";
        case WAHF_GT: return "gt"; case WAHF_LE: return "le";
        default:      return "al";
    }
}

/* ============================================================================
 * Emit / Dump
 * ========================================================================== */
static inline int wahf_emit(wahf_program_t *p, const char *path) {
    if (!p || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) { perror("wahf_emit"); return -1; }
    fprintf(f, "; WebAssembler Raw Bytecode\n");
    fprintf(f, "; Generated by WAHF (WebAssembler Header File)\n");
    fprintf(f, "; Not recommended to modify\n");
    fprintf(f, "; Contents of %s\n", p->source_name);
    fprintf(f, "; Lines ≈ %d\n", p->count);
    fprintf(f, ";;;;;\n\n");
    for (int i = 0; i < p->count; i++)
        fprintf(f, "%s\n", p->lines[i].text);
    fclose(f);
    fprintf(stdout, "[WAHF] Emitted %d line(s) → %s\n", p->count, path);
    return 0;
}

static inline void wahf_dump(wahf_program_t *p) {
    if (!p) return;
    printf("; ---- WAHF dump: %s (%d lines) ----\n", p->source_name, p->count);
    for (int i = 0; i < p->count; i++)
        printf("%4d | %s\n", i, p->lines[i].text);
}

/* ============================================================================
 * Structural helpers
 * ========================================================================== */
static inline void wahf_blank(wahf_program_t *p)   { wahf__line(p, ""); }

static inline void wahf_comment(wahf_program_t *p, const char *fmt, ...) {
    char buf[WAHF_MAX_LINE_LEN-4];
    va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    wahf__line(p, "; %s", buf);
}

static inline void wahf_label(wahf_program_t *p, const char *name) {
    wahf__line(p, "%s:", name);
}

static inline void wahf_label_c(wahf_program_t *p, const char *name, const char *c) {
    wahf__line(p, "%s:    ; %s", name, c);
}

/* ============================================================================
 * Data directives
 * ========================================================================== */
static inline void wahf_byte(wahf_program_t *p, long long v)   { wahf__line(p, WAHF_INDENT ".byte %lld",  v); }
static inline void wahf_word(wahf_program_t *p, long long v)   { wahf__line(p, WAHF_INDENT ".word %lld",  v); }
static inline void wahf_dword(wahf_program_t *p, long long v)  { wahf__line(p, WAHF_INDENT ".dword %lld", v); }
static inline void wahf_space(wahf_program_t *p, long long n)  { wahf__line(p, WAHF_INDENT ".space %lld", n); }
static inline void wahf_ascii(wahf_program_t *p, const char *s){ wahf__line(p, WAHF_INDENT ".ascii \"%s\"",s); }
static inline void wahf_asciz(wahf_program_t *p, const char *s){ wahf__line(p, WAHF_INDENT ".asciz \"%s\"",s); }
static inline void wahf_align(wahf_program_t *p, long long n)  { wahf__line(p, WAHF_INDENT ".align %lld", n); }

/* ============================================================================
 *  SECTION 1 — CUSTOM / PSEUDO INSTRUCTIONS
 * ========================================================================== */

/** lds REG, "string" — load string literal into register */
static inline void wahf_lds(wahf_program_t *p, wahf_reg_t dst, const char *str) {
    wahf__line(p, WAHF_INDENT "lds %s, \"%s\"", wahf__regname(dst), str);
}

/** adl DEST, SRC — DEST = strlen(SRC) */
static inline void wahf_adl(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "adl %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** strlen DEST, SRC — alias for adl */
static inline void wahf_strlen(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "strlen %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** puts REG — print string in register */
static inline void wahf_puts(wahf_program_t *p, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "puts %s", wahf__regname(src));
}

/** dmp REG — debug dump register value */
static inline void wahf_dmp(wahf_program_t *p, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "dmp %s", wahf__regname(src));
}

/** halt — stop execution */
static inline void wahf_halt(wahf_program_t *p) { wahf__line(p, WAHF_INDENT "halt"); }

/** nop — no operation */
static inline void wahf_nop(wahf_program_t *p)  { wahf__line(p, WAHF_INDENT "nop");  }

/** geti REG — read integer from stdin */
static inline void wahf_geti(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "geti %s", wahf__regname(dst));
}

/** gets REG — read string from stdin */
static inline void wahf_gets(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "gets %s", wahf__regname(dst));
}

/** rand REG — fill register with random integer */
static inline void wahf_rand(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "rand %s", wahf__regname(dst));
}

/** time REG — fill register with unix timestamp (ms) */
static inline void wahf_time(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "time %s", wahf__regname(dst));
}

/** clrr REG — zero a register */
static inline void wahf_clrr(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "clrr %s", wahf__regname(dst));
}

/** inc REG — increment by 1 */
static inline void wahf_inc(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "inc %s", wahf__regname(dst));
}

/** dec REG — decrement by 1 */
static inline void wahf_dec(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "dec %s", wahf__regname(dst));
}

/** abs REG — absolute value in place */
static inline void wahf_abs(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "abs %s", wahf__regname(dst));
}

/** not REG — bitwise NOT in place */
static inline void wahf_not(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "not %s", wahf__regname(dst));
}

/** swp REG, REG — swap two registers */
static inline void wahf_swp(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "swp %s, %s", wahf__regname(a), wahf__regname(b));
}

/** push REG — push onto VM stack */
static inline void wahf_push(wahf_program_t *p, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "push %s", wahf__regname(src));
}

/** pop REG — pop from VM stack */
static inline void wahf_pop(wahf_program_t *p, wahf_reg_t dst) {
    wahf__line(p, WAHF_INDENT "pop %s", wahf__regname(dst));
}

/** shl REG, #IMM — shift left immediate */
static inline void wahf_shl(wahf_program_t *p, wahf_reg_t dst, long long imm) {
    wahf__line(p, WAHF_INDENT "shl %s, #%lld", wahf__regname(dst), imm);
}

/** shr REG, #IMM — shift right immediate */
static inline void wahf_shr(wahf_program_t *p, wahf_reg_t dst, long long imm) {
    wahf__line(p, WAHF_INDENT "shr %s, #%lld", wahf__regname(dst), imm);
}

/** max2 DST, SRC1, SRC2 */
static inline void wahf_max2(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "max2 %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** min2 DST, SRC1, SRC2 */
static inline void wahf_min2(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "min2 %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** mod DST, SRC1, SRC2 — DST = SRC1 % SRC2 */
static inline void wahf_mod(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "mod %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** itoa DST, SRC — integer to ASCII string */
static inline void wahf_itoa(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "itoa %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** atoi DST, SRC — ASCII string to integer */
static inline void wahf_atoi(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "atoi %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** strcpy DST, SRC */
static inline void wahf_strcpy(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "strcpy %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** strcat DST, SRC */
static inline void wahf_strcat(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "strcat %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** strcmp DST, SRC1, SRC2 — result into DST (0 = equal) */
static inline void wahf_strcmp(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "strcmp %s, %s, %s", wahf__regname(dst), wahf__regname(a), wahf__regname(b));
}

/** memcpy DST, SRC, LEN */
static inline void wahf_memcpy(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src, wahf_reg_t len) {
    wahf__line(p, WAHF_INDENT "memcpy %s, %s, %s", wahf__regname(dst), wahf__regname(src), wahf__regname(len));
}

/** memset DST, VAL, LEN */
static inline void wahf_memset(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t val, wahf_reg_t len) {
    wahf__line(p, WAHF_INDENT "memset %s, %s, %s", wahf__regname(dst), wahf__regname(val), wahf__regname(len));
}

/* ============================================================================
 *  SECTION 2 — DATA MOVEMENT
 * ========================================================================== */

/** mov DST, #IMM */
static inline void wahf_mov_imm(wahf_program_t *p, wahf_reg_t dst, long long imm) {
    wahf__line(p, WAHF_INDENT "mov %s, #%lld", wahf__regname(dst), imm);
}

/** mov DST, SRC */
static inline void wahf_mov_reg(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "mov %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** movz DST, #IMM [, LSL #SHIFT] */
static inline void wahf_movz(wahf_program_t *p, wahf_reg_t dst, long long imm, int shift) {
    if (shift) wahf__line(p, WAHF_INDENT "movz %s, #%lld, lsl #%d", wahf__regname(dst), imm, shift);
    else       wahf__line(p, WAHF_INDENT "movz %s, #%lld",           wahf__regname(dst), imm);
}

/** movn DST, #IMM [, LSL #SHIFT] */
static inline void wahf_movn(wahf_program_t *p, wahf_reg_t dst, long long imm, int shift) {
    if (shift) wahf__line(p, WAHF_INDENT "movn %s, #%lld, lsl #%d", wahf__regname(dst), imm, shift);
    else       wahf__line(p, WAHF_INDENT "movn %s, #%lld",           wahf__regname(dst), imm);
}

/** movk DST, #IMM [, LSL #SHIFT] */
static inline void wahf_movk(wahf_program_t *p, wahf_reg_t dst, long long imm, int shift) {
    if (shift) wahf__line(p, WAHF_INDENT "movk %s, #%lld, lsl #%d", wahf__regname(dst), imm, shift);
    else       wahf__line(p, WAHF_INDENT "movk %s, #%lld",           wahf__regname(dst), imm);
}

/** mvn DST, SRC — bitwise NOT of SRC into DST */
static inline void wahf_mvn(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "mvn %s, %s", wahf__regname(dst), wahf__regname(src));
}

/* ============================================================================
 *  SECTION 3 — LOAD / STORE
 * ========================================================================== */

/** ldr DST, [BASE, #OFFSET] */
static inline void wahf_ldr(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    if (off) wahf__line(p, WAHF_INDENT "ldr %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
    else     wahf__line(p, WAHF_INDENT "ldr %s, [%s]",         wahf__regname(dst), wahf__regname(base));
}

/** ldr DST, SRC — load from address in register */
static inline void wahf_ldr_reg(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "ldr %s, %s", wahf__regname(dst), wahf__regname(src));
}

/** str SRC, [BASE, #OFFSET] */
static inline void wahf_str(wahf_program_t *p, wahf_reg_t src, wahf_reg_t base, long long off) {
    if (off) wahf__line(p, WAHF_INDENT "str %s, [%s, #%lld]", wahf__regname(src), wahf__regname(base), off);
    else     wahf__line(p, WAHF_INDENT "str %s, [%s]",         wahf__regname(src), wahf__regname(base));
}

/** ldrb DST, [BASE, #OFFSET] — load byte, zero-extend */
static inline void wahf_ldrb(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldrb %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/** ldrh DST, [BASE, #OFFSET] — load halfword, zero-extend */
static inline void wahf_ldrh(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldrh %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/** ldrsb DST, [BASE, #OFFSET] — load byte, sign-extend */
static inline void wahf_ldrsb(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldrsb %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/** ldrsh DST, [BASE, #OFFSET] — load halfword, sign-extend */
static inline void wahf_ldrsh(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldrsh %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/** ldrsw DST, [BASE, #OFFSET] — load word, sign-extend */
static inline void wahf_ldrsw(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldrsw %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/** strb SRC, [BASE, #OFFSET] — store byte */
static inline void wahf_strb(wahf_program_t *p, wahf_reg_t src, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "strb %s, [%s, #%lld]", wahf__regname(src), wahf__regname(base), off);
}

/** strh SRC, [BASE, #OFFSET] — store halfword */
static inline void wahf_strh(wahf_program_t *p, wahf_reg_t src, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "strh %s, [%s, #%lld]", wahf__regname(src), wahf__regname(base), off);
}

/** ldp R1, R2, [BASE, #OFFSET] — load pair */
static inline void wahf_ldp(wahf_program_t *p, wahf_reg_t r1, wahf_reg_t r2, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldp %s, %s, [%s, #%lld]",
               wahf__regname(r1), wahf__regname(r2), wahf__regname(base), off);
}

/** stp R1, R2, [BASE, #OFFSET] — store pair */
static inline void wahf_stp(wahf_program_t *p, wahf_reg_t r1, wahf_reg_t r2, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "stp %s, %s, [%s, #%lld]",
               wahf__regname(r1), wahf__regname(r2), wahf__regname(base), off);
}

/** adr DST, LABEL — PC-relative address */
static inline void wahf_adr(wahf_program_t *p, wahf_reg_t dst, const char *label) {
    wahf__line(p, WAHF_INDENT "adr %s, %s", wahf__regname(dst), label);
}

/** adrp DST, LABEL — page-relative address */
static inline void wahf_adrp(wahf_program_t *p, wahf_reg_t dst, const char *label) {
    wahf__line(p, WAHF_INDENT "adrp %s, %s", wahf__regname(dst), label);
}

/* ============================================================================
 *  SECTION 4 — ARITHMETIC
 * ========================================================================== */

/** add DST, SRC1, SRC2 */
static inline void wahf_add(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "add %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** add DST, SRC, #IMM */
static inline void wahf_add_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "add %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** adds DST, SRC1, SRC2 — add and set flags */
static inline void wahf_adds(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "adds %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** adds DST, SRC, #IMM */
static inline void wahf_adds_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "adds %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** sub DST, SRC1, SRC2 */
static inline void wahf_sub(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "sub %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** sub DST, SRC, #IMM */
static inline void wahf_sub_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "sub %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** subs DST, SRC1, SRC2 — subtract and set flags */
static inline void wahf_subs(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "subs %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** subs DST, SRC, #IMM */
static inline void wahf_subs_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "subs %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** mul DST, SRC1, SRC2 */
static inline void wahf_mul(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "mul %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** udiv DST, SRC1, SRC2 — unsigned divide */
static inline void wahf_udiv(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "udiv %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** sdiv DST, SRC1, SRC2 — signed divide */
static inline void wahf_sdiv(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "sdiv %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** neg DST, SRC */
static inline void wahf_neg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "neg %s, %s", wahf__regname(d), wahf__regname(a));
}

/** negs DST, SRC — negate and set flags */
static inline void wahf_negs(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "negs %s, %s", wahf__regname(d), wahf__regname(a));
}

/** adc DST, SRC1, SRC2 — add with carry */
static inline void wahf_adc(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "adc %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** adcs DST, SRC1, SRC2 — add with carry and set flags */
static inline void wahf_adcs(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "adcs %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** sbc DST, SRC1, SRC2 — subtract with carry */
static inline void wahf_sbc(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "sbc %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** sbcs DST, SRC1, SRC2 — subtract with carry and set flags */
static inline void wahf_sbcs(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "sbcs %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** madd DST, A, B, C — DST = A*B + C */
static inline void wahf_madd(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "madd %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** msub DST, A, B, C — DST = C - A*B */
static inline void wahf_msub(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "msub %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** mneg DST, SRC1, SRC2 — DST = -(SRC1*SRC2) */
static inline void wahf_mneg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "mneg %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** smull DST, SRC1, SRC2 — signed 32×32→64 multiply */
static inline void wahf_smull(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "smull %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** umull DST, SRC1, SRC2 — unsigned 32×32→64 multiply */
static inline void wahf_umull(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "umull %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** smulh DST, SRC1, SRC2 — signed high 64 bits of 64×64 multiply */
static inline void wahf_smulh(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "smulh %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** umulh DST, SRC1, SRC2 — unsigned high 64 bits of 64×64 multiply */
static inline void wahf_umulh(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "umulh %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** smaddl DST, A, B, C — DST = C + (signed)A*B (64-bit) */
static inline void wahf_smaddl(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "smaddl %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** umaddl DST, A, B, C — DST = C + (unsigned)A*B (64-bit) */
static inline void wahf_umaddl(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "umaddl %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** smsubl DST, A, B, C — DST = C - (signed)A*B (64-bit) */
static inline void wahf_smsubl(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "smsubl %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** umsubl DST, A, B, C — DST = C - (unsigned)A*B (64-bit) */
static inline void wahf_umsubl(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "umsubl %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/* ============================================================================
 *  SECTION 5 — LOGICAL / BITWISE
 * ========================================================================== */

/** and DST, SRC1, SRC2 */
static inline void wahf_and(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "and %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** and DST, SRC, #IMM */
static inline void wahf_and_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "and %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** ands DST, SRC1, SRC2 — AND and set flags */
static inline void wahf_ands(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "ands %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** ands DST, SRC, #IMM */
static inline void wahf_ands_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "ands %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** orr DST, SRC1, SRC2 */
static inline void wahf_orr(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "orr %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** orr DST, SRC, #IMM */
static inline void wahf_orr_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "orr %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** orn DST, SRC1, SRC2 — OR NOT */
static inline void wahf_orn(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "orn %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** eor DST, SRC1, SRC2 */
static inline void wahf_eor(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "eor %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** eor DST, SRC, #IMM */
static inline void wahf_eor_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "eor %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** eon DST, SRC1, SRC2 — EOR NOT */
static inline void wahf_eon(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "eon %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** bic DST, SRC1, SRC2 — bit clear */
static inline void wahf_bic(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "bic %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** bics DST, SRC1, SRC2 — bit clear and set flags */
static inline void wahf_bics(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "bics %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/* ============================================================================
 *  SECTION 6 — SHIFTS
 * ========================================================================== */

/** lsl DST, SRC, #IMM */
static inline void wahf_lsl_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "lsl %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** lsl DST, SRC1, SRC2 */
static inline void wahf_lsl_reg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "lsl %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** lsr DST, SRC, #IMM */
static inline void wahf_lsr_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "lsr %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** lsr DST, SRC1, SRC2 */
static inline void wahf_lsr_reg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "lsr %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** asr DST, SRC, #IMM — arithmetic shift right */
static inline void wahf_asr_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "asr %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** asr DST, SRC1, SRC2 */
static inline void wahf_asr_reg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "asr %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** ror DST, SRC, #IMM — rotate right */
static inline void wahf_ror_imm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "ror %s, %s, #%lld", wahf__regname(d), wahf__regname(a), imm);
}

/** ror DST, SRC1, SRC2 */
static inline void wahf_ror_reg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "ror %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/* ============================================================================
 *  SECTION 7 — COMPARISON
 * ========================================================================== */

/** cmp SRC1, SRC2 */
static inline void wahf_cmp(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "cmp %s, %s", wahf__regname(a), wahf__regname(b));
}

/** cmp SRC, #IMM */
static inline void wahf_cmp_imm(wahf_program_t *p, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "cmp %s, #%lld", wahf__regname(a), imm);
}

/** cmn SRC1, SRC2 — compare negative */
static inline void wahf_cmn(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "cmn %s, %s", wahf__regname(a), wahf__regname(b));
}

/** cmn SRC, #IMM */
static inline void wahf_cmn_imm(wahf_program_t *p, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "cmn %s, #%lld", wahf__regname(a), imm);
}

/** tst SRC1, SRC2 — test bits */
static inline void wahf_tst(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "tst %s, %s", wahf__regname(a), wahf__regname(b));
}

/** tst SRC, #IMM */
static inline void wahf_tst_imm(wahf_program_t *p, wahf_reg_t a, long long imm) {
    wahf__line(p, WAHF_INDENT "tst %s, #%lld", wahf__regname(a), imm);
}

/* ============================================================================
 *  SECTION 8 — BRANCHES
 * ========================================================================== */

/** b LABEL — unconditional branch */
static inline void wahf_b(wahf_program_t *p, const char *label)  { wahf__line(p, WAHF_INDENT "b %s",   label); }

/** bl LABEL — branch with link (call) */
static inline void wahf_bl(wahf_program_t *p, const char *label) { wahf__line(p, WAHF_INDENT "bl %s",  label); }

/** br REG — branch to address in register */
static inline void wahf_br(wahf_program_t *p, wahf_reg_t r) { wahf__line(p, WAHF_INDENT "br %s",  wahf__regname(r)); }

/** blr REG — branch with link to address in register */
static inline void wahf_blr(wahf_program_t *p, wahf_reg_t r){ wahf__line(p, WAHF_INDENT "blr %s", wahf__regname(r)); }

/** ret — return via LR */
static inline void wahf_ret(wahf_program_t *p) { wahf__line(p, WAHF_INDENT "ret"); }

/** ret REG — return via register */
static inline void wahf_ret_reg(wahf_program_t *p, wahf_reg_t r) { wahf__line(p, WAHF_INDENT "ret %s", wahf__regname(r)); }

/** call LABEL — high-level alias for bl */
static inline void wahf_call(wahf_program_t *p, const char *label) { wahf__line(p, WAHF_INDENT "call %s", label); }

/** jmp LABEL — high-level alias for b */
static inline void wahf_jmp(wahf_program_t *p, const char *label)  { wahf__line(p, WAHF_INDENT "jmp %s",  label); }

/** b.cond LABEL — conditional branch */
static inline void wahf_bcond(wahf_program_t *p, wahf_cond_t cond, const char *label) {
    wahf__line(p, WAHF_INDENT "b.%s %s", wahf__condname(cond), label);
}

/* Per-condition branch aliases (wahf_beq, wahf_bne, …) */
#define WAHF__BCOND(suffix, cv) \
    static inline void wahf_b##suffix(wahf_program_t *p, const char *l) { wahf_bcond(p, cv, l); }
WAHF__BCOND(eq, WAHF_EQ) WAHF__BCOND(ne, WAHF_NE)
WAHF__BCOND(lt, WAHF_LT) WAHF__BCOND(le, WAHF_LE)
WAHF__BCOND(gt, WAHF_GT) WAHF__BCOND(ge, WAHF_GE)
WAHF__BCOND(lo, WAHF_LO) WAHF__BCOND(ls, WAHF_LS)
WAHF__BCOND(hi, WAHF_HI) WAHF__BCOND(hs, WAHF_HS)
WAHF__BCOND(mi, WAHF_MI) WAHF__BCOND(pl, WAHF_PL)
WAHF__BCOND(vs, WAHF_VS) WAHF__BCOND(vc, WAHF_VC)
WAHF__BCOND(al, WAHF_AL)
#undef WAHF__BCOND

/* High-level jump aliases — j-prefix versions */
static inline void wahf_jeq(wahf_program_t *p, const char *l) { wahf__line(p, WAHF_INDENT "jeq %s", l); }
static inline void wahf_jne(wahf_program_t *p, const char *l) { wahf__line(p, WAHF_INDENT "jne %s", l); }
static inline void wahf_jlt(wahf_program_t *p, const char *l) { wahf__line(p, WAHF_INDENT "jlt %s", l); }
static inline void wahf_jle(wahf_program_t *p, const char *l) { wahf__line(p, WAHF_INDENT "jle %s", l); }
static inline void wahf_jgt(wahf_program_t *p, const char *l) { wahf__line(p, WAHF_INDENT "jgt %s", l); }
static inline void wahf_jge(wahf_program_t *p, const char *l) { wahf__line(p, WAHF_INDENT "jge %s", l); }

/** cbz REG, LABEL — compare and branch if zero */
static inline void wahf_cbz(wahf_program_t *p, wahf_reg_t r, const char *label) {
    wahf__line(p, WAHF_INDENT "cbz %s, %s", wahf__regname(r), label);
}

/** cbnz REG, LABEL — compare and branch if not zero */
static inline void wahf_cbnz(wahf_program_t *p, wahf_reg_t r, const char *label) {
    wahf__line(p, WAHF_INDENT "cbnz %s, %s", wahf__regname(r), label);
}

/** tbz REG, #BIT, LABEL — test bit and branch if zero */
static inline void wahf_tbz(wahf_program_t *p, wahf_reg_t r, int bit, const char *label) {
    wahf__line(p, WAHF_INDENT "tbz %s, #%d, %s", wahf__regname(r), bit, label);
}

/** tbnz REG, #BIT, LABEL — test bit and branch if not zero */
static inline void wahf_tbnz(wahf_program_t *p, wahf_reg_t r, int bit, const char *label) {
    wahf__line(p, WAHF_INDENT "tbnz %s, #%d, %s", wahf__regname(r), bit, label);
}

/* ============================================================================
 *  SECTION 9 — CONDITIONAL SELECT / SET
 * ========================================================================== */

/** csel DST, A, B, COND — DST = (cond) ? A : B */
static inline void wahf_csel(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "csel %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__condname(cond));
}

/** csinc DST, A, B, COND — DST = (cond) ? A : B+1 */
static inline void wahf_csinc(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "csinc %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__condname(cond));
}

/** csinv DST, A, B, COND — DST = (cond) ? A : ~B */
static inline void wahf_csinv(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "csinv %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__condname(cond));
}

/** csneg DST, A, B, COND — DST = (cond) ? A : -B */
static inline void wahf_csneg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "csneg %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__condname(cond));
}

/** cset DST, COND — DST = (cond) ? 1 : 0 */
static inline void wahf_cset(wahf_program_t *p, wahf_reg_t d, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "cset %s, %s", wahf__regname(d), wahf__condname(cond));
}

/** csetm DST, COND — DST = (cond) ? -1 : 0 */
static inline void wahf_csetm(wahf_program_t *p, wahf_reg_t d, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "csetm %s, %s", wahf__regname(d), wahf__condname(cond));
}

/** cinc DST, SRC, COND — DST = (cond) ? SRC+1 : SRC */
static inline void wahf_cinc(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "cinc %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__condname(cond));
}

/** cinv DST, SRC, COND — DST = (cond) ? ~SRC : SRC */
static inline void wahf_cinv(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "cinv %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__condname(cond));
}

/** cneg DST, SRC, COND — DST = (cond) ? -SRC : SRC */
static inline void wahf_cneg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "cneg %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__condname(cond));
}

/** ccmp SRC1, SRC2, #NZCV, COND — conditional compare */
static inline void wahf_ccmp(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b, int nzcv, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "ccmp %s, %s, #%d, %s",
               wahf__regname(a), wahf__regname(b), nzcv, wahf__condname(cond));
}

/** ccmp SRC, #IMM, #NZCV, COND */
static inline void wahf_ccmp_imm(wahf_program_t *p, wahf_reg_t a, long long imm, int nzcv, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "ccmp %s, #%lld, #%d, %s",
               wahf__regname(a), imm, nzcv, wahf__condname(cond));
}

/** ccmn SRC1, SRC2, #NZCV, COND — conditional compare negative */
static inline void wahf_ccmn(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b, int nzcv, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "ccmn %s, %s, #%d, %s",
               wahf__regname(a), wahf__regname(b), nzcv, wahf__condname(cond));
}

/* ============================================================================
 *  SECTION 10 — BIT MANIPULATION
 * ========================================================================== */

/** clz DST, SRC — count leading zeros */
static inline void wahf_clz(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "clz %s, %s", wahf__regname(d), wahf__regname(a));
}

/** cls DST, SRC — count leading sign bits */
static inline void wahf_cls(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "cls %s, %s", wahf__regname(d), wahf__regname(a));
}

/** rbit DST, SRC — reverse bits */
static inline void wahf_rbit(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "rbit %s, %s", wahf__regname(d), wahf__regname(a));
}

/** rev DST, SRC — reverse bytes */
static inline void wahf_rev(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "rev %s, %s", wahf__regname(d), wahf__regname(a));
}

/** rev16 DST, SRC — reverse bytes in 16-bit halfwords */
static inline void wahf_rev16(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "rev16 %s, %s", wahf__regname(d), wahf__regname(a));
}

/** rev32 DST, SRC — reverse bytes in 32-bit words */
static inline void wahf_rev32(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "rev32 %s, %s", wahf__regname(d), wahf__regname(a));
}

/** rev64 DST, SRC — reverse bytes in 64-bit doubleword */
static inline void wahf_rev64(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "rev64 %s, %s", wahf__regname(d), wahf__regname(a));
}

/** extr DST, SRC1, SRC2, #LSB — extract register */
static inline void wahf_extr(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, long long lsb) {
    wahf__line(p, WAHF_INDENT "extr %s, %s, %s, #%lld",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), lsb);
}

/** sbfm DST, SRC, #IMMR, #IMMS — signed bitfield move */
static inline void wahf_sbfm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long immr, long long imms) {
    wahf__line(p, WAHF_INDENT "sbfm %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), immr, imms);
}

/** bfm DST, SRC, #IMMR, #IMMS — bitfield move */
static inline void wahf_bfm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long immr, long long imms) {
    wahf__line(p, WAHF_INDENT "bfm %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), immr, imms);
}

/** ubfm DST, SRC, #IMMR, #IMMS — unsigned bitfield move */
static inline void wahf_ubfm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long immr, long long imms) {
    wahf__line(p, WAHF_INDENT "ubfm %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), immr, imms);
}

/** sbfx DST, SRC, #LSB, #WIDTH — signed bitfield extract */
static inline void wahf_sbfx(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long lsb, long long width) {
    wahf__line(p, WAHF_INDENT "sbfx %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), lsb, width);
}

/** ubfx DST, SRC, #LSB, #WIDTH — unsigned bitfield extract */
static inline void wahf_ubfx(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long lsb, long long width) {
    wahf__line(p, WAHF_INDENT "ubfx %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), lsb, width);
}

/** sbfiz DST, SRC, #LSB, #WIDTH — signed bitfield insert in zeros */
static inline void wahf_sbfiz(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long lsb, long long width) {
    wahf__line(p, WAHF_INDENT "sbfiz %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), lsb, width);
}

/** ubfiz DST, SRC, #LSB, #WIDTH — unsigned bitfield insert in zeros */
static inline void wahf_ubfiz(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long lsb, long long width) {
    wahf__line(p, WAHF_INDENT "ubfiz %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), lsb, width);
}

/** bfxil DST, SRC, #LSB, #WIDTH — bitfield extract and insert low */
static inline void wahf_bfxil(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long lsb, long long width) {
    wahf__line(p, WAHF_INDENT "bfxil %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), lsb, width);
}

/** bfi DST, SRC, #LSB, #WIDTH — bitfield insert */
static inline void wahf_bfi(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, long long lsb, long long width) {
    wahf__line(p, WAHF_INDENT "bfi %s, %s, #%lld, #%lld", wahf__regname(d), wahf__regname(a), lsb, width);
}

/* Sign / zero extension */
static inline void wahf_sxtb(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) { wahf__line(p, WAHF_INDENT "sxtb %s, %s", wahf__regname(d), wahf__regname(a)); }
static inline void wahf_sxth(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) { wahf__line(p, WAHF_INDENT "sxth %s, %s", wahf__regname(d), wahf__regname(a)); }
static inline void wahf_sxtw(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) { wahf__line(p, WAHF_INDENT "sxtw %s, %s", wahf__regname(d), wahf__regname(a)); }
static inline void wahf_uxtb(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) { wahf__line(p, WAHF_INDENT "uxtb %s, %s", wahf__regname(d), wahf__regname(a)); }
static inline void wahf_uxth(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) { wahf__line(p, WAHF_INDENT "uxth %s, %s", wahf__regname(d), wahf__regname(a)); }

/* ============================================================================
 *  SECTION 11 — FLOATING-POINT
 * ========================================================================== */

/** fmov DST, SRC */
static inline void wahf_fmov(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "fmov %s, %s", wahf__regname(d), wahf__regname(a));
}

/** fadd DST, SRC1, SRC2 */
static inline void wahf_fadd(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fadd %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fsub DST, SRC1, SRC2 */
static inline void wahf_fsub(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fsub %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fmul DST, SRC1, SRC2 */
static inline void wahf_fmul(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fmul %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fdiv DST, SRC1, SRC2 */
static inline void wahf_fdiv(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fdiv %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fabs DST, SRC */
static inline void wahf_fabs(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "fabs %s, %s", wahf__regname(d), wahf__regname(a));
}

/** fneg DST, SRC */
static inline void wahf_fneg(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "fneg %s, %s", wahf__regname(d), wahf__regname(a));
}

/** fsqrt DST, SRC */
static inline void wahf_fsqrt(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "fsqrt %s, %s", wahf__regname(d), wahf__regname(a));
}

/** fcmp SRC1, SRC2 */
static inline void wahf_fcmp(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fcmp %s, %s", wahf__regname(a), wahf__regname(b));
}

/** fcmp SRC, #0.0 */
static inline void wahf_fcmp_zero(wahf_program_t *p, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "fcmp %s, #0.0", wahf__regname(a));
}

/** fcmpe SRC1, SRC2 — compare and raise exception on NaN */
static inline void wahf_fcmpe(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fcmpe %s, %s", wahf__regname(a), wahf__regname(b));
}

/** fccmp SRC1, SRC2, #NZCV, COND — conditional floating-point compare */
static inline void wahf_fccmp(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b, int nzcv, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "fccmp %s, %s, #%d, %s",
               wahf__regname(a), wahf__regname(b), nzcv, wahf__condname(cond));
}

/** fccmpe SRC1, SRC2, #NZCV, COND */
static inline void wahf_fccmpe(wahf_program_t *p, wahf_reg_t a, wahf_reg_t b, int nzcv, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "fccmpe %s, %s, #%d, %s",
               wahf__regname(a), wahf__regname(b), nzcv, wahf__condname(cond));
}

/** fcsel DST, A, B, COND — floating-point conditional select */
static inline void wahf_fcsel(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_cond_t cond) {
    wahf__line(p, WAHF_INDENT "fcsel %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__condname(cond));
}

/** fcvt DST, SRC — convert between float precisions */
static inline void wahf_fcvt(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) {
    wahf__line(p, WAHF_INDENT "fcvt %s, %s", wahf__regname(d), wahf__regname(a));
}

/* All fcvt* conversion variants */
#define WAHF__FCVT1(name) \
    static inline void wahf_##name(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a) { \
        wahf__line(p, WAHF_INDENT #name " %s, %s", wahf__regname(d), wahf__regname(a)); \
    }
WAHF__FCVT1(fcvtas) WAHF__FCVT1(fcvtau)
WAHF__FCVT1(fcvtms) WAHF__FCVT1(fcvtmu)
WAHF__FCVT1(fcvtns) WAHF__FCVT1(fcvtnu)
WAHF__FCVT1(fcvtps) WAHF__FCVT1(fcvtpu)
WAHF__FCVT1(fcvtzs) WAHF__FCVT1(fcvtzu)
WAHF__FCVT1(scvtf)  WAHF__FCVT1(ucvtf)
#undef WAHF__FCVT1

/** fmadd DST, A, B, C — DST = A*B + C */
static inline void wahf_fmadd(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "fmadd %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** fmsub DST, A, B, C — DST = C - A*B */
static inline void wahf_fmsub(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "fmsub %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** fnmadd DST, A, B, C — DST = -(A*B + C) */
static inline void wahf_fnmadd(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "fnmadd %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** fnmsub DST, A, B, C — DST = -(C - A*B) */
static inline void wahf_fnmsub(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b, wahf_reg_t c) {
    wahf__line(p, WAHF_INDENT "fnmsub %s, %s, %s, %s",
               wahf__regname(d), wahf__regname(a), wahf__regname(b), wahf__regname(c));
}

/** fmin DST, SRC1, SRC2 */
static inline void wahf_fmin(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fmin %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fmax DST, SRC1, SRC2 */
static inline void wahf_fmax(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fmax %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fminnm DST, SRC1, SRC2 — min, NaN-propagating */
static inline void wahf_fminnm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fminnm %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/** fmaxnm DST, SRC1, SRC2 — max, NaN-propagating */
static inline void wahf_fmaxnm(wahf_program_t *p, wahf_reg_t d, wahf_reg_t a, wahf_reg_t b) {
    wahf__line(p, WAHF_INDENT "fmaxnm %s, %s, %s", wahf__regname(d), wahf__regname(a), wahf__regname(b));
}

/* ============================================================================
 *  SECTION 12 — ATOMIC / EXCLUSIVE ACCESS
 * ========================================================================== */

/* Load-acquire variants */
#define WAHF__LD1(name) \
    static inline void wahf_##name(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base) { \
        wahf__line(p, WAHF_INDENT #name " %s, [%s]", wahf__regname(dst), wahf__regname(base)); \
    }
WAHF__LD1(ldar)   WAHF__LD1(ldarb)  WAHF__LD1(ldarh)
WAHF__LD1(ldaxr)  WAHF__LD1(ldaxrb) WAHF__LD1(ldaxrh)
WAHF__LD1(ldxr)   WAHF__LD1(ldxrb)  WAHF__LD1(ldxrh)
#undef WAHF__LD1

/* Store-release variants (1 src, 1 base) */
#define WAHF__ST1(name) \
    static inline void wahf_##name(wahf_program_t *p, wahf_reg_t src, wahf_reg_t base) { \
        wahf__line(p, WAHF_INDENT #name " %s, [%s]", wahf__regname(src), wahf__regname(base)); \
    }
WAHF__ST1(stlr)  WAHF__ST1(stlrb) WAHF__ST1(stlrh)
#undef WAHF__ST1

/* Store-exclusive variants (status, src, base) */
#define WAHF__STX(name) \
    static inline void wahf_##name(wahf_program_t *p, wahf_reg_t status, wahf_reg_t src, wahf_reg_t base) { \
        wahf__line(p, WAHF_INDENT #name " %s, %s, [%s]", \
                   wahf__regname(status), wahf__regname(src), wahf__regname(base)); \
    }
WAHF__STX(stlxr)  WAHF__STX(stlxrb) WAHF__STX(stlxrh)
WAHF__STX(stxr)   WAHF__STX(stxrb)  WAHF__STX(stxrh)
#undef WAHF__STX

/** ldxp DST1, DST2, [BASE] — load exclusive pair */
static inline void wahf_ldxp(wahf_program_t *p, wahf_reg_t r1, wahf_reg_t r2, wahf_reg_t base) {
    wahf__line(p, WAHF_INDENT "ldxp %s, %s, [%s]", wahf__regname(r1), wahf__regname(r2), wahf__regname(base));
}

/** ldaxp DST1, DST2, [BASE] — load-acquire exclusive pair */
static inline void wahf_ldaxp(wahf_program_t *p, wahf_reg_t r1, wahf_reg_t r2, wahf_reg_t base) {
    wahf__line(p, WAHF_INDENT "ldaxp %s, %s, [%s]", wahf__regname(r1), wahf__regname(r2), wahf__regname(base));
}

/** stxp STATUS, SRC1, SRC2, [BASE] — store exclusive pair */
static inline void wahf_stxp(wahf_program_t *p, wahf_reg_t status, wahf_reg_t r1, wahf_reg_t r2, wahf_reg_t base) {
    wahf__line(p, WAHF_INDENT "stxp %s, %s, %s, [%s]",
               wahf__regname(status), wahf__regname(r1), wahf__regname(r2), wahf__regname(base));
}

/** stlxp STATUS, SRC1, SRC2, [BASE] — store-release exclusive pair */
static inline void wahf_stlxp(wahf_program_t *p, wahf_reg_t status, wahf_reg_t r1, wahf_reg_t r2, wahf_reg_t base) {
    wahf__line(p, WAHF_INDENT "stlxp %s, %s, %s, [%s]",
               wahf__regname(status), wahf__regname(r1), wahf__regname(r2), wahf__regname(base));
}

/** prfm TYPE, [BASE, #OFFSET] — prefetch memory */
static inline void wahf_prfm(wahf_program_t *p, const char *type, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "prfm %s, [%s, #%lld]", type, wahf__regname(base), off);
}

/** prfum TYPE, [BASE, #OFFSET] — unscaled prefetch */
static inline void wahf_prfum(wahf_program_t *p, const char *type, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "prfum %s, [%s, #%lld]", type, wahf__regname(base), off);
}

/** ldraa DST, [BASE, #OFFSET] — load with address authentication */
static inline void wahf_ldraa(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldraa %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/** ldrab DST, [BASE, #OFFSET] */
static inline void wahf_ldrab(wahf_program_t *p, wahf_reg_t dst, wahf_reg_t base, long long off) {
    wahf__line(p, WAHF_INDENT "ldrab %s, [%s, #%lld]", wahf__regname(dst), wahf__regname(base), off);
}

/* ============================================================================
 *  SECTION 13 — SYSTEM / MISC
 * ========================================================================== */

/** svc #IMM — supervisor call */
static inline void wahf_svc(wahf_program_t *p, long long imm)  { wahf__line(p, WAHF_INDENT "svc #%lld", imm); }

/** hlt #IMM */
static inline void wahf_hlt(wahf_program_t *p, long long imm)  { wahf__line(p, WAHF_INDENT "hlt #%lld", imm); }

/** brk #IMM */
static inline void wahf_brk(wahf_program_t *p, long long imm)  { wahf__line(p, WAHF_INDENT "brk #%lld", imm); }

/* Barrier / hint instructions */
static inline void wahf_wfe(wahf_program_t *p)   { wahf__line(p, WAHF_INDENT "wfe");   }
static inline void wahf_wfi(wahf_program_t *p)   { wahf__line(p, WAHF_INDENT "wfi");   }
static inline void wahf_sev(wahf_program_t *p)   { wahf__line(p, WAHF_INDENT "sev");   }
static inline void wahf_sevl(wahf_program_t *p)  { wahf__line(p, WAHF_INDENT "sevl");  }
static inline void wahf_isb(wahf_program_t *p)   { wahf__line(p, WAHF_INDENT "isb");   }
static inline void wahf_dsb(wahf_program_t *p)   { wahf__line(p, WAHF_INDENT "dsb");   }
static inline void wahf_dmb(wahf_program_t *p)   { wahf__line(p, WAHF_INDENT "dmb");   }
static inline void wahf_yield(wahf_program_t *p) { wahf__line(p, WAHF_INDENT "yield"); }
static inline void wahf_clrex(wahf_program_t *p) { wahf__line(p, WAHF_INDENT "clrex"); }
static inline void wahf_eret(wahf_program_t *p)  { wahf__line(p, WAHF_INDENT "eret");  }
static inline void wahf_drps(wahf_program_t *p)  { wahf__line(p, WAHF_INDENT "drps");  }

/** mrs DST, SYSREG — move system register to GP register */
static inline void wahf_mrs(wahf_program_t *p, wahf_reg_t dst, const char *sysreg) {
    wahf__line(p, WAHF_INDENT "mrs %s, %s", wahf__regname(dst), sysreg);
}

/** msr SYSREG, SRC — move GP register to system register */
static inline void wahf_msr(wahf_program_t *p, const char *sysreg, wahf_reg_t src) {
    wahf__line(p, WAHF_INDENT "msr %s, %s", sysreg, wahf__regname(src));
}

/** sys #OP1, CRn, CRm, #OP2 [, SRC] — system instruction */
static inline void wahf_sys(wahf_program_t *p, const char *operands) {
    wahf__line(p, WAHF_INDENT "sys %s", operands);
}

/** sysl DST, #OP1, CRn, CRm, #OP2 — system instruction with result */
static inline void wahf_sysl(wahf_program_t *p, const char *operands) {
    wahf__line(p, WAHF_INDENT "sysl %s", operands);
}

/** ic OP [, REG] — instruction cache maintenance */
static inline void wahf_ic(wahf_program_t *p, const char *op, wahf_reg_t reg) {
    wahf__line(p, WAHF_INDENT "ic %s, %s", op, wahf__regname(reg));
}
static inline void wahf_ic_noarg(wahf_program_t *p, const char *op) {
    wahf__line(p, WAHF_INDENT "ic %s", op);
}

/** dc OP, REG — data cache maintenance */
static inline void wahf_dc(wahf_program_t *p, const char *op, wahf_reg_t reg) {
    wahf__line(p, WAHF_INDENT "dc %s, %s", op, wahf__regname(reg));
}

/** at OP, REG — address translate */
static inline void wahf_at(wahf_program_t *p, const char *op, wahf_reg_t reg) {
    wahf__line(p, WAHF_INDENT "at %s, %s", op, wahf__regname(reg));
}

/** tlbi OP [, REG] — TLB invalidate */
static inline void wahf_tlbi(wahf_program_t *p, const char *op, wahf_reg_t reg) {
    wahf__line(p, WAHF_INDENT "tlbi %s, %s", op, wahf__regname(reg));
}
static inline void wahf_tlbi_noarg(wahf_program_t *p, const char *op) {
    wahf__line(p, WAHF_INDENT "tlbi %s", op);
}

/* ============================================================================
 *  SECTION 14 — HIGH-LEVEL COMPOUND HELPERS
 * ========================================================================== */

/** wahf_print_str — load + puts in one call. Clobbers X0. */
static inline void wahf_print_str(wahf_program_t *p, const char *str) {
    wahf_lds(p, X0, str);
    wahf_puts(p, X0);
}

/** wahf_print_int — itoa into X9, then puts. Clobbers X9. */
static inline void wahf_print_int(wahf_program_t *p, wahf_reg_t reg) {
    wahf_itoa(p, X9, reg);
    wahf_puts(p, X9);
}

/** wahf_print_newline — print newline. Clobbers X0. */
static inline void wahf_print_newline(wahf_program_t *p) {
    wahf_lds(p, X0, "\n");
    wahf_puts(p, X0);
}

/** wahf_function_begin — label + stp fp,lr + set fp */
static inline void wahf_function_begin(wahf_program_t *p, const char *name) {
    wahf_blank(p);
    wahf_comment(p, "--- function: %s ---", name);
    wahf_label(p, name);
    wahf_stp(p, WAHF_FP, WAHF_LR, WAHF_SP, -16);
    wahf_mov_reg(p, WAHF_FP, WAHF_SP);
}

/** wahf_function_end — ldp fp,lr + ret */
static inline void wahf_function_end(wahf_program_t *p) {
    wahf_ldp(p, WAHF_FP, WAHF_LR, WAHF_SP, 16);
    wahf_ret(p);
    wahf_blank(p);
}

/**
 * wahf_loop_begin — emit top-of-loop label + cbz guard.
 * Counter counts DOWN to zero.
 */
static inline void wahf_loop_begin(wahf_program_t *p, wahf_reg_t counter,
                                    const char *loop_label, const char *end_label) {
    wahf_label(p, loop_label);
    wahf_cbz(p, counter, end_label);
}

/** wahf_loop_end — dec + b back + end label */
static inline void wahf_loop_end(wahf_program_t *p, wahf_reg_t counter,
                                  const char *loop_label, const char *end_label) {
    wahf_dec(p, counter);
    wahf_b(p, loop_label);
    wahf_label(p, end_label);
}

/**
 * wahf_if_begin — emit inverted conditional branch to else_label.
 * Caller must emit cmp/tst before calling this.
 */
static inline void wahf_if_begin(wahf_program_t *p, wahf_cond_t cond, const char *else_label) {
    wahf_cond_t inv;
    switch(cond) {
        case WAHF_EQ: inv=WAHF_NE; break; case WAHF_NE: inv=WAHF_EQ; break;
        case WAHF_LT: inv=WAHF_GE; break; case WAHF_GE: inv=WAHF_LT; break;
        case WAHF_GT: inv=WAHF_LE; break; case WAHF_LE: inv=WAHF_GT; break;
        case WAHF_MI: inv=WAHF_PL; break; case WAHF_PL: inv=WAHF_MI; break;
        case WAHF_VS: inv=WAHF_VC; break; case WAHF_VC: inv=WAHF_VS; break;
        case WAHF_HI: inv=WAHF_LS; break; case WAHF_LS: inv=WAHF_HI; break;
        case WAHF_HS: inv=WAHF_LO; break; case WAHF_LO: inv=WAHF_HS; break;
        default: inv=WAHF_AL; break;
    }
    wahf_bcond(p, inv, else_label);
}

static inline void wahf_if_else(wahf_program_t *p, const char *end_label, const char *else_label) {
    wahf_b(p, end_label);
    wahf_label(p, else_label);
}

static inline void wahf_if_end(wahf_program_t *p, const char *end_label) {
    wahf_label(p, end_label);
}

/** wahf_unique_label — fill buf with a unique label "_L0042" */
static inline void wahf_unique_label(char *buf, int buflen) {
    static int counter = 0;
    snprintf(buf, (size_t)buflen, "_L%04d", counter++);
}

/* ============================================================================
 *  SECTION 15 — OPTIONAL MACRO LAYER  (#define WAHF_USE_MACROS)
 * ========================================================================== */
#ifdef WAHF_USE_MACROS

/* Structural */
#define IWA_LABEL(p,n)          wahf_label(p,n)
#define IWA_COMMENT(p,...)      wahf_comment(p,__VA_ARGS__)
#define IWA_BLANK(p)            wahf_blank(p)

/* Custom */
#define IWA_LDS(p,d,s)          wahf_lds(p,d,s)
#define IWA_PUTS(p,r)           wahf_puts(p,r)
#define IWA_DMP(p,r)            wahf_dmp(p,r)
#define IWA_HALT(p)             wahf_halt(p)
#define IWA_NOP(p)              wahf_nop(p)
#define IWA_GETI(p,r)           wahf_geti(p,r)
#define IWA_GETS(p,r)           wahf_gets(p,r)
#define IWA_RAND(p,r)           wahf_rand(p,r)
#define IWA_TIME(p,r)           wahf_time(p,r)
#define IWA_INC(p,r)            wahf_inc(p,r)
#define IWA_DEC(p,r)            wahf_dec(p,r)
#define IWA_ABS(p,r)            wahf_abs(p,r)
#define IWA_NOT(p,r)            wahf_not(p,r)
#define IWA_CLRR(p,r)           wahf_clrr(p,r)
#define IWA_SWP(p,a,b)          wahf_swp(p,a,b)
#define IWA_PUSH(p,r)           wahf_push(p,r)
#define IWA_POP(p,r)            wahf_pop(p,r)
#define IWA_SHL(p,r,i)          wahf_shl(p,r,i)
#define IWA_SHR(p,r,i)          wahf_shr(p,r,i)
#define IWA_ITOA(p,d,s)         wahf_itoa(p,d,s)
#define IWA_ATOI(p,d,s)         wahf_atoi(p,d,s)
#define IWA_STRLEN(p,d,s)       wahf_strlen(p,d,s)
#define IWA_STRCPY(p,d,s)       wahf_strcpy(p,d,s)
#define IWA_STRCAT(p,d,s)       wahf_strcat(p,d,s)
#define IWA_STRCMP(p,d,a,b)     wahf_strcmp(p,d,a,b)
#define IWA_MEMCPY(p,d,s,l)     wahf_memcpy(p,d,s,l)
#define IWA_MEMSET(p,d,v,l)     wahf_memset(p,d,v,l)

/* Data movement */
#define IWA_MOV(p,d,s)          wahf_mov_reg(p,d,s)
#define IWA_MOVI(p,d,i)         wahf_mov_imm(p,d,i)
#define IWA_MVN(p,d,s)          wahf_mvn(p,d,s)
#define IWA_MOVZ(p,d,i,sh)      wahf_movz(p,d,i,sh)
#define IWA_MOVN(p,d,i,sh)      wahf_movn(p,d,i,sh)
#define IWA_MOVK(p,d,i,sh)      wahf_movk(p,d,i,sh)

/* Load/store */
#define IWA_LDR(p,d,b,o)        wahf_ldr(p,d,b,o)
#define IWA_STR(p,s,b,o)        wahf_str(p,s,b,o)
#define IWA_LDRB(p,d,b,o)       wahf_ldrb(p,d,b,o)
#define IWA_LDRH(p,d,b,o)       wahf_ldrh(p,d,b,o)
#define IWA_LDRSB(p,d,b,o)      wahf_ldrsb(p,d,b,o)
#define IWA_LDRSH(p,d,b,o)      wahf_ldrsh(p,d,b,o)
#define IWA_LDRSW(p,d,b,o)      wahf_ldrsw(p,d,b,o)
#define IWA_STRB(p,s,b,o)       wahf_strb(p,s,b,o)
#define IWA_STRH(p,s,b,o)       wahf_strh(p,s,b,o)
#define IWA_LDP(p,a,b,base,o)   wahf_ldp(p,a,b,base,o)
#define IWA_STP(p,a,b,base,o)   wahf_stp(p,a,b,base,o)
#define IWA_ADR(p,d,l)          wahf_adr(p,d,l)
#define IWA_ADRP(p,d,l)         wahf_adrp(p,d,l)

/* Arithmetic */
#define IWA_ADD(p,d,a,b)        wahf_add(p,d,a,b)
#define IWA_ADDI(p,d,a,i)       wahf_add_imm(p,d,a,i)
#define IWA_ADDS(p,d,a,b)       wahf_adds(p,d,a,b)
#define IWA_SUB(p,d,a,b)        wahf_sub(p,d,a,b)
#define IWA_SUBI(p,d,a,i)       wahf_sub_imm(p,d,a,i)
#define IWA_SUBS(p,d,a,b)       wahf_subs(p,d,a,b)
#define IWA_MUL(p,d,a,b)        wahf_mul(p,d,a,b)
#define IWA_UDIV(p,d,a,b)       wahf_udiv(p,d,a,b)
#define IWA_SDIV(p,d,a,b)       wahf_sdiv(p,d,a,b)
#define IWA_MOD(p,d,a,b)        wahf_mod(p,d,a,b)
#define IWA_NEG(p,d,s)          wahf_neg(p,d,s)
#define IWA_NEGS(p,d,s)         wahf_negs(p,d,s)
#define IWA_ADC(p,d,a,b)        wahf_adc(p,d,a,b)
#define IWA_SBC(p,d,a,b)        wahf_sbc(p,d,a,b)
#define IWA_MADD(p,d,a,b,c)     wahf_madd(p,d,a,b,c)
#define IWA_MSUB(p,d,a,b,c)     wahf_msub(p,d,a,b,c)
#define IWA_MNEG(p,d,a,b)       wahf_mneg(p,d,a,b)
#define IWA_SMULL(p,d,a,b)      wahf_smull(p,d,a,b)
#define IWA_UMULL(p,d,a,b)      wahf_umull(p,d,a,b)
#define IWA_MAX2(p,d,a,b)       wahf_max2(p,d,a,b)
#define IWA_MIN2(p,d,a,b)       wahf_min2(p,d,a,b)

/* Logical */
#define IWA_AND(p,d,a,b)        wahf_and(p,d,a,b)
#define IWA_ANDI(p,d,a,i)       wahf_and_imm(p,d,a,i)
#define IWA_ANDS(p,d,a,b)       wahf_ands(p,d,a,b)
#define IWA_ORR(p,d,a,b)        wahf_orr(p,d,a,b)
#define IWA_ORN(p,d,a,b)        wahf_orn(p,d,a,b)
#define IWA_EOR(p,d,a,b)        wahf_eor(p,d,a,b)
#define IWA_EON(p,d,a,b)        wahf_eon(p,d,a,b)
#define IWA_BIC(p,d,a,b)        wahf_bic(p,d,a,b)

/* Shifts */
#define IWA_LSL(p,d,s,i)        wahf_lsl_imm(p,d,s,i)
#define IWA_LSR(p,d,s,i)        wahf_lsr_imm(p,d,s,i)
#define IWA_ASR(p,d,s,i)        wahf_asr_imm(p,d,s,i)
#define IWA_ROR(p,d,s,i)        wahf_ror_imm(p,d,s,i)

/* Comparison */
#define IWA_CMP(p,a,b)          wahf_cmp(p,a,b)
#define IWA_CMPI(p,a,i)         wahf_cmp_imm(p,a,i)
#define IWA_CMN(p,a,b)          wahf_cmn(p,a,b)
#define IWA_TST(p,a,b)          wahf_tst(p,a,b)

/* Branches */
#define IWA_B(p,l)              wahf_b(p,l)
#define IWA_BL(p,l)             wahf_bl(p,l)
#define IWA_BR(p,r)             wahf_br(p,r)
#define IWA_BLR(p,r)            wahf_blr(p,r)
#define IWA_RET(p)              wahf_ret(p)
#define IWA_CALL(p,l)           wahf_call(p,l)
#define IWA_JMP(p,l)            wahf_jmp(p,l)
#define IWA_JEQ(p,l)            wahf_jeq(p,l)
#define IWA_JNE(p,l)            wahf_jne(p,l)
#define IWA_JLT(p,l)            wahf_jlt(p,l)
#define IWA_JLE(p,l)            wahf_jle(p,l)
#define IWA_JGT(p,l)            wahf_jgt(p,l)
#define IWA_JGE(p,l)            wahf_jge(p,l)
#define IWA_CBZ(p,r,l)          wahf_cbz(p,r,l)
#define IWA_CBNZ(p,r,l)         wahf_cbnz(p,r,l)
#define IWA_TBZ(p,r,b,l)        wahf_tbz(p,r,b,l)
#define IWA_TBNZ(p,r,b,l)       wahf_tbnz(p,r,b,l)

/* Conditional select */
#define IWA_CSEL(p,d,a,b,c)     wahf_csel(p,d,a,b,c)
#define IWA_CSET(p,d,c)         wahf_cset(p,d,c)
#define IWA_CINC(p,d,s,c)       wahf_cinc(p,d,s,c)
#define IWA_CNEG(p,d,s,c)       wahf_cneg(p,d,s,c)
#define IWA_CCMP(p,a,b,n,c)     wahf_ccmp(p,a,b,n,c)

/* Bit manipulation */
#define IWA_CLZ(p,d,s)          wahf_clz(p,d,s)
#define IWA_RBIT(p,d,s)         wahf_rbit(p,d,s)
#define IWA_REV(p,d,s)          wahf_rev(p,d,s)
#define IWA_UBFX(p,d,s,l,w)    wahf_ubfx(p,d,s,l,w)
#define IWA_SBFX(p,d,s,l,w)    wahf_sbfx(p,d,s,l,w)
#define IWA_BFI(p,d,s,l,w)     wahf_bfi(p,d,s,l,w)
#define IWA_SXTB(p,d,s)         wahf_sxtb(p,d,s)
#define IWA_SXTH(p,d,s)         wahf_sxth(p,d,s)
#define IWA_SXTW(p,d,s)         wahf_sxtw(p,d,s)
#define IWA_UXTB(p,d,s)         wahf_uxtb(p,d,s)
#define IWA_UXTH(p,d,s)         wahf_uxth(p,d,s)

/* Floating-point */
#define IWA_FADD(p,d,a,b)       wahf_fadd(p,d,a,b)
#define IWA_FSUB(p,d,a,b)       wahf_fsub(p,d,a,b)
#define IWA_FMUL(p,d,a,b)       wahf_fmul(p,d,a,b)
#define IWA_FDIV(p,d,a,b)       wahf_fdiv(p,d,a,b)
#define IWA_FSQRT(p,d,s)        wahf_fsqrt(p,d,s)
#define IWA_FCMP(p,a,b)         wahf_fcmp(p,a,b)
#define IWA_FCSEL(p,d,a,b,c)    wahf_fcsel(p,d,a,b,c)
#define IWA_SCVTF(p,d,s)        wahf_scvtf(p,d,s)
#define IWA_FCVTZS(p,d,s)       wahf_fcvtzs(p,d,s)
#define IWA_FMADD(p,d,a,b,c)    wahf_fmadd(p,d,a,b,c)
#define IWA_FMIN(p,d,a,b)       wahf_fmin(p,d,a,b)
#define IWA_FMAX(p,d,a,b)       wahf_fmax(p,d,a,b)

/* System */
#define IWA_SVC(p,i)            wahf_svc(p,i)
#define IWA_MRS(p,d,s)          wahf_mrs(p,d,s)
#define IWA_MSR(p,s,r)          wahf_msr(p,s,r)

/* Compound helpers */
#define IWA_PRINT(p,str)        wahf_print_str(p,str)
#define IWA_PRINTLN(p,str)      do { wahf_print_str(p,str); wahf_print_newline(p); } while(0)
#define IWA_PRINT_INT(p,r)      wahf_print_int(p,r)
#define IWA_FUNC_BEGIN(p,name)  wahf_function_begin(p,name)
#define IWA_FUNC_END(p)         wahf_function_end(p)

#endif /* WAHF_USE_MACROS */

/* ============================================================================
 * Version
 * ========================================================================== */
#define WAHF_VERSION_MAJOR 2
#define WAHF_VERSION_MINOR 0
#define WAHF_VERSION_PATCH 0
#define WAHF_VERSION_STR   "2.0.0"

static inline void wahf_print_version(void) {
    printf("WAHF (WebAssembler Header File) v%s\n", WAHF_VERSION_STR);
    printf("  Full opcode coverage for WebAssembler .iwa assembler\n");
}

#ifdef __cplusplus
}
#endif
#endif /* WAHF_H */

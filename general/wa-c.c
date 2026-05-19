#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

#define MAX_LINE      4096
#define MAX_TOKENS    64
#define MAX_LABELS    4096
#define MAX_FIXUPS    8192
#define MAX_INSTRS    65536
#define TOKEN_MAX     512

typedef enum {
    OP_LDS    = 0,
    OP_ADL    = 1,
    OP_MOV    = 2,
    OP_LDR    = 3,
    OP_STR    = 4,
    OP_ADD    = 5,
    OP_SUB    = 6,
    OP_MUL    = 7,
    OP_UDIV   = 8,
    OP_SDIV   = 9,
    OP_AND    = 10,
    OP_ORR    = 11,
    OP_EOR    = 12,
    OP_MVN    = 13,
    OP_LSL    = 14,
    OP_LSR    = 15,
    OP_ASR    = 16,
    OP_ROR    = 17,
    OP_B      = 18,
    OP_BL     = 19,
    OP_BR     = 20,
    OP_BLR    = 21,
    OP_RET    = 22,
    OP_CBZ    = 23,
    OP_CBNZ   = 24,
    OP_TBZ    = 25,
    OP_TBNZ   = 26,
    OP_BEQ    = 27,
    OP_BNE    = 28,
    OP_BLT    = 29,
    OP_BLE    = 30,
    OP_BGT    = 31,
    OP_BGE    = 32,
    OP_BLO    = 33,
    OP_BLS    = 34,
    OP_BHI    = 35,
    OP_BHS    = 36,
    OP_BMI    = 37,
    OP_BPL    = 38,
    OP_BVS    = 39,
    OP_BVC    = 40,
    OP_BAL    = 41,
    OP_CMP    = 42,
    OP_CMN    = 43,
    OP_TST    = 44,
    OP_NEG    = 45,
    OP_NEGS   = 46,
    OP_SBC    = 47,
    OP_SBCS   = 48,
    OP_ADC    = 49,
    OP_ADCS   = 50,
    OP_ADDS   = 51,
    OP_SUBS   = 52,
    OP_MADD   = 53,
    OP_MSUB   = 54,
    OP_MNEG   = 55,
    OP_SMULL  = 56,
    OP_UMULL  = 57,
    OP_SMULH  = 58,
    OP_UMULH  = 59,
    OP_SMADDL = 60,
    OP_UMADDL = 61,
    OP_SMSUBL = 62,
    OP_UMSUBL = 63,
    OP_LDRB   = 64,
    OP_LDRH   = 65,
    OP_LDRSB  = 66,
    OP_LDRSH  = 67,
    OP_LDRSW  = 68,
    OP_STRB   = 69,
    OP_STRH   = 70,
    OP_LDP    = 71,
    OP_STP    = 72,
    OP_ADRP   = 73,
    OP_ADR    = 74,
    OP_NOP    = 75,
    OP_SVC    = 76,
    OP_HLT    = 77,
    OP_BRK    = 78,
    OP_WFE    = 79,
    OP_WFI    = 80,
    OP_SEV    = 81,
    OP_SEVL   = 82,
    OP_ISB    = 83,
    OP_DSB    = 84,
    OP_DMB    = 85,
    OP_CLREX  = 86,
    OP_YIELD  = 87,
    OP_ERET   = 88,
    OP_DRPS   = 89,
    OP_MRS    = 90,
    OP_MSR    = 91,
    OP_SYS    = 92,
    OP_SYSL   = 93,
    OP_IC     = 94,
    OP_DC     = 95,
    OP_AT     = 96,
    OP_TLBI   = 97,
    OP_CLZ    = 98,
    OP_CLS    = 99,
    OP_RBIT   = 100,
    OP_REV    = 101,
    OP_REV16  = 102,
    OP_REV32  = 103,
    OP_REV64  = 104,
    OP_EXTR   = 105,
    OP_SBFM   = 106,
    OP_BFM    = 107,
    OP_UBFM   = 108,
    OP_SBFX   = 109,
    OP_SBFIZ  = 110,
    OP_BFXIL  = 111,
    OP_BFI    = 112,
    OP_UBFX   = 113,
    OP_UBFIZ  = 114,
    OP_SXTB   = 115,
    OP_SXTH   = 116,
    OP_SXTW   = 117,
    OP_UXTB   = 118,
    OP_UXTH   = 119,
    OP_ANDS   = 120,
    OP_BICS   = 121,
    OP_BIC    = 122,
    OP_EON    = 123,
    OP_ORN    = 124,
    OP_MOVZ   = 125,
    OP_MOVN   = 126,
    OP_MOVK   = 127,
    OP_FMOV   = 128,
    OP_FADD   = 129,
    OP_FSUB   = 130,
    OP_FMUL   = 131,
    OP_FDIV   = 132,
    OP_FABS   = 133,
    OP_FNEG   = 134,
    OP_FSQRT  = 135,
    OP_FCMP   = 136,
    OP_FCMPE  = 137,
    OP_FCCMP  = 138,
    OP_FCCMPE = 139,
    OP_FCSEL  = 140,
    OP_FCVT   = 141,
    OP_FCVTAS = 142,
    OP_FCVTAU = 143,
    OP_FCVTMS = 144,
    OP_FCVTMU = 145,
    OP_FCVTNS = 146,
    OP_FCVTNU = 147,
    OP_FCVTPS = 148,
    OP_FCVTPU = 149,
    OP_FCVTZS = 150,
    OP_FCVTZU = 151,
    OP_SCVTF  = 152,
    OP_UCVTF  = 153,
    OP_FMADD  = 154,
    OP_FMSUB  = 155,
    OP_FNMADD = 156,
    OP_FNMSUB = 157,
    OP_FMINNM = 158,
    OP_FMAXNM = 159,
    OP_FMIN   = 160,
    OP_FMAX   = 161,
    OP_CSEL   = 162,
    OP_CSINC  = 163,
    OP_CSINV  = 164,
    OP_CSNEG  = 165,
    OP_CSET   = 166,
    OP_CSETM  = 167,
    OP_CINC   = 168,
    OP_CINV   = 169,
    OP_CNEG   = 170,
    OP_CCMP   = 171,
    OP_CCMN   = 172,
    OP_LDAR   = 173,
    OP_LDARB  = 174,
    OP_LDARH  = 175,
    OP_LDAXR  = 176,
    OP_LDAXRB = 177,
    OP_LDAXRH = 178,
    OP_LDXR   = 179,
    OP_LDXRB  = 180,
    OP_LDXRH  = 181,
    OP_STLR   = 182,
    OP_STLRB  = 183,
    OP_STLRH  = 184,
    OP_STLXR  = 185,
    OP_STLXRB = 186,
    OP_STLXRH = 187,
    OP_STXR   = 188,
    OP_STXRB  = 189,
    OP_STXRH  = 190,
    OP_LDXP   = 191,
    OP_LDAXP  = 192,
    OP_STXP   = 193,
    OP_STLXP  = 194,
    OP_PRFM   = 195,
    OP_PRFUM  = 196,
    OP_LDRAA  = 197,
    OP_LDRAB  = 198,
    OP_LDR_LIT= 199,
    OP_PUSH   = 200,
    OP_POP    = 201,
    OP_SWP    = 203,
    OP_DMP    = 204,
    OP_HALT   = 205,
    OP_PUTS   = 206,
    OP_GETI   = 207,
    OP_GETS   = 208,
    OP_RAND   = 209,
    OP_TIME   = 210,
    OP_CLRR   = 211,
    OP_INC    = 212,
    OP_DEC    = 213,
    OP_ABS    = 214,
    OP_MAX2   = 215,
    OP_MIN2   = 216,
    OP_MOD    = 217,
    OP_NOT    = 218,
    OP_SHL    = 219,
    OP_SHR    = 220,
    OP_MEMCPY = 221,
    OP_MEMSET = 222,
    OP_STRCPY = 223,
    OP_STRCAT_OP = 224,
    OP_STRCMP_OP = 225,
    OP_STRLEN = 226,
    OP_ITOA   = 227,
    OP_ATOI   = 228,
    OP_CALL   = 230,
    OP_JMP    = 231,
    OP_JEQ    = 232,
    OP_JNE    = 233,
    OP_JLT    = 234,
    OP_JLE    = 235,
    OP_JGT    = 236,
    OP_JGE    = 237,
    OP_ALIGN  = 238,
    OP_WORD   = 239,
    OP_DWORD  = 240,
    OP_BYTE_D = 241,
    OP_SPACE  = 242,
    OP_ASCII  = 243,
    OP_ASCIZ  = 244
} Opcode;

typedef struct {
    int family;                                                                                
    int number;                                              
} RegInfo;

typedef struct {
    char  name[TOKEN_MAX];
    int   line_index;                                                   
} Label;

static Label  labels[MAX_LABELS];
static int    label_count = 0;

typedef struct {
    int   bytecode_line;                                              
    int   token_index;                                                    
    char  label_name[TOKEN_MAX];
} Fixup;

static Fixup  fixups[MAX_FIXUPS];
static int    fixup_count = 0;

typedef struct {
    char  line[MAX_LINE];
} OutLine;

static OutLine  out_lines[MAX_INSTRS];
static int      out_count = 0;

static void strip(char *s) {
    int start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, strlen(s) - start + 1);
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
}

static void to_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static int parse_register(const char *tok, int *out_family, int *out_num) {
    char tmp[TOKEN_MAX];
    strncpy(tmp, tok, TOKEN_MAX-1);
    tmp[TOKEN_MAX-1] = '\0';
    int len = (int)strlen(tmp);
    while (len > 0 && (tmp[len-1] == ',' || tmp[len-1] == ']' ||
                        tmp[len-1] == '!' || tmp[len-1] == ' ')) {
        tmp[--len] = '\0';
    }
    to_lower(tmp);

    if (strcmp(tmp, "sp")  == 0) { *out_family = 2; *out_num = 31; return 1; }
    if (strcmp(tmp, "lr")  == 0) { *out_family = 3; *out_num = 30; return 1; }
    if (strcmp(tmp, "pc")  == 0) { *out_family = 4; *out_num = 32; return 1; }
    if (strcmp(tmp, "xzr") == 0) { *out_family = 5; *out_num = 31; return 1; }
    if (strcmp(tmp, "wzr") == 0) { *out_family = 6; *out_num = 31; return 1; }
    if (strcmp(tmp, "fp")  == 0) { *out_family = 7; *out_num = 29; return 1; }
    if (strcmp(tmp, "ip0") == 0) { *out_family = 8; *out_num = 16; return 1; }
    if (strcmp(tmp, "ip1") == 0) { *out_family = 9; *out_num = 17; return 1; }

    if (tmp[0] == 'x' && isdigit((unsigned char)tmp[1])) {
        int n = atoi(tmp + 1);
        if (n >= 0 && n <= 30) { *out_family = 0; *out_num = n; return 1; }
    }
    if (tmp[0] == 'w' && isdigit((unsigned char)tmp[1])) {
        int n = atoi(tmp + 1);
        if (n >= 0 && n <= 30) { *out_family = 1; *out_num = n; return 1; }
    }
    if (tmp[0] == 'd' && isdigit((unsigned char)tmp[1])) {
        int n = atoi(tmp + 1);
        if (n >= 0 && n <= 31) { *out_family = 10; *out_num = n; return 1; }
    }
    if (tmp[0] == 's' && isdigit((unsigned char)tmp[1])) {
        int n = atoi(tmp + 1);
        if (n >= 0 && n <= 31) { *out_family = 11; *out_num = n; return 1; }
    }
    if (tmp[0] == 'q' && isdigit((unsigned char)tmp[1])) {
        int n = atoi(tmp + 1);
        if (n >= 0 && n <= 31) { *out_family = 12; *out_num = n; return 1; }
    }
    if (tmp[0] == 'v' && isdigit((unsigned char)tmp[1])) {
        int n = atoi(tmp + 1);
        if (n >= 0 && n <= 31) { *out_family = 13; *out_num = n; return 1; }
    }
    return 0;
}

static void reg_str(int family, int num, char *out) {
    sprintf(out, "%d:%d", family, num);
}

static long long parse_imm(const char *tok) {
    char tmp[TOKEN_MAX];
    strncpy(tmp, tok, TOKEN_MAX-1);
    tmp[TOKEN_MAX-1] = '\0';
    int len = (int)strlen(tmp);
    while (len > 0 && (tmp[len-1]==',' || tmp[len-1]==']' || tmp[len-1]=='!'))
        tmp[--len] = '\0';
    const char *p = tmp;
    if (*p == '#') p++;
    if (p[0]=='0' && (p[1]=='x' || p[1]=='X'))
        return (long long)strtoll(p, NULL, 16);
    return (long long)strtoll(p, NULL, 10);
}

static int tokenize(char *line, char tokens[][TOKEN_MAX], int max_toks) {
    int count = 0;
    char *p = line;

    int in_q = 0;
    for (char *c = line; *c; c++) {
        if (*c == '"') in_q = !in_q;
        if (*c == ';' && !in_q) { *c = '\0'; break; }
    }
    strip(line);
    if (!*line) return 0;

    while (*p && count < max_toks) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;

        if (*p == '"') {
            p++;                         
            int ti = 0;
            tokens[count][ti++] = '"';                   
            while (*p && *p != '"' && ti < TOKEN_MAX-2) {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    switch (*p) {
                        case 'n':  tokens[count][ti++] = '\n'; break;
                        case 't':  tokens[count][ti++] = '\t'; break;
                        case 'r':  tokens[count][ti++] = '\r'; break;
                        case '0':  tokens[count][ti++] = '\0'; break;
                        case '\\': tokens[count][ti++] = '\\'; break;
                        case '"':  tokens[count][ti++] = '"';  break;
                        default:   tokens[count][ti++] = '\\'; tokens[count][ti++] = *p; break;
                    }
                } else {
                    tokens[count][ti++] = *p;
                }
                p++;
            }
            if (*p == '"') p++;                         
            tokens[count][ti] = '\0';
            count++;
        } else if (*p == '[') {
            int ti = 0;
            while (*p && *p != ']' && ti < TOKEN_MAX-2)
                tokens[count][ti++] = *p++;
            if (*p == ']') tokens[count][ti++] = *p++;
            if (*p == '!') tokens[count][ti++] = *p++;
            tokens[count][ti] = '\0';
            count++;
        } else {
            int ti = 0;
            while (*p && !isspace((unsigned char)*p) && *p != ',' &&
                   *p != '[' && ti < TOKEN_MAX-2)
                tokens[count][ti++] = *p++;
            tokens[count][ti] = '\0';
            count++;
        }
    }
    return count;
}

static void add_label(const char *name, int line_idx) {
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "Error: too many labels\n");
        exit(1);
    }
    strncpy(labels[label_count].name, name, TOKEN_MAX-1);
    labels[label_count].line_index = line_idx;
    label_count++;
}

static int find_label(const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].line_index;
    }
    return -1;
}

static void add_fixup(int bytecode_line, int token_index, const char *lname) {
    if (fixup_count >= MAX_FIXUPS) {
        fprintf(stderr, "Error: too many fixups\n");
        exit(1);
    }
    fixups[fixup_count].bytecode_line = bytecode_line;
    fixups[fixup_count].token_index   = token_index;
    strncpy(fixups[fixup_count].label_name, lname, TOKEN_MAX-1);
    fixup_count++;
}

static int emit(const char *fmt, ...) {
    if (out_count >= MAX_INSTRS) {
        fprintf(stderr, "Error: too many instructions\n");
        exit(1);
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out_lines[out_count].line, MAX_LINE, fmt, ap);
    va_end(ap);
    return out_count++;
}

static void escape_string(const char *src, char *dst, int maxlen) {
    int di = 0;
    const char *p = src;
    if (*p == '"') p++;
    while (*p && di < maxlen - 4) {
        unsigned char c = (unsigned char)*p++;
        if (c == ' ')  { dst[di++]='\\'; dst[di++]='~'; }
        else if (c == '\n') { dst[di++]='\\'; dst[di++]='n'; }
        else if (c == '\t') { dst[di++]='\\'; dst[di++]='t'; }
        else if (c == '\r') { dst[di++]='\\'; dst[di++]='r'; }
        else if (c == '\\') { dst[di++]='\\'; dst[di++]='\\'; }
        else if (c == 0)    { dst[di++]='\\'; dst[di++]='0'; }
        else dst[di++] = c;
    }
    dst[di] = '\0';
}

static int is_label_ref(const char *tok) {
    if (!tok || !*tok) return 0;
    if (tok[0] == '#') return 0;
    if (tok[0] == '"') return 0;
    if (tok[0] == '[') return 0;
    int f, n;
    if (parse_register(tok, &f, &n)) return 0;
    char *end;
    strtoll(tok, &end, 10);
    if (*end == '\0') return 0;
    return 1;
}

static int cond_str_to_int(const char *cc) {
    if (strcmp(cc,"eq")==0) return 0;
    if (strcmp(cc,"ne")==0) return 1;
    if (strcmp(cc,"cs")==0||strcmp(cc,"hs")==0) return 2;
    if (strcmp(cc,"cc")==0||strcmp(cc,"lo")==0) return 3;
    if (strcmp(cc,"mi")==0) return 4;
    if (strcmp(cc,"pl")==0) return 5;
    if (strcmp(cc,"vs")==0) return 6;
    if (strcmp(cc,"vc")==0) return 7;
    if (strcmp(cc,"hi")==0) return 8;
    if (strcmp(cc,"ls")==0) return 9;
    if (strcmp(cc,"ge")==0) return 10;
    if (strcmp(cc,"lt")==0) return 11;
    if (strcmp(cc,"gt")==0) return 12;
    if (strcmp(cc,"le")==0) return 13;
    if (strcmp(cc,"al")==0) return 14;
    if (strcmp(cc,"nv")==0) return 15;
    return -1;
}

static void compile_line(const char *raw_line, int *src_lineno) {
    char line[MAX_LINE];
    strncpy(line, raw_line, MAX_LINE-1);
    line[MAX_LINE-1] = '\0';
    strip(line);

    char tokens[MAX_TOKENS][TOKEN_MAX];
    int  ntok = tokenize(line, tokens, MAX_TOKENS);
    if (ntok == 0) return;

    char *first = tokens[0];
    int flen = (int)strlen(first);
    if (first[flen-1] == ':') {
        char lname[TOKEN_MAX];
        strncpy(lname, first, TOKEN_MAX-1);
        lname[TOKEN_MAX-1] = '\0';
        lname[strlen(lname)-1] = '\0';                  
        add_label(lname, out_count);
        if (ntok > 1) {
            char rest[MAX_LINE] = {0};
            for (int i = 1; i < ntok; i++) {
                strcat(rest, tokens[i]);
                if (i < ntok-1) strcat(rest, " ");
            }
            compile_line(rest, src_lineno);
        }
        return;
    }

    char mnem[TOKEN_MAX];
    strncpy(mnem, tokens[0], TOKEN_MAX-1);
    mnem[TOKEN_MAX-1] = '\0';
    to_lower(mnem);

    #define REG(idx) \
        ({ int _f, _n; \
           if (!parse_register(tokens[idx], &_f, &_n)) { \
               fprintf(stderr, "Line %d: bad register '%s'\n", *src_lineno, tokens[idx]); \
               exit(1); \
           } \
           char _rs[32]; reg_str(_f, _n, _rs); _rs; })

    #undef REG

    char r0[32], r1[32], r2[32], r3[32];
    int  f0, n0, f1, n1, f2, n2, f3, n3;

    #define PARSE_REG(idx, fv, nv, rs) do { \
        if (!parse_register(tokens[idx], &(fv), &(nv))) { \
            fprintf(stderr, "Line %d: bad register '%s'\n", *src_lineno, tokens[idx]); \
            exit(1); \
        } \
        reg_str(fv, nv, rs); \
    } while(0)


    if (strcmp(mnem, "lds") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: lds needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        char escaped[MAX_LINE];
        escape_string(tokens[2], escaped, MAX_LINE);
        emit("%d %s %s", OP_LDS, r0, escaped);
        return;
    }

    if (strcmp(mnem, "adl") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: adl needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_ADL, r0, r1);
        return;
    }

    if (strcmp(mnem, "strlen") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: strlen needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_STRLEN, r0, r1);
        return;
    }

    if (strcmp(mnem, "swp") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: swp needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_SWP, r0, r1);
        return;
    }

    if (strcmp(mnem, "dmp") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: dmp needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_DMP, r0);
        return;
    }

    if (strcmp(mnem, "halt") == 0) {
        emit("%d", OP_HALT);
        return;
    }

    if (strcmp(mnem, "puts") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: puts needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_PUTS, r0);
        return;
    }

    if (strcmp(mnem, "geti") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: geti needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_GETI, r0);
        return;
    }

    if (strcmp(mnem, "gets") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: gets needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_GETS, r0);
        return;
    }

    if (strcmp(mnem, "rand") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: rand needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_RAND, r0);
        return;
    }

    if (strcmp(mnem, "time") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: time needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_TIME, r0);
        return;
    }

    if (strcmp(mnem, "clrr") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: clrr needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_CLRR, r0);
        return;
    }

    if (strcmp(mnem, "inc") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: inc needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_INC, r0);
        return;
    }

    if (strcmp(mnem, "dec") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: dec needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_DEC, r0);
        return;
    }

    if (strcmp(mnem, "abs") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: abs needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_ABS, r0);
        return;
    }

    if (strcmp(mnem, "max2") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: max2 needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MAX2, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "min2") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: min2 needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MIN2, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "mod") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: mod needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MOD, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "not") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: not needs 1 operand\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_NOT, r0);
        return;
    }

    if (strcmp(mnem, "shl") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: shl needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s %lld", OP_SHL, r0, parse_imm(tokens[2]));
        return;
    }

    if (strcmp(mnem, "shr") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: shr needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s %lld", OP_SHR, r0, parse_imm(tokens[2]));
        return;
    }

    if (strcmp(mnem, "memcpy") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: memcpy needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MEMCPY, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "memset") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: memset needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MEMSET, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "strcpy") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: strcpy needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_STRCPY, r0, r1);
        return;
    }

    if (strcmp(mnem, "strcat") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: strcat needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_STRCAT_OP, r0, r1);
        return;
    }

    if (strcmp(mnem, "strcmp") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: strcmp needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_STRCMP_OP, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "itoa") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: itoa needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_ITOA, r0, r1);
        return;
    }

    if (strcmp(mnem, "atoi") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: atoi needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_ATOI, r0, r1);
        return;
    }

    if (strcmp(mnem, "call") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: call needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_CALL);
        add_fixup(ln, 1, tokens[1]);
        return;
    }

    if (strcmp(mnem, "jmp") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jmp needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JMP);
        add_fixup(ln, 1, tokens[1]);
        return;
    }

    if (strcmp(mnem, "jeq") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jeq needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JEQ);
        add_fixup(ln, 1, tokens[1]);
        return;
    }
    if (strcmp(mnem, "jne") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jne needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JNE);
        add_fixup(ln, 1, tokens[1]);
        return;
    }
    if (strcmp(mnem, "jlt") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jlt needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JLT);
        add_fixup(ln, 1, tokens[1]);
        return;
    }
    if (strcmp(mnem, "jle") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jle needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JLE);
        add_fixup(ln, 1, tokens[1]);
        return;
    }
    if (strcmp(mnem, "jgt") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jgt needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JGT);
        add_fixup(ln, 1, tokens[1]);
        return;
    }
    if (strcmp(mnem, "jge") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: jge needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_JGE);
        add_fixup(ln, 1, tokens[1]);
        return;
    }

    if (strcmp(mnem, ".align") == 0) {
        long long n = (ntok > 1) ? parse_imm(tokens[1]) : 4;
        emit("%d %lld", OP_ALIGN, n);
        return;
    }
    if (strcmp(mnem, ".word") == 0) {
        long long v = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_WORD, v);
        return;
    }
    if (strcmp(mnem, ".dword") == 0) {
        long long v = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_DWORD, v);
        return;
    }
    if (strcmp(mnem, ".byte") == 0) {
        long long v = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_BYTE_D, v);
        return;
    }
    if (strcmp(mnem, ".space") == 0) {
        long long v = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_SPACE, v);
        return;
    }
    if (strcmp(mnem, ".ascii") == 0) {
        char escaped[MAX_LINE];
        if (ntok > 1) escape_string(tokens[1], escaped, MAX_LINE);
        else escaped[0] = '\0';
        emit("%d %s", OP_ASCII, escaped);
        return;
    }
    if (strcmp(mnem, ".asciz") == 0) {
        char escaped[MAX_LINE];
        if (ntok > 1) escape_string(tokens[1], escaped, MAX_LINE);
        else escaped[0] = '\0';
        emit("%d %s", OP_ASCIZ, escaped);
        return;
    }


    if (strcmp(mnem, "nop") == 0) {
        emit("%d", OP_NOP);
        return;
    }

    if (strcmp(mnem, "ret") == 0) {
        if (ntok > 1) {
            PARSE_REG(1, f0, n0, r0);
            emit("%d %s", OP_RET, r0);
        } else {
            emit("%d 3:30", OP_RET);                  
        }
        return;
    }

    if (strcmp(mnem, "mov") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: mov needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t, n1t, r1);
            emit("%d %s R %s", OP_MOV, r0, r1);
        } else {
            emit("%d %s I %lld", OP_MOV, r0, parse_imm(tokens[2]));
        }
        return;
    }

    if (strcmp(mnem, "movz") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: movz needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        long long imm = parse_imm(tokens[2]);
        long long shift = 0;
        if (ntok >= 5 && strcmp(tokens[3],"lsl")==0) shift = parse_imm(tokens[4]);
        emit("%d %s %lld %lld", OP_MOVZ, r0, imm, shift);
        return;
    }

    if (strcmp(mnem, "movn") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: movn needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        long long imm = parse_imm(tokens[2]);
        long long shift = 0;
        if (ntok >= 5 && strcmp(tokens[3],"lsl")==0) shift = parse_imm(tokens[4]);
        emit("%d %s %lld %lld", OP_MOVN, r0, imm, shift);
        return;
    }

    if (strcmp(mnem, "movk") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: movk needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        long long imm = parse_imm(tokens[2]);
        long long shift = 0;
        if (ntok >= 5 && strcmp(tokens[3],"lsl")==0) shift = parse_imm(tokens[4]);
        emit("%d %s %lld %lld", OP_MOVK, r0, imm, shift);
        return;
    }

    if (strcmp(mnem, "mvn") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: mvn needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_MVN, r0, r1);
        return;
    }

    if (strcmp(mnem, "ldr") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: ldr needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t, n1t, r1);
            emit("%d %s R %s 0", OP_LDR, r0, r1);
            return;
        }
        char base_tok[TOKEN_MAX];
        long long offset = 0;
        char tmp2[TOKEN_MAX];
        strncpy(tmp2, tokens[2], TOKEN_MAX-1);
        int tl = (int)strlen(tmp2);
        if (tmp2[0]=='[') memmove(tmp2, tmp2+1, tl--);
        if (tl>0 && tmp2[tl-1]==']') { tmp2[tl-1]='\0'; tl--; }
        if (tl>0 && tmp2[tl-1]=='!') { tmp2[tl-1]='\0'; tl--; }
        char *comma = strchr(tmp2, ',');
        if (comma) {
            *comma = '\0';
            offset = parse_imm(comma+1);
        }
        strncpy(base_tok, tmp2, TOKEN_MAX-1);
        strip(base_tok);
        if (ntok >= 4 && comma == NULL) {
            offset = parse_imm(tokens[3]);
        }
        int fb, nb;
        if (!parse_register(base_tok, &fb, &nb)) {
            fprintf(stderr,"Line %d: bad base register in ldr '%s'\n",*src_lineno,base_tok); exit(1);
        }
        reg_str(fb, nb, r1);
        emit("%d %s M %s %lld", OP_LDR, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "str") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: str needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX];
        strncpy(tmp2, tokens[2], TOKEN_MAX-1);
        long long offset = 0;
        char *br = tmp2; int tl = (int)strlen(br);
        if (br[0]=='[') { memmove(br, br+1, tl--); }
        if (tl>0 && br[tl-1]==']') { br[tl-1]='\0'; tl--; }
        if (tl>0 && br[tl-1]=='!') { br[tl-1]='\0'; tl--; }
        char *comma = strchr(br, ',');
        if (comma) { *comma='\0'; offset=parse_imm(comma+1); }
        strip(br);
        if (ntok >= 4 && comma==NULL) offset = parse_imm(tokens[3]);
        int fb, nb;
        if (!parse_register(br, &fb, &nb)) {
            fprintf(stderr,"Line %d: bad base register in str '%s'\n",*src_lineno,br); exit(1);
        }
        reg_str(fb, nb, r1);
        emit("%d %s %s %lld", OP_STR, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "ldrb") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: ldrb needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0;
        char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,',');
        if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br);
        if(ntok>=4&&comma==NULL) offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1);
        emit("%d %s %s %lld", OP_LDRB, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "ldrh") == 0) {
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1); emit("%d %s %s %lld", OP_LDRH, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "ldrsb") == 0) {
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1); emit("%d %s %s %lld", OP_LDRSB, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "ldrsh") == 0) {
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1); emit("%d %s %s %lld", OP_LDRSH, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "ldrsw") == 0) {
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1); emit("%d %s %s %lld", OP_LDRSW, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "strb") == 0) {
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1); emit("%d %s %s %lld", OP_STRB, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "strh") == 0) {
        PARSE_REG(1, f0, n0, r0);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r1); emit("%d %s %s %lld", OP_STRH, r0, r1, offset);
        return;
    }

    if (strcmp(mnem, "ldp") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: ldp needs 3+ operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[3],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        if(tl>0&&br[tl-1]=='!'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=5&&comma==NULL)offset=parse_imm(tokens[4]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r2); emit("%d %s %s %s %lld", OP_LDP, r0, r1, r2, offset);
        return;
    }

    if (strcmp(mnem, "stp") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: stp needs 3+ operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        char tmp2[TOKEN_MAX]; strncpy(tmp2,tokens[3],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2; int tl=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl--);}
        if(tl>0&&br[tl-1]==']'){br[tl-1]='\0';tl--;}
        if(tl>0&&br[tl-1]=='!'){br[tl-1]='\0';tl--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=5&&comma==NULL)offset=parse_imm(tokens[4]);
        int fb,nb; if(!parse_register(br,&fb,&nb)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb,nb,r2); emit("%d %s %s %s %lld", OP_STP, r0, r1, r2, offset);
        return;
    }

    if (strcmp(mnem, "add") == 0 || strcmp(mnem, "adds") == 0) {
        int op = (strcmp(mnem,"adds")==0) ? OP_ADDS : OP_ADD;
        if (ntok < 4) { fprintf(stderr,"Line %d: add/adds needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2);
            char shift_type[16] = "none"; long long shift_amt = 0;
            if (ntok >= 6) { strncpy(shift_type, tokens[4], 15); shift_amt = parse_imm(tokens[5]); }
            emit("%d %s %s R %s %s %lld", op, r0, r1, r2, shift_type, shift_amt);
        } else {
            emit("%d %s %s I %lld none 0", op, r0, r1, parse_imm(tokens[3]));
        }
        return;
    }

    if (strcmp(mnem, "sub") == 0 || strcmp(mnem, "subs") == 0) {
        int op = (strcmp(mnem,"subs")==0) ? OP_SUBS : OP_SUB;
        if (ntok < 4) { fprintf(stderr,"Line %d: sub needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2);
            char shift_type[16]="none"; long long shift_amt=0;
            if (ntok>=6){strncpy(shift_type,tokens[4],15);shift_amt=parse_imm(tokens[5]);}
            emit("%d %s %s R %s %s %lld", op, r0, r1, r2, shift_type, shift_amt);
        } else {
            emit("%d %s %s I %lld none 0", op, r0, r1, parse_imm(tokens[3]));
        }
        return;
    }

    if (strcmp(mnem, "mul") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: mul needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MUL, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "udiv") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: udiv needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_UDIV, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "sdiv") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: sdiv needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_SDIV, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "and") == 0 || strcmp(mnem, "ands") == 0) {
        int op = (strcmp(mnem,"ands")==0) ? OP_ANDS : OP_AND;
        if (ntok < 4) { fprintf(stderr,"Line %d: and needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", op, r0, r1, r2);
        } else { emit("%d %s %s I %lld", op, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "orr") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: orr needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", OP_ORR, r0, r1, r2);
        } else { emit("%d %s %s I %lld", OP_ORR, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "orn") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: orn needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_ORN, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "eor") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: eor needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", OP_EOR, r0, r1, r2);
        } else { emit("%d %s %s I %lld", OP_EOR, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "eon") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: eon needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_EON, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "bic") == 0 || strcmp(mnem, "bics") == 0) {
        int op = (strcmp(mnem,"bics")==0) ? OP_BICS : OP_BIC;
        if (ntok < 4) { fprintf(stderr,"Line %d: bic needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "lsl") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: lsl needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", OP_LSL, r0, r1, r2);
        } else { emit("%d %s %s I %lld", OP_LSL, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "lsr") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: lsr needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", OP_LSR, r0, r1, r2);
        } else { emit("%d %s %s I %lld", OP_LSR, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "asr") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: asr needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", OP_ASR, r0, r1, r2);
        } else { emit("%d %s %s I %lld", OP_ASR, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "ror") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: ror needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        int f2t, n2t;
        if (parse_register(tokens[3], &f2t, &n2t)) {
            reg_str(f2t, n2t, r2); emit("%d %s %s R %s", OP_ROR, r0, r1, r2);
        } else { emit("%d %s %s I %lld", OP_ROR, r0, r1, parse_imm(tokens[3])); }
        return;
    }

    if (strcmp(mnem, "cmp") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: cmp needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t, n1t, r1); emit("%d %s R %s", OP_CMP, r0, r1);
        } else { emit("%d %s I %lld", OP_CMP, r0, parse_imm(tokens[2])); }
        return;
    }

    if (strcmp(mnem, "cmn") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: cmn needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t, n1t, r1); emit("%d %s R %s", OP_CMN, r0, r1);
        } else { emit("%d %s I %lld", OP_CMN, r0, parse_imm(tokens[2])); }
        return;
    }

    if (strcmp(mnem, "tst") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: tst needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t, n1t, r1); emit("%d %s R %s", OP_TST, r0, r1);
        } else { emit("%d %s I %lld", OP_TST, r0, parse_imm(tokens[2])); }
        return;
    }

    if (strcmp(mnem, "neg") == 0 || strcmp(mnem, "negs") == 0) {
        int op = (strcmp(mnem,"negs")==0) ? OP_NEGS : OP_NEG;
        if (ntok < 3) { fprintf(stderr,"Line %d: neg needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", op, r0, r1);
        return;
    }

    if (strcmp(mnem, "adc") == 0 || strcmp(mnem, "adcs") == 0) {
        int op = (strcmp(mnem,"adcs")==0) ? OP_ADCS : OP_ADC;
        if (ntok < 4) { fprintf(stderr,"Line %d: adc needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "sbc") == 0 || strcmp(mnem, "sbcs") == 0) {
        int op = (strcmp(mnem,"sbcs")==0) ? OP_SBCS : OP_SBC;
        if (ntok < 4) { fprintf(stderr,"Line %d: sbc needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem, "madd") == 0) {
        if (ntok < 5) { fprintf(stderr,"Line %d: madd needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        PARSE_REG(3, f2, n2, r2); PARSE_REG(4, f3, n3, r3);
        emit("%d %s %s %s %s", OP_MADD, r0, r1, r2, r3);
        return;
    }

    if (strcmp(mnem, "msub") == 0) {
        if (ntok < 5) { fprintf(stderr,"Line %d: msub needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        PARSE_REG(3, f2, n2, r2); PARSE_REG(4, f3, n3, r3);
        emit("%d %s %s %s %s", OP_MSUB, r0, r1, r2, r3);
        return;
    }

    if (strcmp(mnem, "mneg") == 0) {
        if (ntok < 4) { fprintf(stderr,"Line %d: mneg needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", OP_MNEG, r0, r1, r2);
        return;
    }

    if (strcmp(mnem,"smull")==0||strcmp(mnem,"umull")==0||
        strcmp(mnem,"smulh")==0||strcmp(mnem,"umulh")==0) {
        int op = strcmp(mnem,"smull")==0 ? OP_SMULL :
                 strcmp(mnem,"umull")==0 ? OP_UMULL :
                 strcmp(mnem,"smulh")==0 ? OP_SMULH : OP_UMULH;
        if (ntok < 4) { fprintf(stderr,"Line %d: needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem,"smaddl")==0||strcmp(mnem,"umaddl")==0||
        strcmp(mnem,"smsubl")==0||strcmp(mnem,"umsubl")==0) {
        int op = strcmp(mnem,"smaddl")==0 ? OP_SMADDL :
                 strcmp(mnem,"umaddl")==0 ? OP_UMADDL :
                 strcmp(mnem,"smsubl")==0 ? OP_SMSUBL : OP_UMSUBL;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        PARSE_REG(3, f2, n2, r2); PARSE_REG(4, f3, n3, r3);
        emit("%d %s %s %s %s", op, r0, r1, r2, r3);
        return;
    }

    if (strcmp(mnem, "b") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: b needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_B);
        add_fixup(ln, 1, tokens[1]);
        return;
    }

    if (strcmp(mnem, "bl") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: bl needs label\n",*src_lineno); exit(1); }
        int ln = emit("%d FIXUP", OP_BL);
        add_fixup(ln, 1, tokens[1]);
        return;
    }

    if (strcmp(mnem, "br") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: br needs reg\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_BR, r0);
        return;
    }

    if (strcmp(mnem, "blr") == 0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: blr needs reg\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_BLR, r0);
        return;
    }

    {
        int bcond_op = -1;
        const char *lpart = mnem;
        if (mnem[0]=='b' && mnem[1]=='.') lpart = mnem+2;
        else if (mnem[0]=='b' && strlen(mnem)==3) lpart = mnem+1;
        else lpart = NULL;

        if (lpart) {
            if (strcmp(lpart,"eq")==0) bcond_op=OP_BEQ;
            else if (strcmp(lpart,"ne")==0) bcond_op=OP_BNE;
            else if (strcmp(lpart,"lt")==0) bcond_op=OP_BLT;
            else if (strcmp(lpart,"le")==0) bcond_op=OP_BLE;
            else if (strcmp(lpart,"gt")==0) bcond_op=OP_BGT;
            else if (strcmp(lpart,"ge")==0) bcond_op=OP_BGE;
            else if (strcmp(lpart,"lo")==0||strcmp(lpart,"cc")==0) bcond_op=OP_BLO;
            else if (strcmp(lpart,"ls")==0) bcond_op=OP_BLS;
            else if (strcmp(lpart,"hi")==0) bcond_op=OP_BHI;
            else if (strcmp(lpart,"hs")==0||strcmp(lpart,"cs")==0) bcond_op=OP_BHS;
            else if (strcmp(lpart,"mi")==0) bcond_op=OP_BMI;
            else if (strcmp(lpart,"pl")==0) bcond_op=OP_BPL;
            else if (strcmp(lpart,"vs")==0) bcond_op=OP_BVS;
            else if (strcmp(lpart,"vc")==0) bcond_op=OP_BVC;
            else if (strcmp(lpart,"al")==0) bcond_op=OP_BAL;
        }
        if (bcond_op >= 0) {
            if (ntok < 2) { fprintf(stderr,"Line %d: branch needs label\n",*src_lineno); exit(1); }
            int ln = emit("%d FIXUP", bcond_op);
            add_fixup(ln, 1, tokens[1]);
            return;
        }
    }

    if (strcmp(mnem, "cbz") == 0 || strcmp(mnem, "cbnz") == 0) {
        int op = (strcmp(mnem,"cbnz")==0) ? OP_CBNZ : OP_CBZ;
        if (ntok < 3) { fprintf(stderr,"Line %d: cbz/cbnz needs reg+label\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int ln = emit("%d %s FIXUP", op, r0);
        add_fixup(ln, 2, tokens[2]);
        return;
    }

    if (strcmp(mnem, "tbz") == 0 || strcmp(mnem, "tbnz") == 0) {
        int op = (strcmp(mnem,"tbnz")==0) ? OP_TBNZ : OP_TBZ;
        if (ntok < 4) { fprintf(stderr,"Line %d: tbz/tbnz needs reg, bit, label\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        long long bit = parse_imm(tokens[2]);
        int ln = emit("%d %s %lld FIXUP", op, r0, bit);
        add_fixup(ln, 3, tokens[3]);
        return;
    }

    if (strcmp(mnem, "adr") == 0 || strcmp(mnem, "adrp") == 0) {
        int op = (strcmp(mnem,"adrp")==0) ? OP_ADRP : OP_ADR;
        if (ntok < 3) { fprintf(stderr,"Line %d: adr needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        if (is_label_ref(tokens[2])) {
            int ln = emit("%d %s FIXUP", op, r0);
            add_fixup(ln, 2, tokens[2]);
        } else {
            emit("%d %s %lld", op, r0, parse_imm(tokens[2]));
        }
        return;
    }

    if (strcmp(mnem, "svc") == 0) {
        long long imm = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_SVC, imm);
        return;
    }

    if (strcmp(mnem, "hlt") == 0) {
        long long imm = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_HLT, imm);
        return;
    }

    if (strcmp(mnem, "brk") == 0) {
        long long imm = (ntok > 1) ? parse_imm(tokens[1]) : 0;
        emit("%d %lld", OP_BRK, imm);
        return;
    }

    if (strcmp(mnem,"wfe")==0)   { emit("%d", OP_WFE);   return; }
    if (strcmp(mnem,"wfi")==0)   { emit("%d", OP_WFI);   return; }
    if (strcmp(mnem,"sev")==0)   { emit("%d", OP_SEV);   return; }
    if (strcmp(mnem,"sevl")==0)  { emit("%d", OP_SEVL);  return; }
    if (strcmp(mnem,"isb")==0)   { emit("%d", OP_ISB);   return; }
    if (strcmp(mnem,"dsb")==0)   { emit("%d", OP_DSB);   return; }
    if (strcmp(mnem,"dmb")==0)   { emit("%d", OP_DMB);   return; }
    if (strcmp(mnem,"clrex")==0) { emit("%d", OP_CLREX); return; }
    if (strcmp(mnem,"yield")==0) { emit("%d", OP_YIELD); return; }
    if (strcmp(mnem,"eret")==0)  { emit("%d", OP_ERET);  return; }
    if (strcmp(mnem,"drps")==0)  { emit("%d", OP_DRPS);  return; }

    if (strcmp(mnem, "mrs") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: mrs needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s %s", OP_MRS, r0, tokens[2]);
        return;
    }

    if (strcmp(mnem, "msr") == 0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: msr needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_MSR, tokens[1], r1);
        return;
    }

    if (strcmp(mnem,"sys")==0||strcmp(mnem,"sysl")==0) {
        int op = strcmp(mnem,"sysl")==0 ? OP_SYSL : OP_SYS;
        char rest[MAX_LINE]="";
        for (int i=1; i<ntok; i++) {
            strcat(rest, tokens[i]);
            if (i<ntok-1) strcat(rest," ");
        }
        emit("%d %s", op, rest);
        return;
    }

    if (strcmp(mnem,"ic")==0||strcmp(mnem,"dc")==0||
        strcmp(mnem,"at")==0||strcmp(mnem,"tlbi")==0) {
        int op = strcmp(mnem,"ic")==0 ? OP_IC :
                 strcmp(mnem,"dc")==0 ? OP_DC :
                 strcmp(mnem,"at")==0 ? OP_AT : OP_TLBI;
        if (ntok < 2) { fprintf(stderr,"Line %d: needs op type\n",*src_lineno); exit(1); }
        if (ntok >= 3) {
            PARSE_REG(2, f1, n1, r1);
            emit("%d %s %s", op, tokens[1], r1);
        } else {
            emit("%d %s none", op, tokens[1]);
        }
        return;
    }

    if (strcmp(mnem,"clz")==0||strcmp(mnem,"cls")==0||strcmp(mnem,"rbit")==0) {
        int op = strcmp(mnem,"clz")==0 ? OP_CLZ :
                 strcmp(mnem,"cls")==0 ? OP_CLS : OP_RBIT;
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", op, r0, r1);
        return;
    }

    if (strcmp(mnem,"rev")==0||strcmp(mnem,"rev16")==0||
        strcmp(mnem,"rev32")==0||strcmp(mnem,"rev64")==0) {
        int op = strcmp(mnem,"rev")==0  ? OP_REV :
                 strcmp(mnem,"rev16")==0 ? OP_REV16 :
                 strcmp(mnem,"rev32")==0 ? OP_REV32 : OP_REV64;
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", op, r0, r1);
        return;
    }

    if (strcmp(mnem, "extr") == 0) {
        if (ntok < 5) { fprintf(stderr,"Line %d: extr needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        long long lsb = parse_imm(tokens[4]);
        emit("%d %s %s %s %lld", OP_EXTR, r0, r1, r2, lsb);
        return;
    }

    if (strcmp(mnem,"sbfm")==0||strcmp(mnem,"bfm")==0||strcmp(mnem,"ubfm")==0) {
        int op = strcmp(mnem,"sbfm")==0 ? OP_SBFM :
                 strcmp(mnem,"bfm")==0  ? OP_BFM  : OP_UBFM;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        long long immr=parse_imm(tokens[3]), imms=parse_imm(tokens[4]);
        emit("%d %s %s %lld %lld", op, r0, r1, immr, imms);
        return;
    }

    if (strcmp(mnem,"sbfx")==0||strcmp(mnem,"ubfx")==0) {
        int op = strcmp(mnem,"sbfx")==0 ? OP_SBFX : OP_UBFX;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        long long lsb=parse_imm(tokens[3]), width=parse_imm(tokens[4]);
        emit("%d %s %s %lld %lld", op, r0, r1, lsb, width);
        return;
    }

    if (strcmp(mnem,"sbfiz")==0||strcmp(mnem,"ubfiz")==0) {
        int op = strcmp(mnem,"sbfiz")==0 ? OP_SBFIZ : OP_UBFIZ;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        long long lsb=parse_imm(tokens[3]), width=parse_imm(tokens[4]);
        emit("%d %s %s %lld %lld", op, r0, r1, lsb, width);
        return;
    }

    if (strcmp(mnem,"bfxil")==0||strcmp(mnem,"bfi")==0) {
        int op = strcmp(mnem,"bfxil")==0 ? OP_BFXIL : OP_BFI;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        long long lsb=parse_imm(tokens[3]), width=parse_imm(tokens[4]);
        emit("%d %s %s %lld %lld", op, r0, r1, lsb, width);
        return;
    }

    if (strcmp(mnem,"sxtb")==0||strcmp(mnem,"sxth")==0||strcmp(mnem,"sxtw")==0||
        strcmp(mnem,"uxtb")==0||strcmp(mnem,"uxth")==0) {
        int op = strcmp(mnem,"sxtb")==0 ? OP_SXTB :
                 strcmp(mnem,"sxth")==0 ? OP_SXTH :
                 strcmp(mnem,"sxtw")==0 ? OP_SXTW :
                 strcmp(mnem,"uxtb")==0 ? OP_UXTB : OP_UXTH;
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", op, r0, r1);
        return;
    }

    if (strcmp(mnem,"fmov")==0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: fmov needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t,n1t,r1); emit("%d %s R %s", OP_FMOV, r0, r1);
        } else {
            emit("%d %s I %s", OP_FMOV, r0, tokens[2]);
        }
        return;
    }

    if (strcmp(mnem,"fadd")==0||strcmp(mnem,"fsub")==0||
        strcmp(mnem,"fmul")==0||strcmp(mnem,"fdiv")==0) {
        int op = strcmp(mnem,"fadd")==0 ? OP_FADD :
                 strcmp(mnem,"fsub")==0 ? OP_FSUB :
                 strcmp(mnem,"fmul")==0 ? OP_FMUL : OP_FDIV;
        if (ntok < 4) { fprintf(stderr,"Line %d: needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem,"fabs")==0||strcmp(mnem,"fneg")==0||strcmp(mnem,"fsqrt")==0) {
        int op = strcmp(mnem,"fabs")==0 ? OP_FABS :
                 strcmp(mnem,"fneg")==0 ? OP_FNEG : OP_FSQRT;
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", op, r0, r1);
        return;
    }

    if (strcmp(mnem,"fcmp")==0||strcmp(mnem,"fcmpe")==0) {
        int op = strcmp(mnem,"fcmpe")==0 ? OP_FCMPE : OP_FCMP;
        if (ntok < 3) { fprintf(stderr,"Line %d: fcmp needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t;
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t,n1t,r1); emit("%d %s R %s", op, r0, r1);
        } else {
            emit("%d %s I 0", op, r0);                     
        }
        return;
    }

    if (strcmp(mnem,"fccmp")==0||strcmp(mnem,"fccmpe")==0) {
        int op = strcmp(mnem,"fccmpe")==0 ? OP_FCCMPE : OP_FCCMP;
        if (ntok < 5) { fprintf(stderr,"Line %d: fccmp needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        long long nzcv = parse_imm(tokens[3]);
        emit("%d %s %s %lld %s", op, r0, r1, nzcv, tokens[4]);
        return;
    }

    if (strcmp(mnem,"fcsel")==0) {
        if (ntok < 5) { fprintf(stderr,"Line %d: fcsel needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s %s", OP_FCSEL, r0, r1, r2, tokens[4]);
        return;
    }

    if (strcmp(mnem,"fcvt")==0) {
        if (ntok < 3) { fprintf(stderr,"Line %d: fcvt needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s", OP_FCVT, r0, r1);
        return;
    }

    #define FCVT_VARIANT(name, opcode) \
    if (strcmp(mnem, name)==0) { \
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); } \
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); \
        emit("%d %s %s", opcode, r0, r1); return; \
    }
    FCVT_VARIANT("fcvtas", OP_FCVTAS)
    FCVT_VARIANT("fcvtau", OP_FCVTAU)
    FCVT_VARIANT("fcvtms", OP_FCVTMS)
    FCVT_VARIANT("fcvtmu", OP_FCVTMU)
    FCVT_VARIANT("fcvtns", OP_FCVTNS)
    FCVT_VARIANT("fcvtnu", OP_FCVTNU)
    FCVT_VARIANT("fcvtps", OP_FCVTPS)
    FCVT_VARIANT("fcvtpu", OP_FCVTPU)
    FCVT_VARIANT("fcvtzs", OP_FCVTZS)
    FCVT_VARIANT("fcvtzu", OP_FCVTZU)
    FCVT_VARIANT("scvtf",  OP_SCVTF)
    FCVT_VARIANT("ucvtf",  OP_UCVTF)
    #undef FCVT_VARIANT

    if (strcmp(mnem,"fmadd")==0||strcmp(mnem,"fmsub")==0||
        strcmp(mnem,"fnmadd")==0||strcmp(mnem,"fnmsub")==0) {
        int op = strcmp(mnem,"fmadd")==0  ? OP_FMADD :
                 strcmp(mnem,"fmsub")==0  ? OP_FMSUB :
                 strcmp(mnem,"fnmadd")==0 ? OP_FNMADD : OP_FNMSUB;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        PARSE_REG(3, f2, n2, r2); PARSE_REG(4, f3, n3, r3);
        emit("%d %s %s %s %s", op, r0, r1, r2, r3);
        return;
    }

    if (strcmp(mnem,"fmin")==0||strcmp(mnem,"fmax")==0||
        strcmp(mnem,"fminnm")==0||strcmp(mnem,"fmaxnm")==0) {
        int op = strcmp(mnem,"fmin")==0   ? OP_FMIN :
                 strcmp(mnem,"fmax")==0   ? OP_FMAX :
                 strcmp(mnem,"fminnm")==0 ? OP_FMINNM : OP_FMAXNM;
        if (ntok < 4) { fprintf(stderr,"Line %d: needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem,"csel")==0||strcmp(mnem,"csinc")==0||
        strcmp(mnem,"csinv")==0||strcmp(mnem,"csneg")==0) {
        int op = strcmp(mnem,"csel")==0  ? OP_CSEL  :
                 strcmp(mnem,"csinc")==0 ? OP_CSINC :
                 strcmp(mnem,"csinv")==0 ? OP_CSINV : OP_CSNEG;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        emit("%d %s %s %s %s", op, r0, r1, r2, tokens[4]);
        return;
    }

    if (strcmp(mnem,"cset")==0||strcmp(mnem,"csetm")==0) {
        int op = strcmp(mnem,"csetm")==0 ? OP_CSETM : OP_CSET;
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s %s", op, r0, tokens[2]);
        return;
    }

    if (strcmp(mnem,"cinc")==0||strcmp(mnem,"cinv")==0||strcmp(mnem,"cneg")==0) {
        int op = strcmp(mnem,"cinc")==0 ? OP_CINC :
                 strcmp(mnem,"cinv")==0 ? OP_CINV : OP_CNEG;
        if (ntok < 4) { fprintf(stderr,"Line %d: needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        emit("%d %s %s %s", op, r0, r1, tokens[3]);
        return;
    }

    if (strcmp(mnem,"ccmp")==0||strcmp(mnem,"ccmn")==0) {
        int op = strcmp(mnem,"ccmn")==0 ? OP_CCMN : OP_CCMP;
        if (ntok < 5) { fprintf(stderr,"Line %d: ccmp needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        int f1t, n1t; long long nzcv; char cond[32];
        if (parse_register(tokens[2], &f1t, &n1t)) {
            reg_str(f1t,n1t,r1);
            nzcv = parse_imm(tokens[3]);
            strncpy(cond, tokens[4], 31);
            emit("%d %s R %s %lld %s", op, r0, r1, nzcv, cond);
        } else {
            nzcv = parse_imm(tokens[3]);
            strncpy(cond, tokens[4], 31);
            emit("%d %s I %lld %lld %s", op, r0, parse_imm(tokens[2]), nzcv, cond);
        }
        return;
    }

    #define ATOMIC1(name, opcode) \
    if (strcmp(mnem, name)==0) { \
        if (ntok < 3) { fprintf(stderr,"Line %d: needs 2 operands\n",*src_lineno); exit(1); } \
        PARSE_REG(1, f0, n0, r0); \
        char tmp2b[TOKEN_MAX]; strncpy(tmp2b,tokens[2],TOKEN_MAX-1); \
        int tl2=(int)strlen(tmp2b); \
        if(tmp2b[0]=='['){memmove(tmp2b,tmp2b+1,tl2--);} \
        if(tl2>0&&tmp2b[tl2-1]==']'){tmp2b[tl2-1]='\0';tl2--;} \
        strip(tmp2b); \
        int fb2,nb2; if(!parse_register(tmp2b,&fb2,&nb2)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);} \
        reg_str(fb2,nb2,r1); emit("%d %s %s", opcode, r0, r1); return; \
    }
    ATOMIC1("ldar",  OP_LDAR)
    ATOMIC1("ldarb", OP_LDARB)
    ATOMIC1("ldarh", OP_LDARH)
    ATOMIC1("ldaxr", OP_LDAXR)
    ATOMIC1("ldaxrb",OP_LDAXRB)
    ATOMIC1("ldaxrh",OP_LDAXRH)
    ATOMIC1("ldxr",  OP_LDXR)
    ATOMIC1("ldxrb", OP_LDXRB)
    ATOMIC1("ldxrh", OP_LDXRH)
    ATOMIC1("stlr",  OP_STLR)
    ATOMIC1("stlrb", OP_STLRB)
    ATOMIC1("stlrh", OP_STLRH)
    #undef ATOMIC1

    #define ATOMIC2(name, opcode) \
    if (strcmp(mnem, name)==0) { \
        if (ntok < 4) { fprintf(stderr,"Line %d: needs 3 operands\n",*src_lineno); exit(1); } \
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); \
        char tmp2b[TOKEN_MAX]; strncpy(tmp2b,tokens[3],TOKEN_MAX-1); \
        int tl2=(int)strlen(tmp2b); \
        if(tmp2b[0]=='['){memmove(tmp2b,tmp2b+1,tl2--);} \
        if(tl2>0&&tmp2b[tl2-1]==']'){tmp2b[tl2-1]='\0';tl2--;} \
        strip(tmp2b); \
        int fb2,nb2; if(!parse_register(tmp2b,&fb2,&nb2)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);} \
        reg_str(fb2,nb2,r2); emit("%d %s %s %s", opcode, r0, r1, r2); return; \
    }
    ATOMIC2("stlxr",  OP_STLXR)
    ATOMIC2("stlxrb", OP_STLXRB)
    ATOMIC2("stlxrh", OP_STLXRH)
    ATOMIC2("stxr",   OP_STXR)
    ATOMIC2("stxrb",  OP_STXRB)
    ATOMIC2("stxrh",  OP_STXRH)
    #undef ATOMIC2

    if (strcmp(mnem,"ldxp")==0||strcmp(mnem,"ldaxp")==0) {
        int op = strcmp(mnem,"ldaxp")==0 ? OP_LDAXP : OP_LDXP;
        if (ntok < 4) { fprintf(stderr,"Line %d: needs 3 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1);
        char tmp2b[TOKEN_MAX]; strncpy(tmp2b,tokens[3],TOKEN_MAX-1);
        int tl2=(int)strlen(tmp2b);
        if(tmp2b[0]=='['){memmove(tmp2b,tmp2b+1,tl2--);}
        if(tl2>0&&tmp2b[tl2-1]==']'){tmp2b[tl2-1]='\0';tl2--;}
        strip(tmp2b);
        int fb2,nb2; if(!parse_register(tmp2b,&fb2,&nb2)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb2,nb2,r2); emit("%d %s %s %s", op, r0, r1, r2);
        return;
    }

    if (strcmp(mnem,"stxp")==0||strcmp(mnem,"stlxp")==0) {
        int op = strcmp(mnem,"stlxp")==0 ? OP_STLXP : OP_STXP;
        if (ntok < 5) { fprintf(stderr,"Line %d: needs 4 operands\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0); PARSE_REG(2, f1, n1, r1); PARSE_REG(3, f2, n2, r2);
        char tmp2b[TOKEN_MAX]; strncpy(tmp2b,tokens[4],TOKEN_MAX-1);
        int tl2=(int)strlen(tmp2b);
        if(tmp2b[0]=='['){memmove(tmp2b,tmp2b+1,tl2--);}
        if(tl2>0&&tmp2b[tl2-1]==']'){tmp2b[tl2-1]='\0';tl2--;}
        strip(tmp2b);
        int fb2,nb2; if(!parse_register(tmp2b,&fb2,&nb2)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb2,nb2,r3); emit("%d %s %s %s %s", op, r0, r1, r2, r3);
        return;
    }

    if (strcmp(mnem,"prfm")==0||strcmp(mnem,"prfum")==0) {
        int op = strcmp(mnem,"prfum")==0 ? OP_PRFUM : OP_PRFM;
        if (ntok < 3) { fprintf(stderr,"Line %d: prfm needs 2 operands\n",*src_lineno); exit(1); }
        char tmp2b[TOKEN_MAX]; strncpy(tmp2b,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2b; int tl2=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl2--);}
        if(tl2>0&&br[tl2-1]==']'){br[tl2-1]='\0';tl2--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb2,nb2; if(!parse_register(br,&fb2,&nb2)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb2,nb2,r1); emit("%d %s %s %lld", op, tokens[1], r1, offset);
        return;
    }

    if (strcmp(mnem,"ldraa")==0||strcmp(mnem,"ldrab")==0) {
        int op = strcmp(mnem,"ldrab")==0 ? OP_LDRAB : OP_LDRAA;
        PARSE_REG(1, f0, n0, r0);
        char tmp2b[TOKEN_MAX]; strncpy(tmp2b,tokens[2],TOKEN_MAX-1);
        long long offset=0; char *br=tmp2b; int tl2=(int)strlen(br);
        if(br[0]=='['){memmove(br,br+1,tl2--);}
        if(tl2>0&&br[tl2-1]==']'){br[tl2-1]='\0';tl2--;}
        if(tl2>0&&br[tl2-1]=='!'){br[tl2-1]='\0';tl2--;}
        char *comma=strchr(br,','); if(comma){*comma='\0';offset=parse_imm(comma+1);}
        strip(br); if(ntok>=4&&comma==NULL)offset=parse_imm(tokens[3]);
        int fb2,nb2; if(!parse_register(br,&fb2,&nb2)){fprintf(stderr,"Line %d: bad reg\n",*src_lineno);exit(1);}
        reg_str(fb2,nb2,r1); emit("%d %s %s %lld", op, r0, r1, offset);
        return;
    }

    if (strcmp(mnem,"push")==0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: push needs reg\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_PUSH, r0);
        return;
    }
    if (strcmp(mnem,"pop")==0) {
        if (ntok < 2) { fprintf(stderr,"Line %d: pop needs reg\n",*src_lineno); exit(1); }
        PARSE_REG(1, f0, n0, r0);
        emit("%d %s", OP_POP, r0);
        return;
    }

    fprintf(stderr, "Line %d: unknown mnemonic '%s'\n", *src_lineno, tokens[0]);
    exit(1);

    #undef PARSE_REG
}

static void apply_fixups(void) {
    for (int i = 0; i < fixup_count; i++) {
        int line_idx = fixups[i].bytecode_line;
        const char *lname = fixups[i].label_name;
        int target = find_label(lname);
        if (target < 0) {
            fprintf(stderr, "Error: undefined label '%s'\n", lname);
            exit(1);
        }

        char *pos = strstr(out_lines[line_idx].line, "FIXUP");
        if (!pos) {
            fprintf(stderr, "Internal error: FIXUP marker missing in line %d\n", line_idx);
            exit(1);
        }
        char before[MAX_LINE], after[MAX_LINE];
        int blen = (int)(pos - out_lines[line_idx].line);
        strncpy(before, out_lines[line_idx].line, blen);
        before[blen] = '\0';
        strcpy(after, pos + 5);                   
        char newline[MAX_LINE];
        snprintf(newline, MAX_LINE, "%s%d%s", before, target, after);
        strncpy(out_lines[line_idx].line, newline, MAX_LINE-1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "WebAssembler Compiler\n");
        fprintf(stderr, "Usage: %s <input.iwa> <output.wassm>\n", argv[0]);
        fprintf(stderr, "  Compiles .iwa assembly to .wassm bytecode\n");
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    if (!fin) {
        fprintf(stderr, "Error: cannot open '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    char line[MAX_LINE];
    int lineno = 0;

    while (fgets(line, MAX_LINE, fin)) {
        lineno++;
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
        compile_line(line, &lineno);
    }
    fclose(fin);

    apply_fixups();

    FILE *fout = fopen(argv[2], "w");
    if (!fout) {
        fprintf(stderr, "Error: cannot open '%s' for writing: %s\n", argv[2], strerror(errno));
        return 1;
    }

    fprintf(fout, "; WebAssembler Raw Bytecode\n");
    fprintf(fout, "; Not recommended to modify\n");
    fprintf(fout, "; Contents of %s\n", argv[1]);
    fprintf(fout, "; Lines ≈ %d\n", out_count);
    fprintf(fout, ";;;;;\n");

    for (int i = 0; i < out_count; i++) {
        fprintf(fout, "%s\n", out_lines[i].line);
    }

    fclose(fout);
    fprintf(stdout, "Compiled %d instruction(s) → %s\n", out_count, argv[2]);
    return 0;
}

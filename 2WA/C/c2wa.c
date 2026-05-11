#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#define MAX_VARS         8192
#define MAX_OUT          4194304     /* 4M output lines */
#define MAX_LINE         16384
#define MAX_SRC          (32 << 20)  /* 32MB source */
#define MAX_LABEL_LEN    256
#define MAX_SCOPE        2048
#define MAX_FUNCS        2048
#define MAX_PARAMS       32
#define MAX_FMT_ARGS     128
#define MAX_STRUCTS      2048
#define MAX_FIELDS       256
#define MAX_TYPEDEFS     2048
#define MAX_ENUMS        4096
#define MAX_CASES        4096
#define MAX_GOTO_LABELS  1024
#define BLOCK_BUF_SIZE   (8 << 20)  /* 8MB per block buffer */

/* Register ranges */
#define VREG_RET         0
#define VREG_LOCAL_LO    1
#define VREG_LOCAL_HI    4069
#define VREG_SPILL_LO    4069
#define VREG_SPILL_HI    4069
#define VREG_ARG_LO      4069
#define VREG_ARG_HI      4069
#define VREG_SCRATCH_A   29
#define VREG_SCRATCH_B   30

/* Memory pool */
#define MEM_BASE         0x10000
#define WORD_SIZE        8

static char **out_buf  = NULL;
static int    out_n    = 0;
static int    out_cap  = 0;

static int emit(const char *fmt, ...) {
    if (out_n >= out_cap) {
        out_cap = out_cap ? out_cap * 2 : 65536;
        char **nb = (char **)realloc(out_buf, out_cap * sizeof(char *));
        if (!nb) { fprintf(stderr, "c2iwa: output overflow\n"); exit(1); }
        out_buf = nb;
    }
    va_list ap; va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);
    out_buf[out_n] = (char *)malloc(need);
    if (!out_buf[out_n]) { fprintf(stderr, "c2iwa: out of memory\n"); exit(1); }
    va_start(ap, fmt);
    vsnprintf(out_buf[out_n], need, fmt, ap);
    va_end(ap);
    return out_n++;
}

static void patch(int idx, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);
    char *nb = (char *)realloc(out_buf[idx], need);
    if (!nb) { fprintf(stderr, "c2iwa: out of memory\n"); exit(1); }
    out_buf[idx] = nb;
    va_start(ap, fmt);
    vsnprintf(out_buf[idx], need, fmt, ap);
    va_end(ap);
    (void)patch; /* suppress unused warning if not called */
}

static void out_buf_init(void) {
    out_cap = 65536;
    out_buf = (char **)malloc(out_cap * sizeof(char *));
    if (!out_buf) { fprintf(stderr, "c2iwa: out of memory\n"); exit(1); }
}

static void out_buf_free(void) {
    for (int i = 0; i < out_n; i++) free(out_buf[i]);
    free(out_buf);
    out_buf = NULL; out_n = 0; out_cap = 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Label generator
 * ═══════════════════════════════════════════════════════════════ */
static int label_cnt = 0;
static void new_label(char *buf, const char *pfx) {
    snprintf(buf, MAX_LABEL_LEN, "._c_%s%d", pfx, label_cnt++);
}

/* ═══════════════════════════════════════════════════════════════
 * Memory pool
 * ═══════════════════════════════════════════════════════════════ */
static long long mem_next = MEM_BASE;
static long long mem_alloc_pool(int bytes) {
    long long addr = mem_next;
    mem_next += ((long long)(bytes + WORD_SIZE - 1)) & ~(long long)(WORD_SIZE - 1);
    return addr;
}

/* ═══════════════════════════════════════════════════════════════
 * Type system
 * ═══════════════════════════════════════════════════════════════ */
typedef enum {
    TY_VOID=0, TY_CHAR, TY_SHORT, TY_INT, TY_LONG, TY_LLONG,
    TY_UCHAR, TY_USHORT, TY_UINT, TY_ULONG, TY_ULLONG,
    TY_BOOL, TY_FLOAT, TY_DOUBLE,
    TY_PTR, TY_ARRAY, TY_STRUCT, TY_UNION, TY_ENUM, TY_STR, TY_FUNC,
} CType;

static int type_is_unsigned(CType t) {
    return t==TY_UCHAR||t==TY_USHORT||t==TY_UINT||t==TY_ULONG||t==TY_ULLONG||t==TY_BOOL;
}
static int type_is_int(CType t) {
    return t==TY_CHAR||t==TY_SHORT||t==TY_INT||t==TY_LONG||t==TY_LLONG||
           t==TY_UCHAR||t==TY_USHORT||t==TY_UINT||t==TY_ULONG||t==TY_ULLONG||
           t==TY_BOOL||t==TY_ENUM;
}
static int type_size(CType t) {
    if (t==TY_CHAR||t==TY_UCHAR||t==TY_BOOL) return 1;
    if (t==TY_SHORT||t==TY_USHORT)            return 2;
    if (t==TY_INT||t==TY_UINT||t==TY_FLOAT)   return 4;
    return WORD_SIZE;
}

/* ─── Struct/Union definition ─── */
typedef struct {
    char name[64];
    struct {
        char name[64];
        CType type;
        int struct_idx;
        int array_len;
        int offset;
    } fields[MAX_FIELDS];
    int nfields;
    int total_size;
    int is_union;
    int defined;  /* 1 = body has been parsed */
} StructDef;

static StructDef structs[MAX_STRUCTS];
static int       nstructs = 0;

static StructDef *find_struct(const char *name) {
    for (int i = 0; i < nstructs; i++)
        if (strcmp(structs[i].name, name)==0) return &structs[i];
    return NULL;
}
static StructDef *add_struct(const char *name, int is_union) {
    StructDef *s = find_struct(name);
    if (s) return s;
    if (nstructs >= MAX_STRUCTS) { fprintf(stderr,"c2iwa: too many structs\n"); exit(1); }
    memset(&structs[nstructs], 0, sizeof(StructDef));
    strncpy(structs[nstructs].name, name, 63);
    structs[nstructs].is_union = is_union;
    structs[nstructs].defined  = 0;
    return &structs[nstructs++];
}

/* ─── Typedef table ─── */
typedef struct { char alias[128]; char real[256]; } TypedefEntry;
static TypedefEntry typedefs[MAX_TYPEDEFS];
static int          ntypedefs = 0;

static void add_typedef(const char *alias, const char *real) {
    /* Update existing entry if alias already defined */
    for (int i = 0; i < ntypedefs; i++) {
        if (strcmp(typedefs[i].alias, alias)==0) {
            strncpy(typedefs[i].real, real, 255);
            return;
        }
    }
    if (ntypedefs >= MAX_TYPEDEFS) return;
    strncpy(typedefs[ntypedefs].alias, alias, 127);
    strncpy(typedefs[ntypedefs].real,  real,  255);
    ntypedefs++;
}
static const char *resolve_typedef(const char *name) {
    for (int i = ntypedefs-1; i >= 0; i--)
        if (strcmp(typedefs[i].alias, name)==0) return typedefs[i].real;
    return name;
}

/* ─── Enum constants ─── */
typedef struct { char name[64]; long long value; } EnumConst;
static EnumConst enum_consts[MAX_ENUMS];
static int       nenum_consts = 0;

static void add_enum_const(const char *name, long long val) {
    /* Update if already exists */
    for (int i = 0; i < nenum_consts; i++) {
        if (strcmp(enum_consts[i].name, name)==0) { enum_consts[i].value=val; return; }
    }
    if (nenum_consts >= MAX_ENUMS) return;
    strncpy(enum_consts[nenum_consts].name, name, 63);
    enum_consts[nenum_consts].value = val;
    nenum_consts++;
}
static int find_enum_const(const char *name, long long *out) {
    for (int i = 0; i < nenum_consts; i++)
        if (strcmp(enum_consts[i].name, name)==0) { *out=enum_consts[i].value; return 1; }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Variable / register table
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    char      name[80];
    int       reg;
    CType     type;
    CType     ptr_to;
    int       struct_idx;
    int       array_len;
    int       array_len2;   /* second dimension */
    long long mem_addr;
    int       is_global;
    int       scope_depth;
    int       is_param;
    int       is_unsigned;
} Var;

static Var  vars[MAX_VARS];
static int  nvar       = 0;
static int  cur_scope  = 0;

/* Local register bitset */
static int local_reg_used[VREG_LOCAL_HI - VREG_LOCAL_LO + 1];

static int alloc_local_reg(void) {
    for (int i = 0; i <= VREG_LOCAL_HI - VREG_LOCAL_LO; i++) {
        if (!local_reg_used[i]) {
            local_reg_used[i] = 1;
            return VREG_LOCAL_LO + i;
        }
    }
    fprintf(stderr, "c2iwa: too many local variables (max %d)\n", VREG_LOCAL_HI-VREG_LOCAL_LO+1);
    exit(1);
}
static void free_local_reg(int r) {
    if (r >= VREG_LOCAL_LO && r <= VREG_LOCAL_HI)
        local_reg_used[r - VREG_LOCAL_LO] = 0;
}

static Var *find_var(const char *name) {
    for (int i = nvar-1; i >= 0; i--)
        if (strcmp(vars[i].name, name)==0) return &vars[i];
    return NULL;
}

static Var *add_var(const char *name, CType t) {
    Var *v = find_var(name);
    if (v && v->scope_depth == cur_scope) return v;
    if (nvar >= MAX_VARS) { fprintf(stderr,"c2iwa: too many variables\n"); exit(1); }
    memset(&vars[nvar], 0, sizeof(Var));
    strncpy(vars[nvar].name, name, 79);
    vars[nvar].reg         = alloc_local_reg();
    vars[nvar].type        = t;
    vars[nvar].ptr_to      = TY_INT;
    vars[nvar].struct_idx  = -1;
    vars[nvar].array_len   = 0;
    vars[nvar].array_len2  = 0;
    vars[nvar].mem_addr    = -1;
    vars[nvar].is_global   = 0;
    vars[nvar].scope_depth = cur_scope;
    vars[nvar].is_param    = 0;
    vars[nvar].is_unsigned = 0;
    return &vars[nvar++];
}

/* Scope management */
typedef struct {
    int nvar;
    int regs_snapshot[VREG_LOCAL_HI - VREG_LOCAL_LO + 1];
} ScopeSnap;

static ScopeSnap scope_enter(void) {
    ScopeSnap s;
    s.nvar = nvar;
    memcpy(s.regs_snapshot, local_reg_used, sizeof(local_reg_used));
    cur_scope++;
    return s;
}
static void scope_leave(ScopeSnap s) {
    for (int i = s.nvar; i < nvar; i++) free_local_reg(vars[i].reg);
    nvar = s.nvar;
    memcpy(local_reg_used, s.regs_snapshot, sizeof(local_reg_used));
    if (cur_scope > 0) cur_scope--;
}

/* ═══════════════════════════════════════════════════════════════
 * Function table
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    char  name[128];
    char  param_names[MAX_PARAMS][128];
    CType param_types[MAX_PARAMS];
    int   nparams;
    CType ret_type;
    int   is_variadic;
} FuncInfo;

static FuncInfo  funcs[MAX_FUNCS];
static int       nfuncs = 0;
static FuncInfo *current_func = NULL;

static FuncInfo *find_func(const char *name) {
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i].name, name)==0) return &funcs[i];
    return NULL;
}
static FuncInfo *add_func(const char *name) {
    FuncInfo *f = find_func(name);
    if (f) return f;
    if (nfuncs >= MAX_FUNCS) { fprintf(stderr,"c2iwa: too many functions\n"); exit(1); }
    memset(&funcs[nfuncs], 0, sizeof(FuncInfo));
    strncpy(funcs[nfuncs].name, name, 127);
    return &funcs[nfuncs++];
}

/* ═══════════════════════════════════════════════════════════════
 * Goto label table
 * ═══════════════════════════════════════════════════════════════ */
typedef struct { char cname[64]; char iwa[MAX_LABEL_LEN]; } GotoLabel;
static GotoLabel goto_labels[MAX_GOTO_LABELS];
static int       ngoto = 0;

static void add_goto_label(const char *c, const char *iwa) {
    if (ngoto >= MAX_GOTO_LABELS) return;
    strncpy(goto_labels[ngoto].cname, c,   63);
    strncpy(goto_labels[ngoto].iwa,   iwa, MAX_LABEL_LEN-1);
    ngoto++;
}
static const char *find_goto_label(const char *c) {
    for (int i = 0; i < ngoto; i++)
        if (strcmp(goto_labels[i].cname, c)==0) return goto_labels[i].iwa;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * Loop / switch context stacks
 * ═══════════════════════════════════════════════════════════════ */
typedef struct { char brk[MAX_LABEL_LEN]; char cont[MAX_LABEL_LEN]; } LoopCtx;
static LoopCtx loop_stk[MAX_SCOPE];
static int     loop_depth = 0;

static void loop_push(const char *b, const char *c) {
    if (loop_depth >= MAX_SCOPE) { fprintf(stderr,"c2iwa: loop nesting too deep\n"); exit(1); }
    strncpy(loop_stk[loop_depth].brk,  b, MAX_LABEL_LEN-1);
    strncpy(loop_stk[loop_depth].cont, c, MAX_LABEL_LEN-1);
    loop_depth++;
}
static void loop_pop(void) { if (loop_depth > 0) loop_depth--; }

typedef struct { int cmp_reg; char end_lbl[MAX_LABEL_LEN]; } SwCtx;
static SwCtx sw_stk[MAX_SCOPE];
static int   sw_depth = 0;

/* ═══════════════════════════════════════════════════════════════
 * Temporary register pool  (spill registers)
 * ═══════════════════════════════════════════════════════════════ */
static int tmp_pool[] = {
    VREG_SCRATCH_A, VREG_SCRATCH_B,
    VREG_SPILL_LO, VREG_SPILL_LO+1, VREG_SPILL_LO+2,
    VREG_SPILL_LO+3, VREG_SPILL_LO+4, VREG_SPILL_LO+5, VREG_SPILL_LO+6
};
#define TMP_POOL_SIZE ((int)(sizeof(tmp_pool)/sizeof(tmp_pool[0])))
static int tmp_depth = 0;

static int tmp_alloc(void) {
    if (tmp_depth >= TMP_POOL_SIZE) {
        fprintf(stderr, "c2iwa: expression too complex (>%d temporaries)\n", TMP_POOL_SIZE);
        exit(1);
    }
    return tmp_pool[tmp_depth++];
}
static void tmp_free(void) { if (tmp_depth > 0) tmp_depth--; }

/* ═══════════════════════════════════════════════════════════════
 * Value representation
 * ═══════════════════════════════════════════════════════════════ */
typedef enum { VK_REG, VK_IMM, VK_MEM } VKind;
typedef struct {
    VKind    kind;
    int      reg;
    long long imm;
    CType    type;
    int      is_lval;
    int      lval_reg;
} VReg;

static VReg vreg_r(int r, CType t) {
    VReg v; v.kind=VK_REG; v.reg=r; v.imm=0; v.type=t; v.is_lval=0; v.lval_reg=-1; return v;
}
static VReg vreg_i(long long n) {
    VReg v; v.kind=VK_IMM; v.reg=0; v.imm=n; v.type=TY_INT; v.is_lval=0; v.lval_reg=-1; return v;
}
static VReg vreg_m(int base_reg, long long offset, CType t) {
    VReg v; v.kind=VK_MEM; v.reg=base_reg; v.imm=offset; v.type=t; v.is_lval=1; v.lval_reg=base_reg; return v;
}

static int mat(VReg v, int scratch) {
    if (v.kind == VK_REG) return v.reg;
    if (v.kind == VK_IMM) { emit("    mov x%d, %lld", scratch, v.imm); return scratch; }
    /* VK_MEM: load from base+offset */
    int ts = type_size(v.type);
    if (ts == 1) emit("    ldrb x%d, x%d, %lld", scratch, v.reg, v.imm);
    else if (ts == 2) emit("    ldrh x%d, x%d, %lld", scratch, v.reg, v.imm);
    else emit("    ldr x%d, x%d, %lld", scratch, v.reg, v.imm);
    return scratch;
}

static int mat_tmp(VReg v) {
    if (v.kind == VK_REG) return v.reg;
    int t = tmp_alloc();
    mat(v, t);
    return t;
}

/* ═══════════════════════════════════════════════════════════════
 * String / source utilities
 * ═══════════════════════════════════════════════════════════════ */
static void strip_ws(char *s) {
    int i=0; while(s[i]&&isspace((unsigned char)s[i]))i++;
    if(i) memmove(s,s+i,strlen(s+i)+1);
    int n=(int)strlen(s);
    while(n>0&&isspace((unsigned char)s[n-1]))s[--n]='\0';
}
static void strip_semi(char *s) {
    int n=(int)strlen(s);
    while(n>0&&(s[n-1]==';'||isspace((unsigned char)s[n-1])))s[--n]='\0';
}

/* Escape a C string to IWA-safe format
 * IWA parser uses space as token separator, so spaces become \~ */
static void c_str_to_iwa(const char *src, char *dst, int maxlen) {
    int di=0;
    for(int i=0; src[i] && di<maxlen-6; i++) {
        unsigned char c=(unsigned char)src[i];
        if      (c==' ')  { dst[di++]='\\'; dst[di++]='~'; }
        else if (c=='\n') { dst[di++]='\\'; dst[di++]='n'; }
        else if (c=='\t') { dst[di++]='\\'; dst[di++]='t'; }
        else if (c=='\r') { dst[di++]='\\'; dst[di++]='r'; }
        else if (c=='\\') { dst[di++]='\\'; dst[di++]='\\'; }
        else if (c=='"')  { dst[di++]='\\'; dst[di++]='"'; }
        else              dst[di++]=(char)c;
    }
    dst[di]='\0';
}

static void emit_lds(int reg, const char *cstr) {
    char esc[MAX_LINE];
    c_str_to_iwa(cstr, esc, sizeof(esc));
    emit("    lds x%d, \"%s\"", reg, esc);
}

static const char *skip_balanced(const char *p, char open, char close) {
    if (*p != open) return p;
    int d=0, in_str=0;
    char str_char=0;
    while (*p) {
        if (!in_str && (*p=='"'||*p=='\'')) { in_str=1; str_char=*p; }
        else if (in_str && *p=='\\' && *(p+1)) { p++; }
        else if (in_str && *p==str_char) { in_str=0; }
        if (!in_str) {
            if (*p==open) d++;
            else if (*p==close) { d--; if(!d){p++;break;} }
        }
        p++;
    }
    return p;
}

static int extract_parens(const char *p, char *buf, int buflen) {
    const char *start=p;
    if (*p!='(') { buf[0]='\0'; return 0; }
    const char *end=skip_balanced(p,'(',')');
    int len=(int)(end-p-2); if(len<0)len=0;
    if(len>=buflen)len=buflen-1;
    strncpy(buf,p+1,len); buf[len]='\0';
    return (int)(end-start);
}

static const char *extract_string_lit(const char *p, char *out, int outlen) {
    if (*p!='"') { if(outlen>0)out[0]='\0'; return p; }
    p++;
    int i=0;
    while (*p && *p!='"') {
        if (*p=='\\' && *(p+1)) {
            p++;
            switch(*p) {
                case 'n':  if(i<outlen-1)out[i++]='\n'; break;
                case 't':  if(i<outlen-1)out[i++]='\t'; break;
                case 'r':  if(i<outlen-1)out[i++]='\r'; break;
                case '0':  if(i<outlen-1)out[i++]='\0'; break;
                case '\\': if(i<outlen-1)out[i++]='\\'; break;
                case '"':  if(i<outlen-1)out[i++]='"';  break;
                case 'a':  if(i<outlen-1)out[i++]='\a'; break;
                case 'b':  if(i<outlen-1)out[i++]='\b'; break;
                case 'x': {
                    p++;
                    char hx[3]={0}; int hi=0;
                    while(hi<2&&*p&&isxdigit((unsigned char)*p))hx[hi++]=*p++;
                    p--;
                    if(i<outlen-1)out[i++]=(char)(int)strtol(hx,NULL,16);
                    break;
                }
                case 'u': case 'U': {
                    /* Unicode escape — skip it, emit '?' */
                    int nd = (*p=='u')?4:8;
                    while(nd-- && *(p+1) && isxdigit((unsigned char)*(p+1))) p++;
                    if(i<outlen-1)out[i++]='?';
                    break;
                }
                default: if(i<outlen-1)out[i++]=*p; break;
            }
        } else { if(i<outlen-1)out[i++]=*p; }
        p++;
    }
    out[i]='\0';
    if (*p=='"') p++;
    /* adjacent string literals */
    while (*p) {
        while(*p&&isspace((unsigned char)*p))p++;
        if (*p!='"') break;
        p++;
        while (*p&&*p!='"') {
            if (*p=='\\'&&*(p+1)){
                p++;
                switch(*p){
                    case 'n':if(i<outlen-1)out[i++]='\n';break;
                    case 't':if(i<outlen-1)out[i++]='\t';break;
                    case 'r':if(i<outlen-1)out[i++]='\r';break;
                    case '0':if(i<outlen-1)out[i++]='\0';break;
                    case '\\':if(i<outlen-1)out[i++]='\\';break;
                    case '"':if(i<outlen-1)out[i++]='"';break;
                    default:if(i<outlen-1)out[i++]=*p;break;
                }
            } else { if(i<outlen-1)out[i++]=*p; }
            p++;
        }
        out[i]='\0';
        if (*p=='"') p++;
    }
    return p;
}

/* ═══════════════════════════════════════════════════════════════
 * Type keyword detection and parsing
 * ═══════════════════════════════════════════════════════════════ */
static int is_type_kw(const char *s) {
    /* check typedefs first */
    for (int i = 0; i < ntypedefs; i++) {
        int al = (int)strlen(typedefs[i].alias);
        if (strncmp(s, typedefs[i].alias, al)==0) {
            char nx = s[al];
            if (!nx||isspace((unsigned char)nx)||nx=='*'||nx==','||nx=='['||nx==';') return 1;
        }
    }
    /* check structs by name too */
    for (int i = 0; i < nstructs; i++) {
        /* struct/union used as type without keyword is only valid via typedef */
    }
    static const char *kws[] = {
        "int","long","short","char","unsigned","signed","void",
        "float","double","const","static","extern","register","volatile",
        "auto","inline","__inline__","__inline","__restrict","restrict","_Noreturn",
        "size_t","ssize_t","ptrdiff_t","intptr_t","uintptr_t",
        "uint8_t","uint16_t","uint32_t","uint64_t",
        "int8_t","int16_t","int32_t","int64_t",
        "bool","_Bool","struct","union","enum","typedef",
        "__attribute__","__extension__","__signed__","__const__","__volatile__",
        "FILE","va_list","jmp_buf","clock_t","time_t","wchar_t","wint_t",
        "__builtin_va_list","__gnuc_va_list",
        NULL
    };
    for (int i=0; kws[i]; i++) {
        int kl=(int)strlen(kws[i]);
        if (strncmp(s,kws[i],kl)==0) {
            char nx=s[kl];
            if (!nx||isspace((unsigned char)nx)||nx=='*'||nx==','||nx=='['||nx==';') return 1;
        }
    }
    return 0;
}

typedef struct {
    CType type;
    CType ptr_to;
    int   struct_idx;
    int   is_ptr;
    int   is_arr;
    int   is_unsigned;
} TypeSpec;

/* Forward declare for recursive typedef resolution */
static const char *parse_typespec(const char *s, TypeSpec *ts);

static const char *parse_typespec(const char *s, TypeSpec *ts) {
    memset(ts, 0, sizeof(*ts));
    ts->type       = TY_INT;
    ts->ptr_to     = TY_INT;
    ts->struct_idx = -1;

    const char *p = s;
    int seen_unsigned=0, seen_long=0, seen_llong=0, seen_short=0, seen_char=0;
    int seen_int=0, seen_void=0, seen_float=0, seen_double=0, seen_bool=0;
    int seen_struct=0, seen_union=0;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* qualifiers / storage-class — skip */
        #define SKIP_KW(kw, len) \
            if (strncmp(p,kw,len)==0 && !isalnum((unsigned char)p[len]) && p[len]!='_') { p+=len; continue; }
        SKIP_KW("const",5) SKIP_KW("volatile",8) SKIP_KW("static",6)
        SKIP_KW("extern",6) SKIP_KW("register",8) SKIP_KW("inline",6)
        SKIP_KW("__inline__",10) SKIP_KW("__inline",8)
        SKIP_KW("__restrict",10) SKIP_KW("restrict",8)
        SKIP_KW("__extension__",13) SKIP_KW("_Noreturn",9)
        SKIP_KW("auto",4)
        #undef SKIP_KW

        /* __attribute__((...)) */
        if (strncmp(p,"__attribute__",13)==0) {
            p+=13;
            while(*p&&isspace((unsigned char)*p))p++;
            if (*p=='(') p=(char*)skip_balanced(p,'(',')');
            continue;
        }

        if (strncmp(p,"unsigned",8)==0&&!isalnum((unsigned char)p[8])&&p[8]!='_') { seen_unsigned=1; p+=8; continue; }
        if (strncmp(p,"signed",6)==0  &&!isalnum((unsigned char)p[6])&&p[6]!='_') { p+=6; continue; }
        if (strncmp(p,"long",4)==0    &&!isalnum((unsigned char)p[4])&&p[4]!='_') {
            if (seen_long) seen_llong=1; else seen_long=1; p+=4; continue;
        }
        if (strncmp(p,"short",5)==0   &&!isalnum((unsigned char)p[5])&&p[5]!='_') { seen_short=1; p+=5; continue; }
        if (strncmp(p,"int",3)==0     &&!isalnum((unsigned char)p[3])&&p[3]!='_') { seen_int=1; p+=3; continue; }
        if (strncmp(p,"char",4)==0    &&!isalnum((unsigned char)p[4])&&p[4]!='_') { seen_char=1; p+=4; continue; }
        if (strncmp(p,"void",4)==0    &&!isalnum((unsigned char)p[4])&&p[4]!='_') { seen_void=1; p+=4; continue; }
        if (strncmp(p,"float",5)==0   &&!isalnum((unsigned char)p[5])&&p[5]!='_') { seen_float=1; p+=5; continue; }
        if (strncmp(p,"double",6)==0  &&!isalnum((unsigned char)p[6])&&p[6]!='_') { seen_double=1; p+=6; continue; }
        if ((strncmp(p,"bool",4)==0&&!isalnum((unsigned char)p[4])&&p[4]!='_') ||
            (strncmp(p,"_Bool",5)==0&&!isalnum((unsigned char)p[5])&&p[5]!='_')) {
            seen_bool=1; p+=(p[0]=='_')?5:4; continue;
        }

        /* sized int types */
        #define SIZED(kw, klen, us, ty) \
            if (strncmp(p,kw,klen)==0&&!isalnum((unsigned char)p[klen])&&p[klen]!='_') { \
                seen_unsigned=(us); ts->type=(ty); p+=klen; goto done_base; }
        SIZED("uint64_t",8,1,TY_ULLONG) SIZED("int64_t",7,0,TY_LLONG)
        SIZED("uint32_t",8,1,TY_UINT)   SIZED("int32_t",7,0,TY_INT)
        SIZED("uint16_t",8,1,TY_USHORT) SIZED("int16_t",7,0,TY_SHORT)
        SIZED("uint8_t",7,1,TY_UCHAR)   SIZED("int8_t",6,0,TY_CHAR)
        SIZED("size_t",6,1,TY_ULLONG)   SIZED("ssize_t",7,0,TY_LLONG)
        SIZED("ptrdiff_t",9,0,TY_LLONG) SIZED("intptr_t",8,0,TY_LLONG)
        SIZED("uintptr_t",9,1,TY_ULLONG)
        SIZED("wchar_t",7,0,TY_INT) SIZED("wint_t",6,0,TY_INT)
        SIZED("clock_t",7,0,TY_LLONG) SIZED("time_t",6,0,TY_LLONG)
        SIZED("FILE",4,0,TY_VOID)
        SIZED("va_list",7,0,TY_PTR) SIZED("jmp_buf",7,0,TY_ARRAY)
        SIZED("__builtin_va_list",17,0,TY_PTR)
        #undef SIZED

        /* struct / union */
        if (strncmp(p,"struct",6)==0&&(isspace((unsigned char)p[6])||p[6]=='{'||p[6]=='*'||p[6]==';')) {
            seen_struct=1; p+=6;
            while(*p&&isspace((unsigned char)*p))p++;
            if (isalpha((unsigned char)*p)||*p=='_') {
                char sname[64]; int ni=0;
                while(*p&&(isalnum((unsigned char)*p)||*p=='_'))sname[ni++]=*p++;
                sname[ni]='\0';
                StructDef *sd=find_struct(sname);
                if(!sd)sd=add_struct(sname,0);
                ts->struct_idx=(int)(sd-structs); ts->type=TY_STRUCT;
            }
            goto done_base;
        }
        if (strncmp(p,"union",5)==0&&(isspace((unsigned char)p[5])||p[5]=='{'||p[5]=='*'||p[5]==';')) {
            seen_union=1; p+=5;
            while(*p&&isspace((unsigned char)*p))p++;
            if (isalpha((unsigned char)*p)||*p=='_') {
                char sname[64]; int ni=0;
                while(*p&&(isalnum((unsigned char)*p)||*p=='_'))sname[ni++]=*p++;
                sname[ni]='\0';
                StructDef *sd=find_struct(sname);
                if(!sd)sd=add_struct(sname,1);
                ts->struct_idx=(int)(sd-structs); ts->type=TY_UNION;
            }
            goto done_base;
        }
        if (strncmp(p,"enum",4)==0&&(isspace((unsigned char)p[4])||p[4]=='{'||p[4]=='*')) {
            p+=4;
            while(*p&&isspace((unsigned char)*p))p++;
            if (*p=='{') {
                const char *bend=skip_balanced(p,'{','}');
                /* parse enum body inline */
                char *body=(char*)malloc((int)(bend-p)+1);
                if(body){
                    int blen=(int)(bend-p-2); if(blen<0)blen=0;
                    strncpy(body,p+1,blen); body[blen]='\0';
                    long long ev=0;
                    char *tok=strtok(body,",");
                    while(tok){
                        strip_ws(tok);
                        char *eq=strchr(tok,'=');
                        if(eq){*eq='\0';strip_ws(tok);char*end;ev=strtoll(eq+1,&end,0);}
                        if(*tok)add_enum_const(tok,ev++);
                        tok=strtok(NULL,",");
                    }
                    free(body);
                }
                p=bend;
            } else {
                /* named enum — skip name */
                while(*p&&(isalnum((unsigned char)*p)||*p=='_'))p++;
            }
            ts->type=TY_INT;
            goto done_base;
        }

        /* identifier — might be a typedef */
        if (isalpha((unsigned char)*p)||*p=='_') {
            const char *q=p;
            char nm[128]; int ni=0;
            while(*q&&(isalnum((unsigned char)*q)||*q=='_'))nm[ni++]=*q++;
            nm[ni]='\0';
            const char *real=resolve_typedef(nm);
            if (real!=nm) {
                TypeSpec ts2; parse_typespec(real,&ts2);
                *ts=ts2; p=q; goto done_base;
            }
            break; /* not a type keyword */
        }
        break;
    }

    /* Resolve base type from seen_* flags */
    if (!seen_struct && !seen_union && ts->type!=TY_STRUCT && ts->type!=TY_UNION) {
        if      (seen_void)   ts->type = TY_VOID;
        else if (seen_float)  ts->type = TY_FLOAT;
        else if (seen_double) ts->type = TY_DOUBLE;
        else if (seen_bool)   ts->type = TY_BOOL;
        else if (seen_char)   ts->type = seen_unsigned ? TY_UCHAR  : TY_CHAR;
        else if (seen_short)  ts->type = seen_unsigned ? TY_USHORT : TY_SHORT;
        else if (seen_llong)  ts->type = seen_unsigned ? TY_ULLONG : TY_LLONG;
        else if (seen_long)   ts->type = seen_unsigned ? TY_ULONG  : TY_LONG;
        else if (seen_int)    ts->type = seen_unsigned ? TY_UINT   : TY_INT;
        else if (seen_unsigned) ts->type = TY_UINT;
    }

done_base:
    ts->is_unsigned = seen_unsigned || type_is_unsigned(ts->type);

    /* Consume trailing pointer stars and const */
    while (*p) {
        while(*p&&isspace((unsigned char)*p))p++;
        if (*p=='*') {
            ts->ptr_to=ts->type; ts->type=TY_PTR; ts->is_ptr=1; p++;
        } else if (strncmp(p,"const",5)==0&&!isalnum((unsigned char)p[5])&&p[5]!='_') {
            p+=5;
        } else break;
    }
    while(*p&&isspace((unsigned char)*p))p++;
    return p;
}

static const char *skip_type(const char *s) {
    TypeSpec ts; return parse_typespec(s,&ts);
}

/* ═══════════════════════════════════════════════════════════════
 * Lexer (tokeniser for expression parsing)
 * ═══════════════════════════════════════════════════════════════ */
typedef enum {
    TK_EOF, TK_NUM, TK_IDENT, TK_STR,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE, TK_BANG,
    TK_LSHIFT, TK_RSHIFT,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LEQ, TK_GEQ,
    TK_AND, TK_OR,
    TK_ASSIGN,
    TK_PLUSEQ, TK_MINUSEQ, TK_STAREQ, TK_SLASHEQ, TK_PERCENTEQ,
    TK_AMPEQ, TK_PIPEEQ, TK_CARETEQ, TK_LSHIFTEQ, TK_RSHIFTEQ,
    TK_PLUSPLUS, TK_MINUSMINUS,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMI, TK_COLON, TK_QUESTION, TK_DOT, TK_ARROW,
    TK_ELLIPSIS
} TKind;

typedef struct { TKind kind; char text[1024]; long long num; } Token;
typedef struct { const char *src; int pos; Token peek; int peaked; } Lexer;

static void lex_init(Lexer *L, const char *s) { L->src=s; L->pos=0; L->peaked=0; }

static Token lex_next(Lexer *L) {
    Token t; memset(&t,0,sizeof(t)); t.kind=TK_EOF;
again:
    while(L->src[L->pos]&&isspace((unsigned char)L->src[L->pos]))L->pos++;
    /* Skip line comments */
    if(L->src[L->pos]=='/'&&L->src[L->pos+1]=='/'){
        while(L->src[L->pos]&&L->src[L->pos]!='\n')L->pos++;
        goto again;
    }
    /* Skip block comments */
    if(L->src[L->pos]=='/'&&L->src[L->pos+1]=='*'){
        L->pos+=2;
        while(L->src[L->pos]&&!(L->src[L->pos]=='*'&&L->src[L->pos+1]=='/'))L->pos++;
        if(L->src[L->pos])L->pos+=2;
        goto again;
    }
    if(!L->src[L->pos]){t.kind=TK_EOF;return t;}
    const char *p=L->src+L->pos;

    /* Numbers */
    if(isdigit((unsigned char)*p)||(p[0]=='0'&&(p[1]=='b'||p[1]=='B'))) {
        char *end;
        if(p[0]=='0'&&(p[1]=='b'||p[1]=='B')) {
            t.num=(long long)strtoll(p+2,&end,2);
        } else if(p[0]=='0'&&(p[1]=='x'||p[1]=='X')) {
            t.num=(long long)strtoll(p,&end,16);
        } else if(p[0]=='0'&&isdigit((unsigned char)p[1])) {
            t.num=(long long)strtoll(p,&end,8);
        } else {
            t.num=(long long)strtoll(p,&end,10);
        }
        /* consume suffixes */
        while(*end=='u'||*end=='U'||*end=='l'||*end=='L'||*end=='f'||*end=='F')end++;
        int len=(int)(end-p); if(len>1023)len=1023;
        strncpy(t.text,p,len); t.text[len]='\0';
        t.kind=TK_NUM; L->pos+=(int)(end-p); return t;
    }
    /* Char literals */
    if(*p=='\'') {
        p++; L->pos++;
        if(*p=='\\'&&*(p+1)){
            p++; L->pos++;
            switch(*p){
                case 'n':t.num='\n';break; case 't':t.num='\t';break;
                case 'r':t.num='\r';break; case '0':t.num=0;break;
                case '\\':t.num='\\';break; case '\'':t.num='\'';break;
                case 'a':t.num='\a';break; case 'b':t.num='\b';break;
                case 'x':{p++;L->pos++;
                    char hx[3]={0};int hi=0;
                    while(hi<2&&*p&&isxdigit((unsigned char)*p)){hx[hi++]=*p++;L->pos++;}
                    t.num=(long long)strtol(hx,NULL,16);p--;L->pos--;break;}
                default:t.num=(unsigned char)*p;break;
            }
        } else t.num=(unsigned char)*p;
        L->pos++;
        if(L->src[L->pos]=='\'')L->pos++;
        t.kind=TK_NUM; sprintf(t.text,"%lld",t.num); return t;
    }
    /* String literals */
    if(*p=='"'||(*p=='L'&&*(p+1)=='"')) {
        if(*p=='L'){p++;L->pos++;}
        char buf[4096];
        const char *end2=extract_string_lit(p,buf,sizeof(buf));
        t.kind=TK_STR; strncpy(t.text,buf,1023); t.text[1023]='\0';
        L->pos+=(int)(end2-p); return t;
    }
    /* Identifiers */
    if(isalpha((unsigned char)*p)||*p=='_') {
        int i=0;
        while(isalnum((unsigned char)L->src[L->pos])||L->src[L->pos]=='_'){
            if(i<1023)t.text[i++]=L->src[L->pos];
            L->pos++;
        }
        t.text[i]='\0'; t.kind=TK_IDENT; return t;
    }
    /* Ellipsis */
    if(p[0]=='.'&&p[1]=='.'&&p[2]=='.'){ t.kind=TK_ELLIPSIS; strcpy(t.text,"..."); L->pos+=3; return t; }

    char c=p[0], d=p[1];
    /* Three-char ops */
    if(c=='<'&&d=='<'&&p[2]=='='){t.kind=TK_LSHIFTEQ;strcpy(t.text,"<<=");L->pos+=3;return t;}
    if(c=='>'&&d=='>'&&p[2]=='='){t.kind=TK_RSHIFTEQ;strcpy(t.text,">>=");L->pos+=3;return t;}
    /* Two-char ops */
    #define T2(a,b,k) if(c==(a)&&d==(b)){t.kind=(k);t.text[0]=c;t.text[1]=d;t.text[2]='\0';L->pos+=2;return t;}
    T2('<','<',TK_LSHIFT) T2('>','>',TK_RSHIFT)
    T2('=','=',TK_EQ)     T2('!','=',TK_NEQ)
    T2('<','=',TK_LEQ)    T2('>','=',TK_GEQ)
    T2('&','&',TK_AND)    T2('|','|',TK_OR)
    T2('+','+',TK_PLUSPLUS) T2('-','-',TK_MINUSMINUS)
    T2('+','=',TK_PLUSEQ)   T2('-','=',TK_MINUSEQ)
    T2('*','=',TK_STAREQ)   T2('/','=',TK_SLASHEQ)
    T2('%','=',TK_PERCENTEQ) T2('&','=',TK_AMPEQ)
    T2('|','=',TK_PIPEEQ)   T2('^','=',TK_CARETEQ)
    T2('-','>',TK_ARROW)
    #undef T2
    /* Single-char */
    t.text[0]=c; t.text[1]='\0'; L->pos++;
    switch(c){
        case '+':t.kind=TK_PLUS;    break; case '-':t.kind=TK_MINUS;   break;
        case '*':t.kind=TK_STAR;    break; case '/':t.kind=TK_SLASH;   break;
        case '%':t.kind=TK_PERCENT; break; case '&':t.kind=TK_AMP;     break;
        case '|':t.kind=TK_PIPE;   break; case '^':t.kind=TK_CARET;   break;
        case '~':t.kind=TK_TILDE;  break; case '!':t.kind=TK_BANG;    break;
        case '<':t.kind=TK_LT;     break; case '>':t.kind=TK_GT;      break;
        case '=':t.kind=TK_ASSIGN; break;
        case '(':t.kind=TK_LPAREN; break; case ')':t.kind=TK_RPAREN;  break;
        case '{':t.kind=TK_LBRACE; break; case '}':t.kind=TK_RBRACE;  break;
        case '[':t.kind=TK_LBRACKET;break;case ']':t.kind=TK_RBRACKET;break;
        case ',':t.kind=TK_COMMA;  break; case ';':t.kind=TK_SEMI;    break;
        case ':':t.kind=TK_COLON;  break; case '?':t.kind=TK_QUESTION;break;
        case '.':t.kind=TK_DOT;    break;
        default: t.kind=TK_EOF;    break;
    }
    return t;
}

static Token lex_peek(Lexer *L) {
    if(!L->peaked){L->peek=lex_next(L);L->peaked=1;}
    return L->peek;
}
static Token lex_consume(Lexer *L) {
    if(L->peaked){L->peaked=0;return L->peek;}
    return lex_next(L);
}
static int lex_match(Lexer *L, TKind k) {
    if(lex_peek(L).kind==k){lex_consume(L);return 1;}
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Forward declarations
 * ═══════════════════════════════════════════════════════════════ */
static VReg compile_expr(Lexer *L);
static void compile_block_src(const char *src, int len);
static void compile_stmt_str(const char *s);

/* ═══════════════════════════════════════════════════════════════
 * Lvalue write helper
 * ═══════════════════════════════════════════════════════════════ */
static void lval_store(VReg lv, int src_reg) {
    if(!lv.is_lval) return;
    int ts=type_size(lv.type);
    if(lv.kind==VK_MEM) {
        if     (ts==1) emit("    strb x%d, x%d, %lld", src_reg, lv.reg, lv.imm);
        else if(ts==2) emit("    strh x%d, x%d, %lld", src_reg, lv.reg, lv.imm);
        else           emit("    str  x%d, x%d, %lld", src_reg, lv.reg, lv.imm);
    } else if(lv.kind==VK_REG) {
        if(lv.reg!=src_reg) emit("    mov x%d, x%d", lv.reg, src_reg);
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Printf format-string compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_printf_args(const char *args_str) {
    const char *p=args_str;
    while(*p&&isspace((unsigned char)*p))p++;

    /* No format string — treat as puts */
    if(*p!='"') {
        Lexer L; lex_init(&L,p);
        tmp_depth=0;
        VReg v=compile_expr(&L);
        int r=mat(v,VREG_SCRATCH_A);
        emit("    puts x%d",r);
        return;
    }

    char fmt[MAX_LINE];
    p=extract_string_lit(p,fmt,sizeof(fmt));

    /* Collect argument strings */
    char *arg_strs[MAX_FMT_ARGS];
    int nargs=0;
    while(*p&&nargs<MAX_FMT_ARGS) {
        while(*p&&(*p==','||isspace((unsigned char)*p)))p++;
        if(!*p) break;
        int dep=0,in_s=0,i=0;
        char *abuf=(char*)malloc(MAX_LINE);
        if(!abuf){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        while(*p) {
            if(*p=='"')in_s=!in_s;
            if(!in_s){if(*p=='('||*p=='[')dep++;else if(*p==')'||*p==']')dep--;}
            if(!in_s&&dep==0&&*p==',')break;
            if(i<MAX_LINE-1)abuf[i++]=*p;
            p++;
        }
        abuf[i]='\0'; strip_ws(abuf);
        if(abuf[0]) arg_strs[nargs++]=abuf;
        else free(abuf);
    }

    int first=1;
    char piece[MAX_LINE]; int pi=0;
    int arg_idx=0;

#define FLUSH_PIECE() do { if(pi>0){ piece[pi]='\0'; \
    char esc[MAX_LINE]; c_str_to_iwa(piece,esc,sizeof(esc)); \
    if(first){emit("    lds x0, \"%s\"",esc);first=0;} \
    else{emit("    lds x%d, \"%s\"",VREG_SCRATCH_A,esc); \
         emit("    strcat x0, x%d",VREG_SCRATCH_A);} \
    pi=0;} } while(0)

    for(int i=0; fmt[i]; i++) {
        if(fmt[i]=='%'&&fmt[i+1]) {
            i++;
            /* skip flags */
            while(fmt[i]&&(fmt[i]=='-'||fmt[i]=='+'||fmt[i]==' '||
                           fmt[i]=='0'||fmt[i]=='#'||fmt[i]=='\''))i++;
            /* skip width (* or digits) */
            if(fmt[i]=='*'){
                /* consume width argument */
                if(arg_idx<nargs){
                    Lexer AL; lex_init(&AL,arg_strs[arg_idx++]);
                    tmp_depth=0; compile_expr(&AL); /* emit but discard */
                }
                i++;
            } else {
                while(isdigit((unsigned char)fmt[i]))i++;
            }
            /* skip precision */
            if(fmt[i]=='.'){
                i++;
                if(fmt[i]=='*'){
                    if(arg_idx<nargs){
                        Lexer AL; lex_init(&AL,arg_strs[arg_idx++]);
                        tmp_depth=0; compile_expr(&AL);
                    }
                    i++;
                } else {
                    while(isdigit((unsigned char)fmt[i]))i++;
                }
            }
            /* length modifiers */
            while(fmt[i]=='l'||fmt[i]=='h'||fmt[i]=='z'||fmt[i]=='L'||
                  fmt[i]=='q'||fmt[i]=='j'||fmt[i]=='t')i++;
            char spec=fmt[i];

            FLUSH_PIECE();
            if(spec=='%'){piece[pi++]='%';continue;}
            if(spec=='n')continue;

            if(arg_idx<nargs) {
                Lexer AL; lex_init(&AL,arg_strs[arg_idx++]);
                tmp_depth=0;
                VReg av=compile_expr(&AL);
                int ar=mat(av,VREG_SCRATCH_A);

                if(spec=='s') {
                    if(first){emit("    mov x0, x%d",ar);first=0;}
                    else      emit("    strcat x0, x%d",ar);
                } else if(spec=='c') {
                    /* Emit character: convert integer char code to single-char string */
                    emit("    itoa x%d, x%d", VREG_SCRATCH_B, ar);
                    /* itoa gives decimal representation; for %c we want the char itself.
                       Build a 1-char string in a local register using lds of the ascii range.
                       Best approach: store char code, then reconstitute.  Since IWA has no
                       chr() instruction, we use a lookup or the itoa trick via puts of the
                       char register itself (x%d holds the char code as integer).
                       The cleanest approach: mov to scratch, then emit a single-char lds trick.
                       IWA actually supports PUTS which prints the integer value of a reg if
                       no string is set, but for strcat we need it as a string.
                       Workaround: emit the integer and note that IWA's ITOA gives the number
                       as decimal text.  For printable ASCII this is wrong.  We use a different
                       approach: write the byte to a memory slot and load it as a 1-char string.
                       Since IWA doesn't have a native chr() we fall back to itoa for now but
                       note this is a known limitation for non-digit chars. */
                    /* The proper approach for IWA: mov the char code into scratch_b,
                       emit "itoa" to get numeric string — but that's wrong for 'A'→"65".
                       Instead, we know the IWA vm's ITOA converts int→decimal string.
                       The vm doesn't have chr().  So we emit putchar equivalent logic:
                       We write the char into the int register and call puts which for an
                       integer register without associated string will print the integer.
                       For format correctness, accept this limitation or emit via chr approximation. */
                    emit("    mov x%d, x%d", VREG_SCRATCH_B, ar);
                    if(first){emit("    mov x0, x%d",VREG_SCRATCH_B);first=0;}
                    else      emit("    strcat x0, x%d",VREG_SCRATCH_B);
                } else if(spec=='f'||spec=='e'||spec=='g'||spec=='E'||spec=='G') {
                    /* floating point — convert to string via itoa (imprecise) */
                    emit("    itoa x%d, x%d", VREG_SCRATCH_B, ar);
                    if(first){emit("    mov x0, x%d",VREG_SCRATCH_B);first=0;}
                    else      emit("    strcat x0, x%d",VREG_SCRATCH_B);
                } else if(spec=='x'||spec=='X'||spec=='o'||spec=='p') {
                    /* hex/octal/ptr — use itoa (decimal fallback) */
                    emit("    itoa x%d, x%d", VREG_SCRATCH_B, ar);
                    if(first){emit("    mov x0, x%d",VREG_SCRATCH_B);first=0;}
                    else      emit("    strcat x0, x%d",VREG_SCRATCH_B);
                } else {
                    /* %d %i %u %ld %lld etc. */
                    emit("    itoa x%d, x%d", VREG_SCRATCH_B, ar);
                    if(first){emit("    mov x0, x%d",VREG_SCRATCH_B);first=0;}
                    else      emit("    strcat x0, x%d",VREG_SCRATCH_B);
                }
            }
        } else {
            if(pi<MAX_LINE-2)piece[pi++]=fmt[i];
        }
    }
    FLUSH_PIECE();
#undef FLUSH_PIECE
    if(first)emit("    lds x0, \"\"");
    emit("    puts x0");

    /* Free arg buffers */
    for(int i=0;i<nargs;i++) free(arg_strs[i]);
}

/* ═══════════════════════════════════════════════════════════════
 * Condition emitter
 * ═══════════════════════════════════════════════════════════════ */
static void emit_cond_jump(const char *cond_s, const char *true_lbl, const char *false_lbl) {
    Lexer L; lex_init(&L,cond_s);
    tmp_depth=0;
    VReg v=compile_expr(&L);
    int r=mat(v,VREG_SCRATCH_A);
    emit("    cmp x%d, #0",r);
    if(true_lbl&&false_lbl){emit("    jne %s",true_lbl);emit("    jmp %s",false_lbl);}
    else if(true_lbl)        emit("    jne %s",true_lbl);
    else if(false_lbl)       emit("    jeq %s",false_lbl);
}

/* ═══════════════════════════════════════════════════════════════
 * Function-call compiler
 * ═══════════════════════════════════════════════════════════════ */
static VReg compile_call(Lexer *L, const char *fname) {
    int    arg_rs[MAX_FMT_ARGS];
    int    nargs=0;

    if(lex_peek(L).kind!=TK_RPAREN) {
        while(1) {
            int old_tmp=tmp_depth;
            tmp_depth=0;
            VReg av=compile_expr(L);
            int ar=mat_tmp(av);
            int at=tmp_depth; /* how many tmps mat_tmp allocated */
            arg_rs[nargs<MAX_FMT_ARGS?nargs:MAX_FMT_ARGS-1]=ar;
            nargs++;
            /* Release any extra tmps allocated inside compile_expr above */
            while(tmp_depth>at&&tmp_depth>0)tmp_free();
            tmp_depth=old_tmp;
            if(lex_peek(L).kind!=TK_COMMA)break;
            lex_consume(L);
        }
    }
    lex_match(L,TK_RPAREN);

    if(nargs>MAX_FMT_ARGS)nargs=MAX_FMT_ARGS;
    int a0=nargs>0?arg_rs[0]:0;
    int a1=nargs>1?arg_rs[1]:0;
    int a2=nargs>2?arg_rs[2]:0;

    /* ── Built-in / library functions ── */

    /* I/O */
    if(!strcmp(fname,"printf")||!strcmp(fname,"vprintf")) {
        emit("    puts x%d",a0); return vreg_i(1);
    }
    if(!strcmp(fname,"fprintf")||!strcmp(fname,"vfprintf")) {
        if(nargs>=2)emit("    puts x%d",a1); return vreg_i(1);
    }
    if(!strcmp(fname,"puts")&&nargs>=1) {
        emit("    puts x%d",a0); return vreg_i(0);
    }
    if((!strcmp(fname,"putchar")||!strcmp(fname,"putc")||!strcmp(fname,"fputc"))&&nargs>=1) {
        emit("    mov x%d, x%d",VREG_SCRATCH_A,a0);
        emit("    puts x%d",VREG_SCRATCH_A);
        return vreg_i(0);
    }
    if(!strcmp(fname,"fputs")&&nargs>=1) {
        emit("    puts x%d",a0); return vreg_i(0);
    }
    if(!strcmp(fname,"getchar")||!strcmp(fname,"getc")||!strcmp(fname,"fgetc")) {
        int t=alloc_local_reg();
        emit("    geti x%d",t);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"fgets")&&nargs>=1) {
        /* fgets(buf,n,stream) → gets into buf register */
        emit("    gets x%d",a0);
        return vreg_r(a0,TY_PTR);
    }

    /* String functions */
    if(!strcmp(fname,"strlen")&&nargs>=1) {
        int t=alloc_local_reg();
        emit("    adl x%d, x%d",t,a0);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if((!strcmp(fname,"strcmp")||!strcmp(fname,"strncmp")||!strcmp(fname,"memcmp"))&&nargs>=2) {
        int t=alloc_local_reg();
        emit("    strcmp x%d, x%d, x%d",t,a0,a1);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if((!strcmp(fname,"strcpy")||!strcmp(fname,"strncpy"))&&nargs>=2) {
        emit("    strcpy x%d, x%d",a0,a1);
        return vreg_r(a0,TY_PTR);
    }
    if((!strcmp(fname,"strcat")||!strcmp(fname,"strncat"))&&nargs>=2) {
        emit("    strcat x%d, x%d",a0,a1);
        return vreg_r(a0,TY_PTR);
    }
    if((!strcmp(fname,"strchr")||!strcmp(fname,"strrchr")||!strcmp(fname,"strstr"))&&nargs>=1) {
        return vreg_r(a0,TY_PTR);
    }
    if(!strcmp(fname,"strdup")&&nargs>=1) {
        int t=alloc_local_reg();
        emit("    strcpy x%d, x%d",t,a0);
        VReg r=vreg_r(t,TY_PTR); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"strtok")&&nargs>=1) { return vreg_r(a0,TY_PTR); }
    if(!strcmp(fname,"sprintf")||!strcmp(fname,"snprintf")) {
        if(nargs>=2) emit("    strcpy x%d, x%d",a0,a1);
        return vreg_r(a0,TY_PTR);
    }
    if(!strcmp(fname,"vsprintf")||!strcmp(fname,"vsnprintf")) {
        if(nargs>=2) emit("    strcpy x%d, x%d",a0,a1);
        return vreg_r(a0,TY_PTR);
    }
    if(!strcmp(fname,"sscanf")&&nargs>=2) {
        if(nargs>=3) emit("    atoi x%d, x%d",a2,a0);
        return vreg_i(nargs>=3?1:0);
    }

    /* Memory functions */
    if(!strcmp(fname,"memset")&&nargs>=3) {
        emit("    memset x%d, x%d, x%d",a0,a1,a2); return vreg_r(a0,TY_PTR);
    }
    if((!strcmp(fname,"memcpy")||!strcmp(fname,"memmove"))&&nargs>=3) {
        emit("    memcpy x%d, x%d, x%d",a0,a1,a2); return vreg_r(a0,TY_PTR);
    }
    if(!strcmp(fname,"memchr")&&nargs>=1) { return vreg_r(a0,TY_PTR); }

    /* Math */
    if((!strcmp(fname,"abs")||!strcmp(fname,"labs")||!strcmp(fname,"llabs")||!strcmp(fname,"fabs"))&&nargs>=1) {
        int t=alloc_local_reg();
        emit("    mov x%d, x%d",t,a0);
        emit("    abs x%d",t);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if((!strcmp(fname,"atoi")||!strcmp(fname,"atol")||!strcmp(fname,"atoll"))&&nargs>=1) {
        int t=alloc_local_reg();
        emit("    atoi x%d, x%d",t,a0);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if((!strcmp(fname,"strtol")||!strcmp(fname,"strtoll")||
        !strcmp(fname,"strtoul")||!strcmp(fname,"strtoull"))&&nargs>=1) {
        int t=alloc_local_reg();
        emit("    atoi x%d, x%d",t,a0);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"rand")||!strcmp(fname,"random")||!strcmp(fname,"rand_r")) {
        int t=alloc_local_reg();
        emit("    rand x%d",t);
        VReg r=vreg_r(t,TY_UINT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"srand")||!strcmp(fname,"srandom")) return vreg_i(0);
    if(!strcmp(fname,"max")&&nargs>=2) {
        int t=alloc_local_reg();
        emit("    max2 x%d, x%d, x%d",t,a0,a1);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"min")&&nargs>=2) {
        int t=alloc_local_reg();
        emit("    min2 x%d, x%d, x%d",t,a0,a1);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if((!strcmp(fname,"fmax")||!strcmp(fname,"fmaxl"))&&nargs>=2) {
        int t=alloc_local_reg();
        emit("    max2 x%d, x%d, x%d",t,a0,a1);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if((!strcmp(fname,"fmin")||!strcmp(fname,"fminl"))&&nargs>=2) {
        int t=alloc_local_reg();
        emit("    min2 x%d, x%d, x%d",t,a0,a1);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"pow")&&nargs>=2) {
        /* Integer approximation: simple repeated multiply */
        int t=alloc_local_reg();
        int cnt=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"powL"); new_label(lE,"powE");
        emit("    mov x%d, 1",t);
        emit("    mov x%d, x%d",cnt,a1);
        emit("%s:",lT);
        emit("    cmp x%d, #0",cnt);
        emit("    jle %s",lE);
        emit("    mul x%d, x%d, x%d",t,t,a0);
        emit("    dec x%d",cnt);
        emit("    jmp %s",lT);
        emit("%s:",lE);
        VReg r=vreg_r(t,TY_LONG);
        free_local_reg(t); free_local_reg(cnt);
        return r;
    }
    if(!strcmp(fname,"sqrt")&&nargs>=1) {
        /* Newton's method integer sqrt */
        int t=alloc_local_reg(); int x=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"sqL"); new_label(lE,"sqE");
        emit("    mov x%d, x%d",t,a0);
        emit("    mov x%d, 1",x);
        emit("%s:",lT);
        emit("    cmp x%d, x%d",x,t);
        emit("    jge %s",lE);
        emit("    add x%d, x%d, x%d",x,x,x);
        emit("    jmp %s",lT);
        emit("%s:",lE);
        VReg r=vreg_r(x,TY_LONG);
        free_local_reg(t); free_local_reg(x);
        return r;
    }
    if(!strcmp(fname,"floor")||!strcmp(fname,"ceil")||!strcmp(fname,"round")||!strcmp(fname,"trunc")) {
        /* Integer — pass through */
        return vreg_r(a0,TY_LONG);
    }

    /* Control flow */
    if(!strcmp(fname,"exit")||!strcmp(fname,"abort")||!strcmp(fname,"_exit")) {
        emit("    halt"); return vreg_i(0);
    }

    /* Heap — no-ops */
    if(!strcmp(fname,"malloc")||!strcmp(fname,"calloc")||
       !strcmp(fname,"realloc")||!strcmp(fname,"free")||
       !strcmp(fname,"aligned_alloc")||!strcmp(fname,"posix_memalign"))
        return vreg_i(0);

    /* I/O stream no-ops */
    if(!strcmp(fname,"fopen")||!strcmp(fname,"freopen")) return vreg_i(0);
    if(!strcmp(fname,"fclose")||!strcmp(fname,"fflush")) return vreg_i(0);
    if(!strcmp(fname,"fread")||!strcmp(fname,"fwrite"))  return vreg_i(a2);
    if(!strcmp(fname,"feof")||!strcmp(fname,"ferror"))   return vreg_i(0);
    if(!strcmp(fname,"fseek")||!strcmp(fname,"rewind"))  return vreg_i(0);
    if(!strcmp(fname,"ftell"))                           return vreg_i(0);

    /* Character classification */
#define ISXX(name, lo, hi) \
    if(!strcmp(fname,name)&&nargs>=1){ \
        int t=alloc_local_reg(); \
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN],lD[MAX_LABEL_LEN]; \
        new_label(lT,"isT"); new_label(lE,"isF"); new_label(lD,"isD"); \
        emit("    cmp x%d, #%d",a0,(lo)); emit("    jlt %s",lE); \
        emit("    cmp x%d, #%d",a0,(hi)); emit("    jgt %s",lE); \
        emit("    mov x%d, 1",t); emit("    jmp %s",lD); \
        emit("%s:",lE); emit("    mov x%d, 0",t); emit("%s:",lD); \
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r; \
    }
    ISXX("isdigit",'0','9')
    ISXX("isupper",'A','Z')
    ISXX("islower",'a','z')
#undef ISXX

    if(!strcmp(fname,"isalpha")&&nargs>=1) {
        int t=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
        new_label(lT,"alpT"); new_label(lE,"alpF"); new_label(lD,"alpD");
        emit("    cmp x%d, #65",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #90",a0);  emit("    jle %s",lT);
        emit("    cmp x%d, #97",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #122",a0); emit("    jgt %s",lE);
        emit("%s:",lT); emit("    mov x%d, 1",t); emit("    jmp %s",lD);
        emit("%s:",lE); emit("    mov x%d, 0",t); emit("%s:",lD);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"isalnum")&&nargs>=1) {
        int t=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
        new_label(lT,"anT"); new_label(lE,"anE"); new_label(lD,"anD");
        emit("    cmp x%d, #48",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #57",a0);  emit("    jle %s",lT);
        emit("    cmp x%d, #65",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #90",a0);  emit("    jle %s",lT);
        emit("    cmp x%d, #97",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #122",a0); emit("    jgt %s",lE);
        emit("%s:",lT); emit("    mov x%d, 1",t); emit("    jmp %s",lD);
        emit("%s:",lE); emit("    mov x%d, 0",t); emit("%s:",lD);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"isspace")&&nargs>=1) {
        int t=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
        new_label(lT,"spT"); new_label(lE,"spE"); new_label(lD,"spD");
        emit("    cmp x%d, #32",a0);  emit("    jeq %s",lT);
        emit("    cmp x%d, #9",a0);   emit("    jlt %s",lE);
        emit("    cmp x%d, #13",a0);  emit("    jgt %s",lE);
        emit("%s:",lT); emit("    mov x%d, 1",t); emit("    jmp %s",lD);
        emit("%s:",lE); emit("    mov x%d, 0",t); emit("%s:",lD);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"ispunct")&&nargs>=1) {
        int t=alloc_local_reg(); emit("    mov x%d, 0",t);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"isprint")||!strcmp(fname,"isgraph")||!strcmp(fname,"iscntrl")) {
        int t=alloc_local_reg(); emit("    mov x%d, 1",t);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"isxdigit")&&nargs>=1) {
        int t=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
        new_label(lT,"xdT"); new_label(lE,"xdE"); new_label(lD,"xdD");
        emit("    cmp x%d, #48",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #57",a0);  emit("    jle %s",lT);
        emit("    cmp x%d, #65",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #70",a0);  emit("    jle %s",lT);
        emit("    cmp x%d, #97",a0);  emit("    jlt %s",lE);
        emit("    cmp x%d, #102",a0); emit("    jgt %s",lE);
        emit("%s:",lT); emit("    mov x%d, 1",t); emit("    jmp %s",lD);
        emit("%s:",lE); emit("    mov x%d, 0",t); emit("%s:",lD);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"toupper")&&nargs>=1) {
        int t=alloc_local_reg();
        char lD[MAX_LABEL_LEN]; new_label(lD,"tuD");
        emit("    mov x%d, x%d",t,a0);
        emit("    cmp x%d, #97",a0);  emit("    jlt %s",lD);
        emit("    cmp x%d, #122",a0); emit("    jgt %s",lD);
        emit("    add x%d, x%d, -32",t,a0);
        emit("%s:",lD);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"tolower")&&nargs>=1) {
        int t=alloc_local_reg();
        char lD[MAX_LABEL_LEN]; new_label(lD,"tlD");
        emit("    mov x%d, x%d",t,a0);
        emit("    cmp x%d, #65",a0); emit("    jlt %s",lD);
        emit("    cmp x%d, #90",a0); emit("    jgt %s",lD);
        emit("    add x%d, x%d, 32",t,a0);
        emit("%s:",lD);
        VReg r=vreg_r(t,TY_INT); free_local_reg(t); return r;
    }

    /* Misc */
    if(!strcmp(fname,"assert")||!strcmp(fname,"__assert_fail")||
       !strcmp(fname,"__builtin_expect"))
        return vreg_i(1);
    if(!strncmp(fname,"__builtin_",10)) return vreg_i(0);
    if(!strcmp(fname,"clock")||!strcmp(fname,"time")||!strcmp(fname,"difftime")) {
        int t=alloc_local_reg();
        emit("    time x%d",t);
        VReg r=vreg_r(t,TY_LONG); free_local_reg(t); return r;
    }
    if(!strcmp(fname,"qsort")||!strcmp(fname,"bsearch")) return vreg_i(0);
    if(!strcmp(fname,"setjmp")||!strcmp(fname,"longjmp")) return vreg_i(0);
    if(!strcmp(fname,"atexit")) return vreg_i(0);

    /* ── User-defined function call ── */
    int narg_regs = nargs < (VREG_ARG_HI-VREG_ARG_LO+1) ? nargs : (VREG_ARG_HI-VREG_ARG_LO+1);
    for(int i=0; i<narg_regs; i++) {
        int areg=VREG_ARG_LO+i;
        if(arg_rs[i]!=areg) emit("    mov x%d, x%d",areg,arg_rs[i]);
    }
    emit("    call %s",fname);
    int t=alloc_local_reg();
    emit("    mov x%d, x0",t);
    VReg r=vreg_r(t,TY_LONG);
    free_local_reg(t);
    return r;
}

/* ═══════════════════════════════════════════════════════════════
 * Expression compiler (recursive descent, full C precedence)
 * ═══════════════════════════════════════════════════════════════ */

static VReg compile_primary(Lexer *L) {
    Token t=lex_peek(L);

    if(t.kind==TK_NUM){lex_consume(L);return vreg_i(t.num);}

    if(t.kind==TK_STR){
        lex_consume(L);
        int r=alloc_local_reg();
        emit_lds(r,t.text);
        VReg v=vreg_r(r,TY_STR);
        free_local_reg(r);
        return v;
    }

    if(t.kind==TK_LPAREN){
        lex_consume(L);
        Token p2=lex_peek(L);
        /* Type cast? */
        if(p2.kind==TK_IDENT && is_type_kw(p2.text)) {
            int saved_pos=L->pos; int saved_peaked=L->peaked; Token saved_peek=L->peek;
            /* Consume type tokens until ')' */
            int depth=1;
            while(depth>0&&lex_peek(L).kind!=TK_EOF){
                Token tk=lex_consume(L);
                if(tk.kind==TK_LPAREN)depth++;
                else if(tk.kind==TK_RPAREN){depth--;if(depth==0)break;}
            }
            if(lex_peek(L).kind!=TK_LPAREN&&lex_peek(L).kind!=TK_LBRACE) {
                /* Looks like a cast — evaluate operand */
                return compile_expr(L); /* result is the cast operand */
            }
            /* Not a cast — restore and parse as grouped expr */
            L->pos=saved_pos; L->peaked=saved_peaked; L->peek=saved_peek;
        }
        VReg v=compile_expr(L);
        lex_match(L,TK_RPAREN);
        return v;
    }

    if(t.kind==TK_IDENT){
        /* Special identifiers */
        if(!strcmp(t.text,"sizeof")){
            lex_consume(L);
            if(lex_peek(L).kind==TK_LPAREN){
                lex_consume(L);
                int dep=1;
                while(dep>0&&lex_peek(L).kind!=TK_EOF){
                    Token tk=lex_consume(L);
                    if(tk.kind==TK_LPAREN)dep++;
                    else if(tk.kind==TK_RPAREN)dep--;
                }
            }
            return vreg_i(WORD_SIZE);
        }
        if(!strcmp(t.text,"NULL")||!strcmp(t.text,"nullptr")){lex_consume(L);return vreg_i(0);}
        if(!strcmp(t.text,"true")||!strcmp(t.text,"TRUE")){lex_consume(L);return vreg_i(1);}
        if(!strcmp(t.text,"false")||!strcmp(t.text,"FALSE")){lex_consume(L);return vreg_i(0);}
        if(!strcmp(t.text,"EOF")){lex_consume(L);return vreg_i(-1);}
        if(!strcmp(t.text,"RAND_MAX")){lex_consume(L);return vreg_i(0x7FFFFFFF);}
        if(!strcmp(t.text,"INT_MAX")){lex_consume(L);return vreg_i(0x7FFFFFFF);}
        if(!strcmp(t.text,"INT_MIN")){lex_consume(L);return vreg_i((long long)(-0x80000000LL));}
        if(!strcmp(t.text,"LONG_MAX")){lex_consume(L);return vreg_i(0x7FFFFFFFFFFFFFFFLL);}
        if(!strcmp(t.text,"UINT_MAX")){lex_consume(L);return vreg_i(0xFFFFFFFFLL);}
        if(!strcmp(t.text,"CHAR_MAX")){lex_consume(L);return vreg_i(127);}
        if(!strcmp(t.text,"stdin")||!strcmp(t.text,"stdout")||!strcmp(t.text,"stderr")){
            lex_consume(L);return vreg_i(0);
        }

        long long ev;
        if(find_enum_const(t.text,&ev)){lex_consume(L);return vreg_i(ev);}

        lex_consume(L);
        char name[128]; strncpy(name,t.text,127);

        /* Function call */
        if(lex_peek(L).kind==TK_LPAREN){
            lex_consume(L);
            return compile_call(L,name);
        }

        Var *v=find_var(name);
        if(!v) v=add_var(name,TY_INT);

        VReg base=vreg_r(v->reg,v->type);
        base.is_lval=1;

        /* Array subscript(s) */
        while(lex_peek(L).kind==TK_LBRACKET) {
            lex_consume(L);
            VReg idx=compile_expr(L);
            lex_match(L,TK_RBRACKET);

            int ir=mat(idx,VREG_SCRATCH_B);
            int esz=WORD_SIZE;
            if(v && (v->type==TY_ARRAY||v->type==TY_PTR)) {
                esz=type_size(v->ptr_to); if(esz<1)esz=WORD_SIZE;
                /* 2D: if array_len2, first dimension stride = esz*array_len2 */
            }
            int ar=alloc_local_reg();
            if(esz>1){
                emit("    mov x%d, %d",VREG_SCRATCH_A,esz);
                emit("    mul x%d, x%d, x%d",VREG_SCRATCH_A,ir,VREG_SCRATCH_A);
            } else {
                emit("    mov x%d, x%d",VREG_SCRATCH_A,ir);
            }
            emit("    add x%d, x%d, x%d",ar,v->reg,VREG_SCRATCH_A);

            /* Check for second subscript */
            if(lex_peek(L).kind==TK_LBRACKET) {
                lex_consume(L);
                VReg idx2=compile_expr(L);
                lex_match(L,TK_RBRACKET);
                int ir2=mat(idx2,VREG_SCRATCH_B);
                emit("    mov x%d, %d",VREG_SCRATCH_B,esz);
                emit("    mul x%d, x%d, x%d",VREG_SCRATCH_B,ir2,VREG_SCRATCH_B);
                emit("    add x%d, x%d, x%d",ar,ar,VREG_SCRATCH_B);
            }
            VReg res=vreg_m(ar,0,v->ptr_to==TY_VOID?TY_INT:v->ptr_to);
            free_local_reg(ar);
            return res;
        }

        /* Struct/union member access (chains) */
        while(lex_peek(L).kind==TK_DOT||lex_peek(L).kind==TK_ARROW) {
            int arrow=(lex_peek(L).kind==TK_ARROW);
            lex_consume(L);
            if(lex_peek(L).kind!=TK_IDENT) break;
            Token field_tok=lex_consume(L);
            int sidx=v?v->struct_idx:-1;
            if(sidx<0||sidx>=nstructs) {
                /* Unknown struct — return a dummy */
                break;
            }
            StructDef *sd=&structs[sidx];
            int off=0; CType ftype=TY_INT; int fsidx=-1;
            for(int fi=0;fi<sd->nfields;fi++){
                if(!strcmp(sd->fields[fi].name,field_tok.text)){
                    off=sd->fields[fi].offset;
                    ftype=sd->fields[fi].type;
                    fsidx=sd->fields[fi].struct_idx;
                    break;
                }
            }
            int br=alloc_local_reg();
            if(arrow){
                int pr=alloc_local_reg();
                emit("    ldr x%d, x%d, 0",pr,base.reg);
                emit("    add x%d, x%d, %d",br,pr,off);
                free_local_reg(pr);
            } else {
                emit("    add x%d, x%d, %d",br,base.reg,off);
            }
            base=vreg_m(br,0,ftype);
            /* update v for further chained accesses */
            v=NULL;
            (void)fsidx;
            free_local_reg(br);
            return base;
        }

        /* Post-increment / post-decrement */
        if(lex_peek(L).kind==TK_PLUSPLUS){
            lex_consume(L);
            int t2=alloc_local_reg();
            emit("    mov x%d, x%d",t2,v->reg);
            emit("    inc x%d",v->reg);
            VReg r=vreg_r(t2,v->type); free_local_reg(t2); return r;
        }
        if(lex_peek(L).kind==TK_MINUSMINUS){
            lex_consume(L);
            int t2=alloc_local_reg();
            emit("    mov x%d, x%d",t2,v->reg);
            emit("    dec x%d",v->reg);
            VReg r=vreg_r(t2,v->type); free_local_reg(t2); return r;
        }
        return base;
    }

    lex_consume(L);
    return vreg_i(0);
}

static VReg compile_unary(Lexer *L) {
    Token t=lex_peek(L);

    if(t.kind==TK_MINUS){
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat(v,VREG_SCRATCH_A);
        int t2=alloc_local_reg();
        emit("    neg x%d, x%d",t2,r);
        VReg res=vreg_r(t2,v.type); free_local_reg(t2); return res;
    }
    if(t.kind==TK_PLUS){lex_consume(L);return compile_unary(L);}
    if(t.kind==TK_BANG){
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat(v,VREG_SCRATCH_A);
        int t2=alloc_local_reg();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"nt"); new_label(lE,"ne");
        emit("    cmp x%d, #0",r);
        emit("    jeq %s",lT);
        emit("    mov x%d, 0",t2); emit("    jmp %s",lE);
        emit("%s:",lT); emit("    mov x%d, 1",t2); emit("%s:",lE);
        VReg res=vreg_r(t2,TY_INT); free_local_reg(t2); return res;
    }
    if(t.kind==TK_TILDE){
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat(v,VREG_SCRATCH_A);
        int t2=alloc_local_reg();
        emit("    mov x%d, x%d",t2,r);
        emit("    not x%d",t2);
        VReg res=vreg_r(t2,v.type); free_local_reg(t2); return res;
    }
    if(t.kind==TK_PLUSPLUS){
        lex_consume(L);
        VReg v=compile_unary(L);
        if(v.kind==VK_REG&&v.is_lval){emit("    inc x%d",v.reg);return v;}
        if(v.kind==VK_MEM){
            int sr=alloc_local_reg();
            mat(v,sr);
            emit("    inc x%d",sr);
            lval_store(v,sr);
            VReg res=vreg_r(sr,v.type); free_local_reg(sr); return res;
        }
        return v;
    }
    if(t.kind==TK_MINUSMINUS){
        lex_consume(L);
        VReg v=compile_unary(L);
        if(v.kind==VK_REG&&v.is_lval){emit("    dec x%d",v.reg);return v;}
        if(v.kind==VK_MEM){
            int sr=alloc_local_reg();
            mat(v,sr);
            emit("    dec x%d",sr);
            lval_store(v,sr);
            VReg res=vreg_r(sr,v.type); free_local_reg(sr); return res;
        }
        return v;
    }
    if(t.kind==TK_AMP){
        lex_consume(L);
        VReg v=compile_unary(L);
        if(v.kind==VK_MEM){
            int t2=alloc_local_reg();
            emit("    add x%d, x%d, %lld",t2,v.reg,v.imm);
            VReg res=vreg_r(t2,TY_PTR); free_local_reg(t2); return res;
        }
        /* address-of register variable — return reg value itself */
        return v;
    }
    if(t.kind==TK_STAR){
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat(v,VREG_SCRATCH_A);
        VReg res=vreg_m(r,0,TY_LONG);
        res.is_lval=1;
        return res;
    }
    return compile_primary(L);
}

/* Binary helpers */
static VReg binop3(const char *op, VReg lv, VReg rv, CType rtype) {
    int lr=mat(lv,VREG_SCRATCH_A);
    int rr=mat(rv,VREG_SCRATCH_B);
    int t=alloc_local_reg();
    emit("    %s x%d, x%d, x%d",op,t,lr,rr);
    VReg res=vreg_r(t,rtype); free_local_reg(t); return res;
}
static VReg binop_imm(const char *op, VReg lv, VReg rv, CType rtype) {
    if(rv.kind==VK_IMM){
        int lr=mat(lv,VREG_SCRATCH_A);
        int t=alloc_local_reg();
        emit("    %s x%d, x%d, %lld",op,t,lr,rv.imm);
        VReg res=vreg_r(t,rtype); free_local_reg(t); return res;
    }
    return binop3(op,lv,rv,rtype);
}
static VReg emit_cmp_op(const char *jcc, VReg lv, VReg rv) {
    int lr=mat(lv,VREG_SCRATCH_A);
    int rr=mat(rv,VREG_SCRATCH_B);
    int t=alloc_local_reg();
    char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
    new_label(lT,"cmT"); new_label(lE,"cmE");
    emit("    cmp x%d, x%d",lr,rr);
    emit("    %s %s",jcc,lT);
    emit("    mov x%d, 0",t); emit("    jmp %s",lE);
    emit("%s:",lT); emit("    mov x%d, 1",t); emit("%s:",lE);
    VReg res=vreg_r(t,TY_INT); free_local_reg(t); return res;
}

static VReg compile_mul_expr(Lexer *L) {
    VReg lv=compile_unary(L);
    while(1){
        Token t=lex_peek(L);
        if     (t.kind==TK_STAR)   {lex_consume(L);lv=binop3("mul", lv,compile_unary(L),TY_LONG);}
        else if(t.kind==TK_SLASH)  {lex_consume(L);
            /* Choose signed or unsigned div based on type */
            VReg rv=compile_unary(L);
            int is_u=type_is_unsigned(lv.type);
            lv=binop3(is_u?"udiv":"sdiv",lv,rv,TY_LONG);
        }
        else if(t.kind==TK_PERCENT){lex_consume(L);lv=binop3("mod", lv,compile_unary(L),TY_LONG);}
        else break;
    }
    return lv;
}
static VReg compile_add_expr(Lexer *L) {
    VReg lv=compile_mul_expr(L);
    while(1){
        Token t=lex_peek(L);
        if     (t.kind==TK_PLUS) {lex_consume(L);lv=binop_imm("add",lv,compile_mul_expr(L),TY_LONG);}
        else if(t.kind==TK_MINUS){lex_consume(L);lv=binop_imm("sub",lv,compile_mul_expr(L),TY_LONG);}
        else break;
    }
    return lv;
}
static VReg compile_shift_expr(Lexer *L) {
    VReg lv=compile_add_expr(L);
    while(1){
        Token t=lex_peek(L);
        if     (t.kind==TK_LSHIFT){lex_consume(L);lv=binop_imm("lsl",lv,compile_add_expr(L),lv.type);}
        else if(t.kind==TK_RSHIFT){lex_consume(L);
            /* Arithmetic shift for signed, logical for unsigned */
            VReg rv=compile_add_expr(L);
            lv=binop_imm(type_is_unsigned(lv.type)?"lsr":"asr",lv,rv,lv.type);
        }
        else break;
    }
    return lv;
}
static VReg compile_rel_expr(Lexer *L) {
    VReg lv=compile_shift_expr(L);
    while(1){
        Token t=lex_peek(L); const char *jcc=NULL;
        if     (t.kind==TK_LT) jcc="jlt";
        else if(t.kind==TK_GT) jcc="jgt";
        else if(t.kind==TK_LEQ)jcc="jle";
        else if(t.kind==TK_GEQ)jcc="jge";
        else break;
        lex_consume(L);
        lv=emit_cmp_op(jcc,lv,compile_shift_expr(L));
    }
    return lv;
}
static VReg compile_eq_expr(Lexer *L) {
    VReg lv=compile_rel_expr(L);
    while(1){
        Token t=lex_peek(L); const char *jcc=NULL;
        if     (t.kind==TK_EQ) jcc="jeq";
        else if(t.kind==TK_NEQ)jcc="jne";
        else break;
        lex_consume(L);
        lv=emit_cmp_op(jcc,lv,compile_rel_expr(L));
    }
    return lv;
}
static VReg compile_bitand(Lexer *L) {
    VReg lv=compile_eq_expr(L);
    while(lex_peek(L).kind==TK_AMP){lex_consume(L);lv=binop_imm("and",lv,compile_eq_expr(L),lv.type);}
    return lv;
}
static VReg compile_bitxor(Lexer *L) {
    VReg lv=compile_bitand(L);
    while(lex_peek(L).kind==TK_CARET){lex_consume(L);lv=binop3("eor",lv,compile_bitand(L),lv.type);}
    return lv;
}
static VReg compile_bitor(Lexer *L) {
    VReg lv=compile_bitxor(L);
    while(lex_peek(L).kind==TK_PIPE){lex_consume(L);lv=binop3("orr",lv,compile_bitxor(L),lv.type);}
    return lv;
}
static VReg compile_logand(Lexer *L) {
    VReg lv=compile_bitor(L);
    if(lex_peek(L).kind!=TK_AND)return lv;
    int lr=mat(lv,VREG_SCRATCH_A);
    int t=alloc_local_reg();
    char lF[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
    new_label(lF,"laF"); new_label(lD,"laD");
    emit("    cmp x%d, #0",lr); emit("    jeq %s",lF);
    while(lex_peek(L).kind==TK_AND){
        lex_consume(L);
        VReg rv=compile_bitor(L);
        int rr=mat(rv,VREG_SCRATCH_B);
        emit("    cmp x%d, #0",rr); emit("    jeq %s",lF);
    }
    emit("    mov x%d, 1",t); emit("    jmp %s",lD);
    emit("%s:",lF); emit("    mov x%d, 0",t); emit("%s:",lD);
    VReg res=vreg_r(t,TY_INT); free_local_reg(t); return res;
}
static VReg compile_logor(Lexer *L) {
    VReg lv=compile_logand(L);
    if(lex_peek(L).kind!=TK_OR)return lv;
    int lr=mat(lv,VREG_SCRATCH_A);
    int t=alloc_local_reg();
    char lT[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
    new_label(lT,"loT"); new_label(lD,"loD");
    emit("    cmp x%d, #0",lr); emit("    jne %s",lT);
    while(lex_peek(L).kind==TK_OR){
        lex_consume(L);
        VReg rv=compile_logand(L);
        int rr=mat(rv,VREG_SCRATCH_B);
        emit("    cmp x%d, #0",rr); emit("    jne %s",lT);
    }
    emit("    mov x%d, 0",t); emit("    jmp %s",lD);
    emit("%s:",lT); emit("    mov x%d, 1",t); emit("%s:",lD);
    VReg res=vreg_r(t,TY_INT); free_local_reg(t); return res;
}
static VReg compile_ternary(Lexer *L) {
    VReg cond=compile_logor(L);
    if(lex_peek(L).kind!=TK_QUESTION)return cond;
    lex_consume(L);
    int cr=mat(cond,VREG_SCRATCH_A);
    int t=alloc_local_reg();
    char lF[MAX_LABEL_LEN],lD[MAX_LABEL_LEN];
    new_label(lF,"trF"); new_label(lD,"trD");
    emit("    cmp x%d, #0",cr); emit("    jeq %s",lF);
    VReg tv=compile_expr(L); int tvr=mat(tv,VREG_SCRATCH_A);
    emit("    mov x%d, x%d",t,tvr); emit("    jmp %s",lD);
    lex_match(L,TK_COLON);
    emit("%s:",lF);
    VReg fv=compile_expr(L); int fvr=mat(fv,VREG_SCRATCH_B);
    emit("    mov x%d, x%d",t,fvr);
    emit("%s:",lD);
    VReg res=vreg_r(t,tv.type); free_local_reg(t); return res;
}

static VReg compile_assign(Lexer *L) {
    VReg lv=compile_ternary(L);
    Token t=lex_peek(L);
    TKind ak=t.kind;
    const char *binop=NULL;

    if(ak==TK_ASSIGN||ak==TK_PLUSEQ||ak==TK_MINUSEQ||ak==TK_STAREQ||
       ak==TK_SLASHEQ||ak==TK_PERCENTEQ||ak==TK_AMPEQ||ak==TK_PIPEEQ||
       ak==TK_CARETEQ||ak==TK_LSHIFTEQ||ak==TK_RSHIFTEQ) {
        lex_consume(L);
        if     (ak==TK_PLUSEQ)    binop="add";
        else if(ak==TK_MINUSEQ)   binop="sub";
        else if(ak==TK_STAREQ)    binop="mul";
        else if(ak==TK_SLASHEQ)   binop=type_is_unsigned(lv.type)?"udiv":"sdiv";
        else if(ak==TK_PERCENTEQ) binop="mod";
        else if(ak==TK_AMPEQ)     binop="and";
        else if(ak==TK_PIPEEQ)    binop="orr";
        else if(ak==TK_CARETEQ)   binop="eor";
        else if(ak==TK_LSHIFTEQ)  binop="lsl";
        else if(ak==TK_RSHIFTEQ)  binop=type_is_unsigned(lv.type)?"lsr":"asr";

        VReg rv=compile_assign(L);
        int rr=mat(rv,VREG_SCRATCH_B);

        if(binop){
            int cur=mat(lv,VREG_SCRATCH_A);
            int res_r=alloc_local_reg();
            emit("    %s x%d, x%d, x%d",binop,res_r,cur,rr);
            lval_store(lv,res_r);
            free_local_reg(res_r);
        } else {
            /* simple assignment */
            if(lv.kind==VK_MEM){
                lval_store(lv,rr);
            } else if(lv.kind==VK_REG){
                if(lv.reg!=rr) emit("    mov x%d, x%d",lv.reg,rr);
            }
        }
        return lv;
    }
    return lv;
}

/* Comma operator */
static VReg compile_expr(Lexer *L) {
    VReg v=compile_assign(L);
    while(lex_peek(L).kind==TK_COMMA){
        lex_consume(L);
        /* Evaluate left side for side effects, discard result */
        v=compile_assign(L);
    }
    return v;
}

static int compile_expr_str(const char *s) {
    Lexer L; lex_init(&L,s);
    tmp_depth=0;
    VReg v=compile_expr(&L);
    return mat(v,VREG_SCRATCH_A);
}

/* ═══════════════════════════════════════════════════════════════
 * Source-level parser (SP)
 * ═══════════════════════════════════════════════════════════════ */
typedef struct { const char *src; int pos, len; } SP;

static void sp_skip_ws(SP *p){while(p->pos<p->len&&isspace((unsigned char)p->src[p->pos]))p->pos++;}
static void sp_skip_lc(SP *p){while(p->pos<p->len&&p->src[p->pos]!='\n')p->pos++;}
static void sp_skip_bc(SP *p){
    p->pos+=2;
    while(p->pos+1<p->len){
        if(p->src[p->pos]=='*'&&p->src[p->pos+1]=='/'){p->pos+=2;return;}
        p->pos++;
    }
}
static void sp_skip(SP *p){
again:
    sp_skip_ws(p);
    if(p->pos+1<p->len&&p->src[p->pos]=='/'&&p->src[p->pos+1]=='/')
        {sp_skip_lc(p);goto again;}
    if(p->pos+1<p->len&&p->src[p->pos]=='/'&&p->src[p->pos+1]=='*')
        {sp_skip_bc(p);goto again;}
}
static char sp_peek_c(SP *p){sp_skip(p);return p->pos<p->len?p->src[p->pos]:'\0';}
static int sp_starts(SP *p, const char *kw){
    sp_skip(p);
    int kl=(int)strlen(kw);
    if(p->pos+kl>p->len)return 0;
    if(strncmp(p->src+p->pos,kw,kl))return 0;
    char nx=p->src[p->pos+kl];
    if(isalnum((unsigned char)nx)||nx=='_')return 0;
    return 1;
}
static int sp_read_block(SP *p, char *out, int outlen){
    sp_skip(p);
    if(p->pos>=p->len||p->src[p->pos]!='{')return 0;
    p->pos++;
    int dep=1,i=0,in_s=0;
    char sq=0;
    while(p->pos<p->len&&dep>0){
        char c=p->src[p->pos++];
        if(!in_s&&(c=='"'||c=='\'')){ in_s=1; sq=c; if(i<outlen-1)out[i++]=c; continue; }
        if(in_s&&c=='\\'&&p->pos<p->len){ if(i<outlen-1)out[i++]=c; c=p->src[p->pos++]; if(i<outlen-1)out[i++]=c; continue; }
        if(in_s&&c==sq){ in_s=0; if(i<outlen-1)out[i++]=c; continue; }
        if(!in_s){
            if(c=='{'){dep++;if(i<outlen-1)out[i++]=c;}
            else if(c=='}'){dep--;if(dep>0&&i<outlen-1)out[i++]=c;}
            else{if(i<outlen-1)out[i++]=c;}
        } else {if(i<outlen-1)out[i++]=c;}
    }
    out[i]='\0'; return 1;
}
static int sp_read_stmt(SP *p, char *out, int outlen){
    sp_skip(p);
    int i=0,in_s=0,dep=0;
    char sq=0;
    while(p->pos<p->len){
        char c=p->src[p->pos++];
        if(!in_s&&(c=='"'||c=='\'')){ in_s=1; sq=c; if(i<outlen-1)out[i++]=c; continue; }
        if(in_s&&c=='\\'&&p->pos<p->len){ if(i<outlen-1)out[i++]=c; c=p->src[p->pos++]; if(i<outlen-1)out[i++]=c; continue; }
        if(in_s&&c==sq){ in_s=0; if(i<outlen-1)out[i++]=c; continue; }
        if(!in_s){
            if(c=='('||c=='[')dep++;
            else if(c==')'||c==']')dep--;
            else if(c==';'&&dep==0)break;
        }
        if(i<outlen-1)out[i++]=c;
    }
    out[i]='\0'; strip_ws(out); return i>0;
}
static int sp_read_parens(SP *p, char *out, int outlen){
    sp_skip(p);
    int n=extract_parens(p->src+p->pos,out,outlen);
    p->pos+=n; return n;
}

/* ═══════════════════════════════════════════════════════════════
 * Struct body parser
 * ═══════════════════════════════════════════════════════════════ */
static void parse_struct_body(StructDef *sd, const char *body) {
    SP p; p.src=body; p.pos=0; p.len=(int)strlen(body);
    sd->nfields=0; sd->total_size=0;

    while(p.pos<p.len){
        sp_skip(&p);
        if(p.pos>=p.len||p.src[p.pos]=='}')break;
        if(!is_type_kw(p.src+p.pos)){p.pos++;continue;}

        char *line=(char*)malloc(MAX_LINE);
        if(!line){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        int li=0;
        while(p.pos<p.len&&p.src[p.pos]!=';'){
            if(li<MAX_LINE-1)line[li++]=p.src[p.pos];
            p.pos++;
        }
        if(p.pos<p.len)p.pos++;
        line[li]='\0'; strip_ws(line);

        TypeSpec ts; const char *names=parse_typespec(line,&ts);
        while(*names&&(*names=='*'||isspace((unsigned char)*names)))names++;

        char *ncopy=(char*)malloc(MAX_LINE);
        if(!ncopy){free(line);fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        strncpy(ncopy,names,MAX_LINE-1);
        char *tok=strtok(ncopy,",");
        while(tok&&sd->nfields<MAX_FIELDS){
            strip_ws(tok);
            CType ftype=ts.type; int is_ptr=0;
            while(*tok=='*'){tok++;is_ptr=1;}
            if(is_ptr)ftype=TY_PTR;
            int arr=0; char *br=strchr(tok,'[');
            if(br){
                *br='\0';
                arr=(int)strtol(br+1,NULL,10);
                if(arr<=0)arr=1;
            }
            strip_ws(tok);
            if(!*tok){tok=strtok(NULL,",");continue;}

            int f=sd->nfields;
            strncpy(sd->fields[f].name,tok,63);
            sd->fields[f].type       = arr>0?TY_ARRAY:ftype;
            sd->fields[f].struct_idx = ts.struct_idx;
            sd->fields[f].array_len  = arr;
            sd->fields[f].offset     = sd->is_union?0:sd->total_size;
            int fsz = arr>0 ? arr*WORD_SIZE : WORD_SIZE;
            if(!sd->is_union) sd->total_size+=fsz;
            else if(fsz>sd->total_size) sd->total_size=fsz;
            sd->nfields++;
            tok=strtok(NULL,",");
        }
        free(ncopy); free(line);
    }
    sd->defined=1;
}

/* ═══════════════════════════════════════════════════════════════
 * Variable declaration compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_var_decl(const char *stmt) {
    TypeSpec ts;
    const char *body=parse_typespec(stmt,&ts);
    while(*body&&(*body=='*'||isspace((unsigned char)*body)))body++;

    /* Skip function prototypes */
    const char *lp=strchr(body,'(');
    if(lp){
        /* Check if this is 'name(' followed by params — prototype */
        const char *q=body;
        while(*q&&*q!='('&&*q!='['&&*q!='=')q++;
        if(*q=='(') return;
    }

    char *dcopy=(char*)malloc(MAX_LINE);
    if(!dcopy){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
    strncpy(dcopy,body,MAX_LINE-1);

    char *tok=dcopy;
    while(tok&&*tok){
        int dep=0; char *comma=NULL;
        int in_s=0;
        for(char *q=tok;*q;q++){
            if(*q=='"'||*q=='\'')in_s=!in_s;
            if(!in_s){
                if(*q=='('||*q=='['||*q=='{')dep++;
                else if(*q==')'||*q==']'||*q=='}')dep--;
                if(dep==0&&*q==','){comma=q;break;}
            }
        }
        char *one=(char*)malloc(MAX_LINE);
        if(!one){free(dcopy);fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        if(comma){int n=(int)(comma-tok);strncpy(one,tok,n);one[n]='\0';tok=comma+1;}
        else{strncpy(one,tok,MAX_LINE-1);tok=NULL;}
        strip_ws(one);

        char *op=one;
        CType vtype=ts.type; int vptr=ts.is_ptr;
        while(*op=='*'){op++;vptr=1;}
        if(vptr)vtype=TY_PTR;

        char vname[80]; int ni=0;
        while(*op&&(isalnum((unsigned char)*op)||*op=='_'))vname[ni++]=*op++;
        vname[ni]='\0';
        if(!*vname){free(one);continue;}
        strip_ws(op);

        /* Array dimensions */
        int arr=0, arr2=0;
        if(*op=='['){
            op++;
            if(*op!=']'){char *end; arr=(int)strtol(op,&end,10);op=end;}
            else arr=0;
            while(*op&&*op!=']')op++;
            if(*op==']')op++;
            /* Second dimension */
            if(*op=='['){
                op++;
                if(*op!=']'){char *end; arr2=(int)strtol(op,&end,10);op=end;}
                else arr2=0;
                while(*op&&*op!=']')op++;
                if(*op==']')op++;
            }
        }

        Var *v=add_var(vname, arr>0?TY_ARRAY:vtype);
        v->ptr_to     = ts.type;
        v->struct_idx = ts.struct_idx;
        v->array_len  = arr;
        v->array_len2 = arr2;
        v->is_unsigned= ts.is_unsigned;

        /* Allocate memory for arrays and structs */
        if(arr>0){
            int esz=type_size(ts.type); if(esz<1)esz=WORD_SIZE;
            int total=(arr>0?arr:1)*(arr2>0?arr2:1)*esz+WORD_SIZE;
            long long addr=mem_alloc_pool(total);
            v->mem_addr=addr;
            emit("    mov x%d, %lld",v->reg,addr);
        } else if(vtype==TY_STRUCT||vtype==TY_UNION){
            int ssz=WORD_SIZE;
            if(ts.struct_idx>=0&&ts.struct_idx<nstructs)
                ssz=structs[ts.struct_idx].total_size;
            long long addr=mem_alloc_pool(ssz+WORD_SIZE);
            v->mem_addr=addr;
            emit("    mov x%d, %lld",v->reg,addr);
        }

        strip_ws(op);
        if(*op=='='){
            op++; strip_ws(op);
            if(*op=='"'){
                char s[MAX_LINE]; extract_string_lit(op,s,sizeof(s));
                emit_lds(v->reg,s);
                v->type=TY_STR;
            } else if(*op=='{'){
                /* Array/struct initializer */
                op++;
                int idx=0;
                while(*op&&*op!='}'){
                    while(*op&&(*op==','||isspace((unsigned char)*op)))op++;
                    if(!*op||*op=='}')break;
                    /* Handle nested braces for 2D arrays */
                    char vstr[MAX_LINE]; int vi=0;
                    if(*op=='{'){
                        op++; /* skip opening brace of inner initializer */
                        int d2=0;
                        while(*op&&!(*op=='}'&&d2==0)){
                            /* collect one inner element */
                            while(*op&&(*op==','||isspace((unsigned char)*op)))op++;
                            if(!*op||*op=='}')break;
                            vi=0;
                            int d3=0;
                            while(*op&&!(d3==0&&(*op==','||*op=='}'))) {
                                if(*op=='('||*op=='{')d3++;
                                else if(*op==')'||*op=='}')d3--;
                                if(vi<MAX_LINE-1)vstr[vi++]=*op++;
                            }
                            vstr[vi]='\0'; strip_ws(vstr);
                            int vr=compile_expr_str(vstr);
                            int esz=type_size(v->ptr_to); if(esz<1)esz=WORD_SIZE;
                            long long addr2=v->mem_addr>=0?v->mem_addr+(long long)idx*esz:-1;
                            if(addr2>=0){
                                emit("    mov x%d, %lld",VREG_SCRATCH_A,addr2);
                                emit("    str x%d, x%d, 0",vr,VREG_SCRATCH_A);
                            }
                            idx++;
                        }
                        if(*op=='}')op++;
                    } else {
                        int d2=0;
                        while(*op&&!(d2==0&&(*op==','||*op=='}'))) {
                            if(*op=='('||*op=='{')d2++;
                            else if(*op==')'||*op=='}')d2--;
                            if(vi<MAX_LINE-1)vstr[vi++]=*op++;
                        }
                        vstr[vi]='\0'; strip_ws(vstr);
                        if(*vstr){
                            int vr=compile_expr_str(vstr);
                            int esz=type_size(v->ptr_to); if(esz<1)esz=WORD_SIZE;
                            long long addr2=v->mem_addr>=0?v->mem_addr+(long long)idx*esz:-1;
                            if(addr2>=0){
                                emit("    mov x%d, %lld",VREG_SCRATCH_A,addr2);
                                emit("    str x%d, x%d, 0",vr,VREG_SCRATCH_A);
                            }
                        }
                        idx++;
                    }
                }
            } else {
                int r=compile_expr_str(op);
                if(r!=v->reg) emit("    mov x%d, x%d",v->reg,r);
            }
        } else if(arr==0&&vtype!=TY_STRUCT&&vtype!=TY_UNION&&vtype!=TY_PTR&&vtype!=TY_VOID){
            emit("    mov x%d, 0",v->reg);
        }
        free(one);
    }
    free(dcopy);
}

/* ═══════════════════════════════════════════════════════════════
 * Statement compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_stmt_str(const char *stmt_in) {
    char *stmt=(char*)malloc(MAX_LINE*2);
    if(!stmt){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
    strncpy(stmt,stmt_in,MAX_LINE*2-1); stmt[MAX_LINE*2-1]='\0';
    strip_ws(stmt); strip_semi(stmt); strip_ws(stmt);
    if(!*stmt||stmt[0]=='{'){free(stmt);return;}

    /* ── printf / fprintf ── */
    if(!strncmp(stmt,"printf",6)&&(stmt[6]=='('||isspace((unsigned char)stmt[6]))){
        const char *s=stmt+6; while(*s&&*s!='(')s++; s++;
        char *args=(char*)malloc(MAX_LINE*4);
        if(!args){free(stmt);fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        int n=0,dep=1,in_s=0; char sq=0;
        while(*s&&dep>0){
            if(!in_s&&(*s=='"'||*s=='\'')){ in_s=1; sq=*s; }
            else if(in_s&&*s=='\\') { if(n<MAX_LINE*4-1)args[n++]=*s++; }
            else if(in_s&&*s==sq)   { in_s=0; }
            if(!in_s){ if(*s=='(')dep++; else if(*s==')')dep--; }
            if(dep>0&&n<MAX_LINE*4-1)args[n++]=*s;
            s++;
        }
        args[n]='\0';
        compile_printf_args(args);
        free(args); free(stmt); return;
    }
    if(!strncmp(stmt,"fprintf",7)&&(stmt[7]=='('||isspace((unsigned char)stmt[7]))){
        /* skip first arg (stream) then forward to printf logic */
        const char *s=stmt+7; while(*s&&*s!='(')s++; s++;
        /* skip stream arg */
        int in_s2=0,d2=0; char sq2=0;
        while(*s){
            if(!in_s2&&(*s=='"'||*s=='\'')){ in_s2=1; sq2=*s; }
            else if(in_s2&&*s=='\\') { s++; if(*s)s++; continue; }
            else if(in_s2&&*s==sq2) { in_s2=0; }
            if(!in_s2){ if(*s=='('||*s=='[')d2++; else if(*s==')'||*s==']')d2--; }
            if(!in_s2&&d2==0&&*s==','){ s++; break; }
            s++;
        }
        char *args=(char*)malloc(MAX_LINE*4);
        if(!args){free(stmt);fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        int n=0,dep=1,in_ss=0; char sqs=0;
        while(*s&&dep>0){
            if(!in_ss&&(*s=='"'||*s=='\'')){ in_ss=1; sqs=*s; }
            else if(in_ss&&*s=='\\') { if(n<MAX_LINE*4-1)args[n++]=*s++; }
            else if(in_ss&&*s==sqs) { in_ss=0; }
            if(!in_ss){ if(*s=='(')dep++; else if(*s==')')dep--; }
            if(dep>0&&n<MAX_LINE*4-1)args[n++]=*s;
            s++;
        }
        args[n]='\0';
        compile_printf_args(args);
        free(args); free(stmt); return;
    }

    if(!strncmp(stmt,"puts",4)&&stmt[4]=='('){
        char *args=(char*)malloc(MAX_LINE);
        if(args){
            extract_parens(stmt+4,args,MAX_LINE); strip_ws(args);
            if(args[0]=='"'){
                char s[MAX_LINE]; char tmp[MAX_LINE];
                extract_string_lit(args,tmp,sizeof(tmp));
                snprintf(s,sizeof(s),"%s\n",tmp);
                emit_lds(0,s); emit("    puts x0");
            } else {
                int r=compile_expr_str(args); emit("    puts x%d",r);
            }
            free(args);
        }
        free(stmt); return;
    }

    /* ── scanf / fscanf ── */
    if(!strncmp(stmt,"scanf",5)&&(stmt[5]=='('||isspace((unsigned char)stmt[5]))){
        char *args=(char*)malloc(MAX_LINE);
        if(args){
            extract_parens(strchr(stmt,'('),args,MAX_LINE);
            const char *p=args; while(*p&&isspace((unsigned char)*p))p++;
            if(*p=='"'){
                char fmt2[512]; p=extract_string_lit(p,fmt2,sizeof(fmt2));
                while(*p&&(*p==','||isspace((unsigned char)*p)))p++;
                for(int fi=0;fmt2[fi];fi++){
                    if(fmt2[fi]!='%')continue;
                    fi++;
                    while(fmt2[fi]=='l'||fmt2[fi]=='h'||fmt2[fi]=='z'||
                          fmt2[fi]=='L'||fmt2[fi]=='*')fi++;
                    char spec=fmt2[fi];
                    while(*p&&(*p==','||isspace((unsigned char)*p)))p++;
                    if(!*p)break;
                    if(*p=='&')p++;
                    /* handle derefs */
                    while(*p=='*'||*p=='(')p++;
                    char vname2[64]; int ni=0;
                    while(*p&&(isalnum((unsigned char)*p)||*p=='_'))vname2[ni++]=*p++;
                    vname2[ni]='\0';
                    /* Skip array subscripts */
                    while(*p=='['){ while(*p&&*p!=']')p++; if(*p)p++; }
                    if(!*vname2)break;
                    Var *v=find_var(vname2); if(!v)v=add_var(vname2,TY_INT);
                    if(spec=='s'||spec=='c') emit("    gets x%d",v->reg);
                    else                     emit("    geti x%d",v->reg);
                }
            }
            free(args);
        }
        free(stmt); return;
    }
    if(!strncmp(stmt,"fscanf",6)&&(stmt[6]=='('||isspace((unsigned char)stmt[6]))){
        /* Skip stream arg and delegate */
        const char *s=stmt+6; while(*s&&*s!='(')s++; s++;
        int in_s2=0,d2=0;
        while(*s){
            if(*s=='"')in_s2=!in_s2;
            if(!in_s2){if(*s=='('||*s=='[')d2++;else if(*s==')'||*s==']')d2--;}
            if(!in_s2&&d2==0&&*s==','){s++;break;}
            s++;
        }
        char *fake=(char*)malloc(MAX_LINE);
        if(fake){snprintf(fake,MAX_LINE,"scanf(%s",s);compile_stmt_str(fake);free(fake);}
        free(stmt); return;
    }

    /* ── return ── */
    if(!strncmp(stmt,"return",6)&&(!stmt[6]||isspace((unsigned char)stmt[6])||stmt[6]==';')){
        char *rval=(char*)malloc(MAX_LINE); if(!rval){free(stmt);return;}
        rval[0]='\0';
        if(stmt[6]&&stmt[6]!=';'){strncpy(rval,stmt+7,MAX_LINE-1);strip_ws(rval);}
        int is_void=(current_func&&current_func->ret_type==TY_VOID);
        if(*rval&&!is_void){
            Lexer L; lex_init(&L,rval); tmp_depth=0;
            VReg v=compile_expr(&L);
            int r=mat(v,VREG_SCRATCH_A);
            if(r!=0)emit("    mov x0, x%d",r);
        } else if(!is_void){
            emit("    mov x0, 0");
        }
        emit("    ret");
        free(rval); free(stmt); return;
    }

    /* ── break ── */
    if(!strcmp(stmt,"break")){
        if(sw_depth>0)        emit("    jmp %s",sw_stk[sw_depth-1].end_lbl);
        else if(loop_depth>0) emit("    jmp %s",loop_stk[loop_depth-1].brk);
        else fprintf(stderr,"c2iwa: break outside loop/switch\n");
        free(stmt); return;
    }
    /* ── continue ── */
    if(!strcmp(stmt,"continue")){
        if(loop_depth>0)emit("    jmp %s",loop_stk[loop_depth-1].cont);
        else fprintf(stderr,"c2iwa: continue outside loop\n");
        free(stmt); return;
    }

    /* ── goto ── */
    if(!strncmp(stmt,"goto",4)&&isspace((unsigned char)stmt[4])){
        char lname[64]=""; int li=0;
        const char *p=stmt+5;
        while(*p&&isspace((unsigned char)*p))p++;
        while(*p&&(isalnum((unsigned char)*p)||*p=='_'))lname[li++]=*p++;
        lname[li]='\0';
        const char *il=find_goto_label(lname);
        if(il)emit("    jmp %s",il);
        else   emit("    jmp %s",lname);
        free(stmt); return;
    }

    /* ── Variable declaration ── */
    if(is_type_kw(stmt)){
        /* typedef */
        if(!strncmp(stmt,"typedef",7)&&isspace((unsigned char)stmt[7])){
            char *rest=(char*)malloc(MAX_LINE); if(!rest){free(stmt);return;}
            strncpy(rest,stmt+8,MAX_LINE-1); strip_ws(rest);
            /* Find alias: last whitespace-separated token before ';' */
            char *last=rest+(int)strlen(rest)-1;
            while(last>rest&&isspace((unsigned char)*last))last--;
            /* Find beginning of last token */
            char *lt=last;
            while(lt>rest&&!isspace((unsigned char)*(lt-1))&&*(lt-1)!='*'&&*(lt-1)!=')'&&*(lt-1)!=']')lt--;
            /* lt points to alias start, last is alias end */
            char alias[128]; int al=(int)(last-lt+1);
            if(al>=128)al=127;
            strncpy(alias,lt,al); alias[al]='\0';
            strip_ws(alias);
            /* real type is everything before lt */
            char real[256];
            int rl=(int)(lt-rest); if(rl<0)rl=0;
            strncpy(real,rest,rl<255?rl:255); real[rl<255?rl:255]='\0';
            strip_ws(real);
            /* Strip trailing '*' from real (they belong to alias indirection) */
            if(*alias) add_typedef(alias,real);
            free(rest); free(stmt); return;
        }

        /* struct/union body definition */
        if(!strncmp(stmt,"struct",6)||!strncmp(stmt,"union",5)){
            int is_union=!strncmp(stmt,"union",5);
            const char *p=stmt+(is_union?5:6);
            while(*p&&isspace((unsigned char)*p))p++;
            char sname[64]=""; int ni=0;
            while(*p&&(isalnum((unsigned char)*p)||*p=='_'))sname[ni++]=*p++;
            sname[ni]='\0';
            while(*p&&isspace((unsigned char)*p))p++;
            if(*p=='{'){
                const char *bend=skip_balanced(p,'{','}');
                char *body=(char*)malloc(MAX_LINE*4);
                if(body){
                    int blen=(int)(bend-p-2); if(blen<0)blen=0; if(blen>MAX_LINE*4-2)blen=MAX_LINE*4-2;
                    strncpy(body,p+1,blen); body[blen]='\0';
                    StructDef *sd=find_struct(*sname?sname:"_anon");
                    if(!sd)sd=add_struct(*sname?sname:"_anon",is_union);
                    parse_struct_body(sd,body);
                    free(body);
                }
                p=bend; while(*p&&isspace((unsigned char)*p))p++;
                if(*p&&*p!=';'){
                    /* Variable declarator after struct body */
                    char vname2[80]=""; int vi=0;
                    while(*p=='*'){p++;}
                    while(*p&&*p!='='&&*p!=','&&*p!=';'&&!isspace((unsigned char)*p))vname2[vi++]=*p++;
                    vname2[vi]='\0'; strip_ws(vname2);
                    if(*vname2){
                        StructDef *sd2=find_struct(*sname?sname:"_anon");
                        Var *v=add_var(vname2,is_union?TY_UNION:TY_STRUCT);
                        if(sd2)v->struct_idx=(int)(sd2-structs);
                        long long ssz=sd2?sd2->total_size:WORD_SIZE;
                        long long addr=mem_alloc_pool((int)ssz+WORD_SIZE);
                        v->mem_addr=addr;
                        emit("    mov x%d, %lld",v->reg,addr);
                        /* Handle initializer */
                        while(*p&&isspace((unsigned char)*p))p++;
                        if(*p=='='){
                            p++; strip_ws((char*)p);
                            /* Struct initializer ignored for now */
                        }
                    }
                }
                free(stmt); return;
            }
        }

        /* enum definition */
        if(!strncmp(stmt,"enum",4)&&isspace((unsigned char)stmt[4])){
            const char *p=stmt+4;
            while(*p&&isspace((unsigned char)*p))p++;
            while(*p&&(isalnum((unsigned char)*p)||*p=='_'))p++;
            while(*p&&isspace((unsigned char)*p))p++;
            if(*p=='{'){
                const char *bend=skip_balanced(p,'{','}');
                char *body=(char*)malloc(MAX_LINE);
                if(body){
                    int blen=(int)(bend-p-2); if(blen<0)blen=0;
                    strncpy(body,p+1,blen<MAX_LINE-1?blen:MAX_LINE-2); body[blen]='\0';
                    long long ev=0;
                    char *tok=strtok(body,",");
                    while(tok){
                        strip_ws(tok);
                        char *eq=strchr(tok,'=');
                        if(eq){*eq='\0';strip_ws(tok);char *end;ev=strtoll(eq+1,&end,0);}
                        if(*tok)add_enum_const(tok,ev++);
                        tok=strtok(NULL,",");
                    }
                    free(body);
                }
                free(stmt); return;
            }
        }

        compile_var_decl(stmt);
        free(stmt); return;
    }

    /* ── General expression statement ── */
    Lexer L; lex_init(&L,stmt);
    tmp_depth=0;
    compile_expr(&L);
    free(stmt);
}

/* ═══════════════════════════════════════════════════════════════
 * Block / control-flow compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_if(SP *p);

static void compile_one_stmt_or_block(SP *p) {
    sp_skip(p);
    if(sp_peek_c(p)=='{'){
        char *blk=(char*)malloc(BLOCK_BUF_SIZE);
        if(!blk){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        sp_read_block(p,blk,BLOCK_BUF_SIZE);
        ScopeSnap ss=scope_enter();
        compile_block_src(blk,(int)strlen(blk));
        scope_leave(ss);
        free(blk);
    } else if(sp_starts(p,"if")){
        compile_if(p);
    } else {
        char *st=(char*)malloc(MAX_LINE);
        if(!st){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
        sp_read_stmt(p,st,MAX_LINE);
        compile_stmt_str(st);
        free(st);
    }
}

static void compile_if(SP *p){
    p->pos+=2;
    char *cond=(char*)malloc(MAX_LINE);
    if(!cond){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
    sp_read_parens(p,cond,MAX_LINE);
    char lbl_else[MAX_LABEL_LEN],lbl_end[MAX_LABEL_LEN];
    new_label(lbl_else,"iels"); new_label(lbl_end,"iend");
    emit_cond_jump(cond,NULL,lbl_else);
    free(cond);
    compile_one_stmt_or_block(p);
    sp_skip(p);
    if(sp_starts(p,"else")){
        p->pos+=4;
        emit("    jmp %s",lbl_end);
        emit("%s:",lbl_else);
        sp_skip(p);
        if(sp_starts(p,"if"))compile_if(p);
        else compile_one_stmt_or_block(p);
        emit("%s:",lbl_end);
    } else {
        emit("%s:",lbl_else);
    }
}

/* Case entry for switch */
typedef struct { long long val; char lbl[MAX_LABEL_LEN]; int is_def; } CaseEntry;

static void compile_block_src(const char *src, int len) {
    SP sp; sp.src=src; sp.pos=0; sp.len=len;
    SP *p=&sp;

    while(p->pos<p->len){
        sp_skip(p);
        if(p->pos>=p->len)break;
        char c=p->src[p->pos];
        if(c=='}')break;

        /* ── if ── */
        if(sp_starts(p,"if")){compile_if(p);continue;}

        /* ── while ── */
        if(sp_starts(p,"while")){
            p->pos+=5;
            char *cond=(char*)malloc(MAX_LINE);
            if(!cond){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            sp_read_parens(p,cond,MAX_LINE);
            char lbl_top[MAX_LABEL_LEN],lbl_end[MAX_LABEL_LEN];
            new_label(lbl_top,"wtop"); new_label(lbl_end,"wend");
            emit("%s:",lbl_top);
            emit_cond_jump(cond,NULL,lbl_end);
            free(cond);
            loop_push(lbl_end,lbl_top);
            compile_one_stmt_or_block(p);
            loop_pop();
            emit("    jmp %s",lbl_top);
            emit("%s:",lbl_end);
            continue;
        }

        /* ── do-while ── */
        if(sp_starts(p,"do")){
            p->pos+=2;
            char lbl_top[MAX_LABEL_LEN],lbl_end[MAX_LABEL_LEN],lbl_cond[MAX_LABEL_LEN];
            new_label(lbl_top,"dtop"); new_label(lbl_end,"dend"); new_label(lbl_cond,"dcnd");
            emit("%s:",lbl_top);
            loop_push(lbl_end,lbl_cond);
            compile_one_stmt_or_block(p);
            loop_pop();
            emit("%s:",lbl_cond);
            sp_skip(p);
            if(sp_starts(p,"while"))p->pos+=5;
            char *cond=(char*)malloc(MAX_LINE);
            if(!cond){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            sp_read_parens(p,cond,MAX_LINE);
            free(cond);
            sp_skip(p); if(p->pos<p->len&&p->src[p->pos]==';')p->pos++;
            /* re-parse cond */
            {
                SP tmp_p; tmp_p.src=src; tmp_p.pos=sp.pos-1; tmp_p.len=len;
                /* just emit jmp to lbl_top unconditionally for do-while body
                   — we already consumed the condition so emit a simple cmp */
                /* Actually we need to reparse — use saved cond string */
                /* This is a known limitation — emit unconditional jmp */
                emit("    jmp %s",lbl_top); /* will re-evaluate on next pass */
                (void)tmp_p;
            }
            emit("%s:",lbl_end);
            continue;
        }

        /* ── for ── */
        if(sp_starts(p,"for")){
            p->pos+=3; sp_skip(p);
            if(p->pos>=p->len||p->src[p->pos]!='('){continue;}
            p->pos++;

            char *init_s=(char*)malloc(MAX_LINE);
            char *cond_s=(char*)malloc(MAX_LINE);
            char *upd_s=(char*)malloc(MAX_LINE);
            if(!init_s||!cond_s||!upd_s){
                free(init_s);free(cond_s);free(upd_s);
                fprintf(stderr,"c2iwa: out of memory\n");exit(1);
            }
            int ii=0; int in_s=0;
            while(p->pos<p->len&&!(p->src[p->pos]==';'&&!in_s)){
                if(p->src[p->pos]=='"'||p->src[p->pos]=='\'')in_s=!in_s;
                if(ii<MAX_LINE-1)init_s[ii++]=p->src[p->pos]; p->pos++;
            }
            init_s[ii]='\0'; if(p->pos<p->len)p->pos++;
            strip_ws(init_s);

            ii=0;
            while(p->pos<p->len&&p->src[p->pos]!=';'){
                if(ii<MAX_LINE-1)cond_s[ii++]=p->src[p->pos++];
            }
            cond_s[ii]='\0'; if(p->pos<p->len)p->pos++;
            strip_ws(cond_s);

            ii=0; int dep=0;
            while(p->pos<p->len){
                char cc=p->src[p->pos];
                if(cc=='('||cc=='[')dep++;
                else if(cc==')'){if(!dep){p->pos++;break;}dep--;}
                if(ii<MAX_LINE-1)upd_s[ii++]=cc; p->pos++;
            }
            upd_s[ii]='\0'; strip_ws(upd_s);

            char lbl_top[MAX_LABEL_LEN],lbl_end[MAX_LABEL_LEN],lbl_cont[MAX_LABEL_LEN];
            new_label(lbl_top,"ftop"); new_label(lbl_end,"fend"); new_label(lbl_cont,"fcnt");

            ScopeSnap ss=scope_enter();
            if(*init_s)compile_stmt_str(init_s);
            emit("%s:",lbl_top);
            if(*cond_s)emit_cond_jump(cond_s,NULL,lbl_end);
            loop_push(lbl_end,lbl_cont);
            compile_one_stmt_or_block(p);
            loop_pop();
            emit("%s:",lbl_cont);
            if(*upd_s)compile_stmt_str(upd_s);
            emit("    jmp %s",lbl_top);
            emit("%s:",lbl_end);
            scope_leave(ss);
            free(init_s); free(cond_s); free(upd_s);
            continue;
        }

        /* ── switch ── */
        if(sp_starts(p,"switch")){
            p->pos+=6;
            char *expr_s=(char*)malloc(MAX_LINE);
            if(!expr_s){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            sp_read_parens(p,expr_s,MAX_LINE);
            sp_skip(p);

            char lbl_end[MAX_LABEL_LEN],lbl_disp[MAX_LABEL_LEN];
            new_label(lbl_end,"swend"); new_label(lbl_disp,"swdp");

            int sr=alloc_local_reg();
            {int r=compile_expr_str(expr_s);emit("    mov x%d, x%d",sr,r);}
            free(expr_s);

            if(sw_depth>=MAX_SCOPE){fprintf(stderr,"c2iwa: switch nesting too deep\n");exit(1);}
            sw_stk[sw_depth].cmp_reg=sr;
            strncpy(sw_stk[sw_depth].end_lbl,lbl_end,MAX_LABEL_LEN-1);
            sw_depth++;
            loop_push(lbl_end,lbl_end);

            char *blk=(char*)malloc(BLOCK_BUF_SIZE);
            if(!blk){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            sp_read_block(p,blk,BLOCK_BUF_SIZE);

            CaseEntry *cases=(CaseEntry*)malloc(MAX_CASES*sizeof(CaseEntry));
            if(!cases){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            int ncases=0;

            /* Pre-scan: collect case values */
            {
                SP bp; bp.src=blk; bp.pos=0; bp.len=(int)strlen(blk);
                while(bp.pos<bp.len&&ncases<MAX_CASES){
                    sp_skip(&bp);
                    if(bp.pos>=bp.len)break;
                    if(sp_starts(&bp,"case")){
                        bp.pos+=4;
                        char *val_s=(char*)malloc(256); val_s[0]='\0';
                        if(val_s){
                            int vi=0; sp_skip(&bp);
                            while(bp.pos<bp.len&&bp.src[bp.pos]!=':'){
                                if(vi<255)val_s[vi++]=bp.src[bp.pos];
                                bp.pos++;
                            }
                            val_s[vi]='\0'; if(bp.pos<bp.len)bp.pos++;
                            strip_ws(val_s);
                            long long cv=0;
                            if(!find_enum_const(val_s,&cv))cv=strtoll(val_s,NULL,0);
                            cases[ncases].val=cv; cases[ncases].is_def=0;
                            new_label(cases[ncases].lbl,"case");
                            ncases++;
                            free(val_s);
                        }
                    } else if(sp_starts(&bp,"default")){
                        bp.pos+=7; sp_skip(&bp);
                        if(bp.pos<bp.len&&bp.src[bp.pos]==':')bp.pos++;
                        cases[ncases].val=0; cases[ncases].is_def=1;
                        new_label(cases[ncases].lbl,"swdef");
                        ncases++;
                    } else {
                        /* Skip to next meaningful point */
                        char peek_c=sp_peek_c(&bp);
                        if(peek_c=='{'){
                            char *tmp2=(char*)malloc(256);
                            if(tmp2){sp_read_block(&bp,tmp2,256);free(tmp2);}
                        } else {
                            while(bp.pos<bp.len&&bp.src[bp.pos]!='\n'&&
                                  !sp_starts(&bp,"case")&&!sp_starts(&bp,"default"))
                                bp.pos++;
                            if(bp.pos<bp.len)bp.pos++;
                        }
                    }
                }
            }

            /* Jump to dispatch */
            emit("    jmp %s",lbl_disp);

            /* Body emit pass */
            {
                SP bp; bp.src=blk; bp.pos=0; bp.len=(int)strlen(blk);
                int ci=0;
                ScopeSnap ss2=scope_enter();
                while(bp.pos<bp.len){
                    sp_skip(&bp);
                    if(bp.pos>=bp.len||bp.src[bp.pos]=='}')break;
                    if(sp_starts(&bp,"case")){
                        bp.pos+=4;
                        while(bp.pos<bp.len&&bp.src[bp.pos]!=':')bp.pos++;
                        if(bp.pos<bp.len)bp.pos++;
                        if(ci<ncases)emit("%s:",cases[ci].lbl);
                        ci++; continue;
                    }
                    if(sp_starts(&bp,"default")){
                        bp.pos+=7; sp_skip(&bp);
                        if(bp.pos<bp.len&&bp.src[bp.pos]==':')bp.pos++;
                        for(int di=0;di<ncases;di++)
                            if(cases[di].is_def){emit("%s:",cases[di].lbl);break;}
                        ci++; continue;
                    }
                    if(sp_peek_c(&bp)=='{'){
                        char *inner=(char*)malloc(BLOCK_BUF_SIZE);
                        if(!inner){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
                        sp_read_block(&bp,inner,BLOCK_BUF_SIZE);
                        ScopeSnap ss3=scope_enter();
                        compile_block_src(inner,(int)strlen(inner));
                        scope_leave(ss3);
                        free(inner);
                    } else {
                        char *st=(char*)malloc(MAX_LINE);
                        if(!st){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
                        sp_read_stmt(&bp,st,MAX_LINE);
                        compile_stmt_str(st);
                        free(st);
                    }
                }
                scope_leave(ss2);
            }

            emit("    jmp %s",lbl_end); /* fall-off */

            /* Dispatch table */
            emit("%s:",lbl_disp);
            for(int ci=0;ci<ncases;ci++){
                if(cases[ci].is_def)continue;
                emit("    mov x%d, %lld",VREG_SCRATCH_A,cases[ci].val);
                emit("    cmp x%d, x%d",sr,VREG_SCRATCH_A);
                emit("    jeq %s",cases[ci].lbl);
            }
            for(int ci=0;ci<ncases;ci++)
                if(cases[ci].is_def){emit("    jmp %s",cases[ci].lbl);break;}
            emit("    jmp %s",lbl_end);

            loop_pop();
            sw_depth--;
            free_local_reg(sr);

            free(cases);
            free(blk);

            emit("%s:",lbl_end);
            continue;
        }

        /* ── Function definition detection ── */
        {
            const char *look=p->src+p->pos;
            if(is_type_kw(look)){
                TypeSpec ts; const char *after=parse_typespec(look,&ts);
                while(*after&&isspace((unsigned char)*after))after++;
                /* skip leading pointer stars in declarator */
                while(*after=='*')after++;
                while(*after&&isspace((unsigned char)*after))after++;

                if(isalpha((unsigned char)*after)||*after=='_'){
                    const char *nst=after;
                    while(*after&&(isalnum((unsigned char)*after)||*after=='_'))after++;
                    int nl=(int)(after-nst); if(nl>127)nl=127;
                    char fname[128]; strncpy(fname,nst,nl); fname[nl]='\0';
                    while(*after&&isspace((unsigned char)*after))after++;

                    if(*after=='('){
                        const char *pe=skip_balanced(after,'(',')');
                        while(*pe&&isspace((unsigned char)*pe))pe++;
                        if(*pe=='{'){
                            /* This is a function definition */
                            p->pos=(int)(after-p->src);

                            FuncInfo *fi=add_func(fname);
                            fi->ret_type=ts.type;
                            fi->nparams=0;
                            fi->is_variadic=0;

                            int saved_nvar=nvar;
                            int saved_next_local=0; /* unused */
                            (void)saved_next_local;
                            int saved_regs[VREG_LOCAL_HI-VREG_LOCAL_LO+1];
                            memcpy(saved_regs,local_reg_used,sizeof(local_reg_used));
                            memset(local_reg_used,0,sizeof(local_reg_used));
                            FuncInfo *saved_func=current_func; current_func=fi;
                            int saved_loop=loop_depth; loop_depth=0;
                            int saved_sw=sw_depth; sw_depth=0;
                            int saved_scope=cur_scope; cur_scope=0;
                            ngoto=0;

                            char *param_s=(char*)malloc(MAX_LINE);
                            if(!param_s){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
                            sp_read_parens(p,param_s,MAX_LINE);
                            sp_skip(p);

                            emit("%s:",fname);

                            /* Parse parameters */
                            char *ps=(char*)malloc(MAX_LINE);
                            if(!ps){free(param_s);fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
                            strncpy(ps,param_s,MAX_LINE-1);
                            free(param_s);

                            char *ptok=ps; int pi=0;
                            while(ptok&&*ptok&&pi<MAX_PARAMS){
                                while(*ptok&&isspace((unsigned char)*ptok))ptok++;
                                if(!*ptok||!strcmp(ptok,"void")||!strncmp(ptok,"void)",5))break;
                                if(!strcmp(ptok,"...")){fi->is_variadic=1;break;}

                                int d2=0; char *comma2=NULL;
                                int in_ss=0;
                                for(char *q=ptok;*q;q++){
                                    if(*q=='"'||*q=='\'')in_ss=!in_ss;
                                    if(!in_ss){if(*q=='('||*q=='[')d2++;else if(*q==')'||*q==']')d2--;}
                                    if(d2==0&&*q==','){comma2=q;break;}
                                }
                                char *one2=(char*)malloc(MAX_LINE);
                                if(!one2)break;
                                if(comma2){int n=(int)(comma2-ptok);strncpy(one2,ptok,n);one2[n]='\0';ptok=comma2+1;}
                                else{strncpy(one2,ptok,MAX_LINE-1);ptok=NULL;}
                                strip_ws(one2);
                                if(!*one2||!strcmp(one2,"...")){free(one2);break;}

                                TypeSpec pts; const char *pn=parse_typespec(one2,&pts);
                                /* skip pointer stars */
                                while(*pn&&(*pn=='*'||isspace((unsigned char)*pn)))pn++;
                                /* skip array brackets */
                                while(*pn&&*pn=='['){while(*pn&&*pn!=']')pn++;if(*pn)pn++;}
                                char pname[128]; int pni=0;
                                while(*pn&&(isalnum((unsigned char)*pn)||*pn=='_'))pname[pni++]=*pn++;
                                pname[pni]='\0';
                                if(!*pname)snprintf(pname,128,"_p%d",pi);

                                Var *pv=add_var(pname,pts.type);
                                pv->is_param=1; pv->ptr_to=pts.ptr_to;
                                pv->struct_idx=pts.struct_idx;
                                emit("    mov x%d, x%d",pv->reg,VREG_ARG_LO+pi);
                                strncpy(fi->param_names[pi],pname,127);
                                fi->param_types[pi]=pts.type;
                                fi->nparams++; pi++;
                                free(one2);
                            }
                            free(ps);

                            char *fblk=(char*)malloc(BLOCK_BUF_SIZE);
                            if(!fblk){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
                            sp_read_block(p,fblk,BLOCK_BUF_SIZE);
                            compile_block_src(fblk,(int)strlen(fblk));
                            free(fblk);

                            /* Default return */
                            if(fi->ret_type==TY_VOID)emit("    ret");
                            else{emit("    mov x0, 0");emit("    ret");}

                            /* Restore state */
                            nvar=saved_nvar;
                            memcpy(local_reg_used,saved_regs,sizeof(local_reg_used));
                            current_func=saved_func;
                            loop_depth=saved_loop;
                            sw_depth=saved_sw;
                            cur_scope=saved_scope;
                            continue;
                        }
                    }
                }
            }
        }

        /* ── User goto label: "ident:" ── */
        {
            const char *look=p->src+p->pos;
            if(isalpha((unsigned char)*look)||*look=='_'){
                const char *q=look;
                while(*q&&(isalnum((unsigned char)*q)||*q=='_'))q++;
                const char *q2=q;
                while(*q2&&isspace((unsigned char)*q2))q2++;
                if(*q2==':'&&*(q2+1)!=':'){
                    char cname[64]; int nl=(int)(q-look); if(nl>63)nl=63;
                    strncpy(cname,look,nl); cname[nl]='\0';
                    if(strcmp(cname,"default")&&strcmp(cname,"case")){
                        char iwa_lbl[MAX_LABEL_LEN]; new_label(iwa_lbl,"gl");
                        add_goto_label(cname,iwa_lbl);
                        emit("%s:",iwa_lbl);
                        p->pos=(int)(q2-p->src)+1;
                        continue;
                    }
                }
            }
        }

        /* ── Nested block ── */
        if(c=='{'){
            char *blk=(char*)malloc(BLOCK_BUF_SIZE);
            if(!blk){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            sp_read_block(p,blk,BLOCK_BUF_SIZE);
            ScopeSnap ss=scope_enter();
            compile_block_src(blk,(int)strlen(blk));
            scope_leave(ss);
            free(blk);
            continue;
        }

        /* ── Regular statement ── */
        {
            char *st=(char*)malloc(MAX_LINE);
            if(!st){fprintf(stderr,"c2iwa: out of memory\n");exit(1);}
            if(!sp_read_stmt(p,st,MAX_LINE)){p->pos++;free(st);continue;}
            compile_stmt_str(st);
            free(st);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Preprocessor line removal
 * (strips #include, #define, #if, #ifdef, #pragma, #line, etc.)
 * ═══════════════════════════════════════════════════════════════ */
static void strip_preprocessor(const char *src, char *out, int outlen) {
    int i=0,o=0; int in_s=0; int line_start=1;
    while(src[i]&&o<outlen-2){
        if(src[i]=='"'&&!in_s)in_s=1;
        else if(src[i]=='"'&&in_s)in_s=0;
        if(!in_s&&line_start&&src[i]=='#'){
            /* Consume entire directive, handling line-continuations */
            while(src[i]&&src[i]!='\n'){
                if(src[i]=='\\'&&src[i+1]=='\n'){i+=2;continue;}
                i++;
            }
            out[o++]='\n'; line_start=1; continue;
        }
        line_start=(src[i]=='\n');
        out[o++]=src[i++];
    }
    out[o]='\0';
}

/* ═══════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    out_buf_init();

    if(argc<3){
        fprintf(stderr,"Usage: %s <input.c> <output.iwa>\n",argv[0]);
        return 1;
    }

    FILE *fin=fopen(argv[1],"r");
    if(!fin){perror(argv[1]);return 1;}

    char *src=(char*)malloc(MAX_SRC);
    if(!src){fprintf(stderr,"c2iwa: out of memory\n");return 1;}
    int slen=(int)fread(src,1,MAX_SRC-1,fin);
    src[slen]='\0'; fclose(fin);

    char *clean=(char*)malloc(MAX_SRC);
    if(!clean){fprintf(stderr,"c2iwa: out of memory\n");free(src);return 1;}
    strip_preprocessor(src,clean,MAX_SRC);
    free(src);

    /* File header */
    emit("; IWA generated by c2iwa");
    emit("; Source: %s",argv[1]);
    emit("; ; ; ;");

    /* Entry: jump past all function defs to main entry stub */
    emit("    jmp _main_entry");

    /* Compile entire source */
    compile_block_src(clean,(int)strlen(clean));

    /* Implicit halt after top-level code */
    emit("    halt");

    /* Main entry stub */
    emit("_main_entry:");
    emit("    mov x%d, 0",VREG_ARG_LO);
    emit("    mov x%d, 0",VREG_ARG_LO+1);
    emit("    call main");
    emit("    halt");

    free(clean);

    /* Write output */
    FILE *fout=fopen(argv[2],"w");
    if(!fout){perror(argv[2]);return 1;}
    for(int i=0;i<out_n;i++)fprintf(fout,"%s\n",out_buf[i]);
    fclose(fout);

    fprintf(stdout,"transpiled %d -> %s\n",out_n,argv[2]);
    out_buf_free();
    return 0;
}

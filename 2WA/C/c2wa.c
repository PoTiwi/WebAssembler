#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define MAX_VARS       512
#define MAX_OUT        131072
#define MAX_LINE       4096
#define MAX_SRC        (8 << 20)   /* 8 MB */
#define MAX_LABEL_LEN  128
#define MAX_SCOPE      256
#define MAX_FUNCS      256
#define MAX_PARAMS     16
#define MAX_FMT_ARGS   32
#define VREG_USER_LO   1
#define VREG_USER_HI   20
#define VREG_ARG_LO    21
#define VREG_ARG_HI    26
#define VREG_SCRATCH_A 28
#define VREG_SCRATCH_B 29

static char  out_buf[MAX_OUT][MAX_LINE];
static int   out_n = 0;

static int emit(const char *fmt, ...) {
    if (out_n >= MAX_OUT) { fprintf(stderr,"c2iwa: output overflow\n"); exit(1); }
    va_list ap; va_start(ap, fmt);
    vsnprintf(out_buf[out_n], MAX_LINE, fmt, ap);
    va_end(ap);
    return out_n++;
}

static void patch(int idx, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(out_buf[idx], MAX_LINE, fmt, ap);
    va_end(ap);
}

/* ═══════════════════════════════════════════════════════════════
 * Label generator
 * ═══════════════════════════════════════════════════════════════ */
static int label_cnt = 0;
static void new_label(char *buf, const char *prefix) {
    sprintf(buf, "._c2%s%d", prefix, label_cnt++);
}

/* ═══════════════════════════════════════════════════════════════
 * Type system (minimal)
 * ═══════════════════════════════════════════════════════════════ */
typedef enum { TY_INT=0, TY_STR=1, TY_VOID=2 } CType;

/* ═══════════════════════════════════════════════════════════════
 * Variable / register table
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    char   name[80];
    int    reg;
    CType  type;
    int    scope_depth;
    int    is_array;
    int    array_size;
} Var;

static Var  vars[MAX_VARS];
static int  nvar  = 0;
static int  next_user_reg = VREG_USER_LO;
static int  current_scope = 0;

static int alloc_reg(void) {
    if (next_user_reg > VREG_USER_HI) {
        fprintf(stderr,"c2iwa: too many variables (max %d)\n",
                VREG_USER_HI - VREG_USER_LO + 1);
        exit(1);
    }
    return next_user_reg++;
}

static Var *find_var(const char *name) {
    for (int i = nvar-1; i >= 0; i--)
        if (strcmp(vars[i].name, name)==0) return &vars[i];
    return NULL;
}

static Var *add_var(const char *name, CType t) {
    Var *v = find_var(name);
    if (v) return v;
    if (nvar >= MAX_VARS) { fprintf(stderr,"c2iwa: too many vars\n"); exit(1); }
    strncpy(vars[nvar].name, name, 79);
    vars[nvar].reg         = alloc_reg();
    vars[nvar].type        = t;
    vars[nvar].scope_depth = current_scope;
    vars[nvar].is_array    = 0;
    vars[nvar].array_size  = 0;
    return &vars[nvar++];
}

/* Enter/leave a scope block */
static int scope_save_nvar(void) { current_scope++; return nvar; }
static void scope_restore_nvar(int saved) {
    /* Variables declared in the inner scope are hidden; their registers are
       not reclaimed (simple approach) */
    nvar = saved;
    if (current_scope > 0) current_scope--;
}

/* ═══════════════════════════════════════════════════════════════
 * Function table (for parameter passing & return type)
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    char  name[64];
    char  param_names[MAX_PARAMS][64];
    CType param_types[MAX_PARAMS];
    int   nparams;
    CType ret_type;
    /* saved-register list for this function (registers used as locals) */
    int   saved_regs[VREG_USER_HI - VREG_USER_LO + 1];
    int   nsaved;
} FuncInfo;

static FuncInfo funcs[MAX_FUNCS];
static int      nfuncs = 0;
/* currently-compiling function */
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
    strncpy(funcs[nfuncs].name, name, 63);
    funcs[nfuncs].nparams = 0;
    funcs[nfuncs].ret_type = TY_INT;
    funcs[nfuncs].nsaved   = 0;
    return &funcs[nfuncs++];
}

/* ═══════════════════════════════════════════════════════════════
 * Loop break/continue stack
 * ═══════════════════════════════════════════════════════════════ */
typedef struct { char brk[MAX_LABEL_LEN]; char cont[MAX_LABEL_LEN]; } LoopCtx;
static LoopCtx loop_stack[MAX_SCOPE];
static int     loop_depth = 0;

static void push_loop(const char *brk, const char *cont) {
    if (loop_depth >= MAX_SCOPE) { fprintf(stderr,"c2iwa: loop nesting too deep\n"); exit(1); }
    strncpy(loop_stack[loop_depth].brk,  brk,  MAX_LABEL_LEN-1);
    strncpy(loop_stack[loop_depth].cont, cont, MAX_LABEL_LEN-1);
    loop_depth++;
}
static void pop_loop(void) { if (loop_depth>0) loop_depth--; }

/* ═══════════════════════════════════════════════════════════════
 * Switch stack (for case/default labels)
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    int   cmp_reg;
    char  end_lbl[MAX_LABEL_LEN];
    /* chain of pending "skip" labels for cases not yet visited */
    char  pending_lbl[MAX_LABEL_LEN];
    int   has_pending;
    int   in_default;
} SwitchCtx;
static SwitchCtx sw_stack[MAX_SCOPE];
static int       sw_depth = 0;

/* ═══════════════════════════════════════════════════════════════
 * goto label table (within current function)
 * ═══════════════════════════════════════════════════════════════ */
typedef struct { char cname[64]; char iwa_lbl[MAX_LABEL_LEN]; } GotoLabel;
static GotoLabel goto_labels[256];
static int       ngoto = 0;

static void add_goto_label(const char *cname, const char *lbl) {
    if (ngoto >= 256) return;
    strncpy(goto_labels[ngoto].cname,   cname, 63);
    strncpy(goto_labels[ngoto].iwa_lbl, lbl, MAX_LABEL_LEN-1);
    ngoto++;
}
static const char *find_goto_label(const char *cname) {
    for (int i = 0; i < ngoto; i++)
        if (strcmp(goto_labels[i].cname, cname)==0)
            return goto_labels[i].iwa_lbl;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * String utilities
 * ═══════════════════════════════════════════════════════════════ */
static void strip_ws(char *s) {
    int i=0; while(s[i]&&isspace((unsigned char)s[i]))i++;
    if(i) memmove(s,s+i,strlen(s)-i+1);
    int n=(int)strlen(s);
    while(n>0&&isspace((unsigned char)s[n-1]))s[--n]='\0';
}

static void strip_semi(char *s) {
    int n=(int)strlen(s);
    while(n>0&&(s[n-1]==';'||isspace((unsigned char)s[n-1])))s[--n]='\0';
}

/* Escape a C string literal to IWA token-safe encoding */
static void c_str_to_iwa(const char *src, char *dst, int maxlen) {
    int di=0;
    for(int i=0; src[i]&&di<maxlen-4; i++) {
        unsigned char c=(unsigned char)src[i];
        // if     (c==' ')  { dst[di++]='\\'; dst[di++]='~'; }
        if(c=='\n') { dst[di++]='\\'; dst[di++]='n'; }
        else if(c=='\t') { dst[di++]='\\'; dst[di++]='t'; }
        else if(c=='\r') { dst[di++]='\\'; dst[di++]='r'; }
        else if(c=='\\') { dst[di++]='\\'; dst[di++]='\\'; }
        else if(c=='"')  { dst[di++]='\\'; dst[di++]='"'; }
        else if(c==0)    { dst[di++]='\\'; dst[di++]='0'; }
        else             dst[di++]=(char)c;
    }
    dst[di]='\0';
}

/* Skip balanced parens/brackets starting at *p */
static const char *skip_balanced(const char *p, char open, char close) {
    if(*p!=open) return p;
    int d=0, in_str=0;
    while(*p) {
        if(*p=='"') in_str=!in_str;
        if(!in_str) {
            if(*p==open)  d++;
            else if(*p==close){ d--; if(d==0){p++;break;} }
        }
        p++;
    }
    return p;
}

/* Extract content of balanced parens into buf, return bytes consumed */
static int extract_parens(const char *p, char *buf, int buflen) {
    const char *start=p;
    if(*p!='(') return 0;
    const char *end=skip_balanced(p,'(',')');
    int len=(int)(end-p-2); if(len<0)len=0;
    if(len>=buflen)len=buflen-1;
    strncpy(buf,p+1,len); buf[len]='\0';
    return (int)(end-start);
}

/* Extract a quoted string literal, return ptr past closing quote */
static const char *extract_string_lit(const char *p, char *out, int outlen) {
    if(*p!='"') return p;
    p++;
    int i=0;
    while(*p&&*p!='"') {
        if(*p=='\\'&&*(p+1)) {
            p++;
            switch(*p) {
                case 'n': if(i<outlen-1)out[i++]='\n'; break;
                case 't': if(i<outlen-1)out[i++]='\t'; break;
                case 'r': if(i<outlen-1)out[i++]='\r'; break;
                case '0': if(i<outlen-1)out[i++]='\0'; break;
                case '\\':if(i<outlen-1)out[i++]='\\'; break;
                case '"': if(i<outlen-1)out[i++]='"';  break;
                case 'a': if(i<outlen-1)out[i++]='\a'; break;
                case 'b': if(i<outlen-1)out[i++]='\b'; break;
                case 'x': {
                    /* hex escape */
                    p++;
                    char hex[3]={0}; int hi=0;
                    while(hi<2&&*p&&isxdigit((unsigned char)*p))hex[hi++]=*p++;
                    p--; /* will be incremented by outer loop */
                    if(i<outlen-1)out[i++]=(char)(int)strtol(hex,NULL,16);
                    break;
                }
                default: if(i<outlen-1)out[i++]=*p; break;
            }
        } else { if(i<outlen-1)out[i++]=*p; }
        p++;
    }
    out[i]='\0';
    if(*p=='"')p++;
    /* handle adjacent string literal: "hello" " world" */
    while(*p) {
        while(*p&&isspace((unsigned char)*p))p++;
        if(*p!='"') break;
        p++;
        while(*p&&*p!='"') {
            if(*p=='\\'&&*(p+1)){
                p++;
                switch(*p){
                    case 'n':if(i<outlen-1)out[i++]='\n';break;
                    case 't':if(i<outlen-1)out[i++]='\t';break;
                    case '\\':if(i<outlen-1)out[i++]='\\';break;
                    case '"':if(i<outlen-1)out[i++]='"';break;
                    default:if(i<outlen-1)out[i++]=*p;break;
                }
            } else { if(i<outlen-1)out[i++]=*p; }
            p++;
        }
        out[i]='\0';
        if(*p=='"')p++;
    }
    return p;
}

/* ═══════════════════════════════════════════════════════════════
 * Virtual register: expression result
 * ═══════════════════════════════════════════════════════════════ */
typedef enum { VK_REG, VK_IMM } VKind;
typedef struct { VKind kind; int reg; long long imm; } VReg;

static VReg vreg(int r)       { VReg v; v.kind=VK_REG; v.reg=r; v.imm=0; return v; }
static VReg vimm(long long n) { VReg v; v.kind=VK_IMM; v.reg=0; v.imm=n; return v; }

/* Materialise a VReg into a register, emitting mov if needed.
   Uses scratch only when the VReg is an immediate. */
static int materialise(VReg v, int scratch) {
    if(v.kind==VK_REG) return v.reg;
    emit("    mov x%d, %lld", scratch, v.imm);
    return scratch;
}

/* ═══════════════════════════════════════════════════════════════
 * Temporary register pool
 *
 * DISCIPLINE: alloc_tmp() increments depth and returns a register.
 * free_tmp()  decrements depth.  A caller MUST NOT call free_tmp on
 * a register it is still using.  Returned VRegs are NOT freed by the
 * function that creates them — the CALLER frees them after use.
 * ═══════════════════════════════════════════════════════════════ */
static int tmp_pool[8]={28,29,27,26,25,24,23,22};
static int tmp_depth=0;

static int alloc_tmp(void){
    if(tmp_depth>=8){fprintf(stderr,"c2iwa: expression too deep (>8 temps)\n");exit(1);}
    return tmp_pool[tmp_depth++];
}
static void free_tmp(void){ if(tmp_depth>0)tmp_depth--; }

/* Allocate a temp, materialise v into it if needed, return the reg.
   Unlike the old materialise(v, alloc_tmp()), this version only advances
   tmp_depth when the temp slot is actually consumed. */
static int mat_to_tmp(VReg v) {
    if(v.kind==VK_REG) return v.reg;   /* no temp needed */
    int t=alloc_tmp();
    emit("    mov x%d, %lld", t, v.imm);
    return t;
}

/* ═══════════════════════════════════════════════════════════════
 * Tokeniser / Lexer for expressions
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
    TK_COMMA, TK_SEMI, TK_COLON, TK_QUESTION, TK_DOT, TK_ARROW
} TKind;

typedef struct { TKind kind; char text[512]; long long num; } Token;

typedef struct {
    const char *src;
    int pos;
    Token peek;
    int peaked;
} Lexer;

static void lex_init(Lexer *L, const char *s) { L->src=s; L->pos=0; L->peaked=0; }

static Token lex_next(Lexer *L) {
    Token t; t.text[0]='\0'; t.num=0; t.kind=TK_EOF;

    /* skip whitespace and line comments */
again:
    while(L->src[L->pos]&&isspace((unsigned char)L->src[L->pos])) L->pos++;
    /* skip // comment to end of line */
    if(L->src[L->pos]=='/'&&L->src[L->pos+1]=='/') {
        while(L->src[L->pos]&&L->src[L->pos]!='\n')L->pos++;
        goto again;
    }
    /* skip block comment */
    if(L->src[L->pos]=='/'&&L->src[L->pos+1]=='*') {
        L->pos+=2;
        while(L->src[L->pos]&&!(L->src[L->pos]=='*'&&L->src[L->pos+1]=='/'))L->pos++;
        if(L->src[L->pos])L->pos+=2;
        goto again;
    }
    if(!L->src[L->pos]) { t.kind=TK_EOF; return t; }

    const char *p=L->src+L->pos;

    /* number literal */
    if(isdigit((unsigned char)*p)) {
        char *end;
        if(p[0]=='0'&&(p[1]=='x'||p[1]=='X'))
            t.num=(long long)strtoll(p,&end,16);
        else if(p[0]=='0'&&isdigit((unsigned char)p[1]))
            t.num=(long long)strtoll(p,&end,8);
        else
            t.num=(long long)strtoll(p,&end,10);
        while(*end=='l'||*end=='L'||*end=='u'||*end=='U') end++;
        int len=(int)(end-p); if(len>=511)len=511;
        strncpy(t.text,p,len); t.text[len]='\0';
        t.kind=TK_NUM; L->pos+=(int)(end-p); return t;
    }
    /* char literal */
    if(*p=='\'') {
        p++; L->pos++;
        if(*p=='\\'&&*(p+1)) { p++; L->pos++;
            switch(*p){
                case 'n':t.num='\n';break; case 't':t.num='\t';break;
                case 'r':t.num='\r';break; case '0':t.num=0;break;
                case '\\':t.num='\\';break; case '\'':t.num='\'';break;
                case 'x':{ p++; L->pos++;
                    char hx[3]={0}; int hi=0;
                    while(hi<2&&*p&&isxdigit((unsigned char)*p)){hx[hi++]=*p++;L->pos++;}
                    t.num=(long long)strtol(hx,NULL,16); p--; L->pos--; break; }
                default: t.num=(unsigned char)*p; break;
            }
        } else t.num=(unsigned char)*p;
        L->pos++;
        if(L->src[L->pos]=='\'')L->pos++;
        t.kind=TK_NUM; sprintf(t.text,"%lld",t.num); return t;
    }
    /* string literal */
    if(*p=='"') {
        char buf[2048];
        const char *end=extract_string_lit(p,buf,sizeof(buf));
        t.kind=TK_STR;
        strncpy(t.text,buf,511); t.text[511]='\0';
        L->pos+=(int)(end-p); return t;
    }
    /* identifier or keyword */
    if(isalpha((unsigned char)*p)||*p=='_') {
        int i=0;
        while(isalnum((unsigned char)L->src[L->pos])||L->src[L->pos]=='_') {
            if(i<510) t.text[i++]=L->src[L->pos];
            L->pos++;
        }
        t.text[i]='\0'; t.kind=TK_IDENT; return t;
    }

    /* two-char operators (check 3-char first) */
    char c=p[0], d=p[1];
    if(c=='<'&&d=='<'&&p[2]=='='){t.kind=TK_LSHIFTEQ;strcpy(t.text,"<<=");L->pos+=3;return t;}
    if(c=='>'&&d=='>'&&p[2]=='='){t.kind=TK_RSHIFTEQ;strcpy(t.text,">>=");L->pos+=3;return t;}
    #define TWO(a,b,k) if(c==(a)&&d==(b)){t.kind=(k);t.text[0]=c;t.text[1]=d;t.text[2]='\0';L->pos+=2;return t;}
    TWO('<','<',TK_LSHIFT) TWO('>','>',TK_RSHIFT)
    TWO('=','=',TK_EQ)     TWO('!','=',TK_NEQ)
    TWO('<','=',TK_LEQ)    TWO('>','=',TK_GEQ)
    TWO('&','&',TK_AND)    TWO('|','|',TK_OR)
    TWO('+','+',TK_PLUSPLUS)   TWO('-','-',TK_MINUSMINUS)
    TWO('+','=',TK_PLUSEQ)     TWO('-','=',TK_MINUSEQ)
    TWO('*','=',TK_STAREQ)     TWO('/','=',TK_SLASHEQ)
    TWO('%','=',TK_PERCENTEQ)  TWO('&','=',TK_AMPEQ)
    TWO('|','=',TK_PIPEEQ)     TWO('^','=',TK_CARETEQ)
    TWO('-','>',TK_ARROW)
    #undef TWO

    t.text[0]=c; t.text[1]='\0'; L->pos++;
    switch(c) {
        case '+': t.kind=TK_PLUS;    break; case '-': t.kind=TK_MINUS;   break;
        case '*': t.kind=TK_STAR;    break; case '/': t.kind=TK_SLASH;   break;
        case '%': t.kind=TK_PERCENT; break; case '&': t.kind=TK_AMP;     break;
        case '|': t.kind=TK_PIPE;   break; case '^': t.kind=TK_CARET;   break;
        case '~': t.kind=TK_TILDE;  break; case '!': t.kind=TK_BANG;    break;
        case '<': t.kind=TK_LT;     break; case '>': t.kind=TK_GT;      break;
        case '=': t.kind=TK_ASSIGN; break;
        case '(': t.kind=TK_LPAREN; break; case ')': t.kind=TK_RPAREN;  break;
        case '{': t.kind=TK_LBRACE; break; case '}': t.kind=TK_RBRACE;  break;
        case '[': t.kind=TK_LBRACKET;break;case ']': t.kind=TK_RBRACKET;break;
        case ',': t.kind=TK_COMMA;  break; case ';': t.kind=TK_SEMI;    break;
        case ':': t.kind=TK_COLON;  break; case '?': t.kind=TK_QUESTION;break;
        case '.': t.kind=TK_DOT;    break;
        default:  t.kind=TK_EOF;    break;
    }
    return t;
}

static Token lex_peek(Lexer *L) {
    if(!L->peaked) { L->peek=lex_next(L); L->peaked=1; }
    return L->peek;
}
static Token lex_consume(Lexer *L) {
    if(L->peaked) { L->peaked=0; return L->peek; }
    return lex_next(L);
}
static int lex_match(Lexer *L, TKind k) {
    if(lex_peek(L).kind==k){ lex_consume(L); return 1; }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Helper: emit lds into a register
 * ═══════════════════════════════════════════════════════════════ */
static void emit_lds(int reg, const char *cstr) {
    char esc[4096]; c_str_to_iwa(cstr,esc,sizeof(esc));
    emit("    lds x%d, \"%s\"", reg, esc);
}

/* ═══════════════════════════════════════════════════════════════
 * Forward declarations
 * ═══════════════════════════════════════════════════════════════ */
static VReg compile_expr(Lexer *L);
static void compile_block_src(const char *src, int len);

/* ═══════════════════════════════════════════════════════════════
 * Function call compiler
 *
 * Key fix: evaluate ALL arguments into temporaries FIRST, then
 * move them into x21..x26.  This prevents nested calls from
 * clobbering already-placed arguments.
 * ═══════════════════════════════════════════════════════════════ */
static VReg compile_call(Lexer *L, const char *fname) {
    /* ---------- collect argument values ---------- */
    /* Each arg is evaluated and its integer value (and string) stored in
       a pair of arrays before we start moving into the arg registers. */
    int    arg_regs[MAX_FMT_ARGS];   /* register holding the int value */
    int    arg_is_tmp[MAX_FMT_ARGS]; /* 1 if we allocated a tmp for this arg */
    int    nargs=0;

    if(lex_peek(L).kind!=TK_RPAREN) {
        while(1) {
            int old_depth=tmp_depth;
            VReg av=compile_expr(L);
            int ar=mat_to_tmp(av);
            int used_tmp=(tmp_depth>old_depth);
            arg_regs[nargs]=ar;
            arg_is_tmp[nargs]=used_tmp;
            if(nargs<MAX_FMT_ARGS)nargs++;
            if(lex_peek(L).kind!=TK_COMMA) break;
            lex_consume(L);
        }
    }
    lex_match(L,TK_RPAREN);

    /* ---------- built-in functions ---------- */
    int a0 = nargs>0 ? arg_regs[0] : 0;
    int a1 = nargs>1 ? arg_regs[1] : 0;
    int a2 = nargs>2 ? arg_regs[2] : 0;

    if(strcmp(fname,"printf")==0||strcmp(fname,"puts")==0||
       strcmp(fname,"fprintf")==0) {
        /* Already handled at statement level; if we get here just emit puts */
        emit("    puts x%d", a0);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(0);
    }
    if(strcmp(fname,"putchar")==0&&nargs>=1) {
        int tr=alloc_tmp();
        emit("    itoa x%d, x%d", tr, a0);
        emit("    puts x%d", tr);
        free_tmp();
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vimm(0);
    }
    if(strcmp(fname,"getchar")==0) {
        int tr=alloc_tmp();
        emit("    geti x%d", tr);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"strlen")==0&&nargs>=1) {
        int tr=alloc_tmp();
        emit("    adl x%d, x%d", tr, a0);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"strcmp")==0&&nargs>=2) {
        int tr=alloc_tmp();
        emit("    strcmp x%d, x%d, x%d", tr, a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"strcpy")==0&&nargs>=2) {
        emit("    strcpy x%d, x%d", a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(a0);
    }
    if(strcmp(fname,"strcat")==0&&nargs>=2) {
        emit("    strcat x%d, x%d", a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(a0);
    }
    if(strcmp(fname,"strncpy")==0&&nargs>=2) {
        emit("    strcpy x%d, x%d", a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(a0);
    }
    if(strcmp(fname,"strncat")==0&&nargs>=2) {
        emit("    strcat x%d, x%d", a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(a0);
    }
    if(strcmp(fname,"memcmp")==0&&nargs>=2) {
        int tr=alloc_tmp();
        emit("    strcmp x%d, x%d, x%d", tr, a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if((strcmp(fname,"abs")==0||strcmp(fname,"labs")==0||
        strcmp(fname,"llabs")==0)&&nargs>=1) {
        int tr=alloc_tmp();
        emit("    mov x%d, x%d", tr, a0);
        emit("    abs x%d", tr);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if((strcmp(fname,"atoi")==0||strcmp(fname,"atol")==0||
        strcmp(fname,"atoll")==0)&&nargs>=1) {
        int tr=alloc_tmp();
        emit("    atoi x%d, x%d", tr, a0);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if((strcmp(fname,"strtol")==0||strcmp(fname,"strtoll")==0||
        strcmp(fname,"strtoul")==0)&&nargs>=1) {
        int tr=alloc_tmp();
        emit("    atoi x%d, x%d", tr, a0);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"rand")==0||strcmp(fname,"random")==0) {
        int tr=alloc_tmp();
        emit("    rand x%d", tr);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"srand")==0||strcmp(fname,"srandom")==0) {
        /* ignore seed */
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vimm(0);
    }
    if(strcmp(fname,"exit")==0||strcmp(fname,"abort")==0) {
        emit("    halt");
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vimm(0);
    }
    if(strcmp(fname,"malloc")==0||strcmp(fname,"calloc")==0||
       strcmp(fname,"realloc")==0||strcmp(fname,"free")==0) {
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vimm(0);
    }
    /* isdigit / isalpha / isspace / isupper / islower / isalnum / ispunct */
    if(strcmp(fname,"isdigit")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char lt[MAX_LABEL_LEN],le[MAX_LABEL_LEN];
        new_label(lt,"idt"); new_label(le,"ide");
        emit("    cmp x%d, #48", a0);
        emit("    jlt %s", le);
        emit("    cmp x%d, #57", a0);
        emit("    jgt %s", le);
        emit("    mov x%d, 1", tr);
        emit("    jmp %s", lt);
        emit("%s:", le);
        emit("    mov x%d, 0", tr);
        emit("%s:", lt);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"isalpha")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char la[MAX_LABEL_LEN],lb[MAX_LABEL_LEN],le[MAX_LABEL_LEN];
        new_label(la,"ala"); new_label(lb,"alb"); new_label(le,"ale");
        emit("    cmp x%d, #65", a0);
        emit("    jlt %s", la);
        emit("    cmp x%d, #90", a0);
        emit("    jle %s", lb);
        emit("%s:", la);
        emit("    cmp x%d, #97", a0);
        emit("    jlt %s", le);
        emit("    cmp x%d, #122", a0);
        emit("    jgt %s", le);
        emit("%s:", lb);
        emit("    mov x%d, 1", tr);
        emit("    jmp %s", la);
        emit("%s:", le);
        emit("    mov x%d, 0", tr);
        emit("%s:", la); /* reuse la as "done" label here - need new */
        /* simpler: just use a simple range check */
        (void)la; (void)lb; (void)le;
        /* Redo with cleaner labels */
        char lt2[MAX_LABEL_LEN],le2[MAX_LABEL_LEN];
        new_label(lt2,"alt"); new_label(le2,"ale2");
        /* erase last 4 emits and redo */
        out_n-=14; /* roll back */
        emit("    cmp x%d, #65", a0);  emit("    jlt %s", le2);
        emit("    cmp x%d, #90", a0);  emit("    jle %s", lt2);
        emit("    cmp x%d, #97", a0);  emit("    jlt %s", le2);
        emit("    cmp x%d, #122", a0); emit("    jgt %s", le2);
        emit("%s:", lt2);
        emit("    mov x%d, 1", tr);    emit("    jmp %s", le2);
        emit("    mov x%d, 0", tr);    /* dead */
        emit("%s:", le2);
        /* cleaner approach: evaluate to 1 first then patch */
        out_n-=12; /* redo again, simpler */
        {
            char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
            new_label(lT,"alpT"); new_label(lE,"alpE");
            emit("    cmp x%d, #65", a0);  emit("    jlt %s", lE);
            emit("    cmp x%d, #90", a0);  emit("    jle %s", lT);
            emit("    cmp x%d, #97", a0);  emit("    jlt %s", lE);
            emit("    cmp x%d, #122", a0); emit("    jgt %s", lE);
            emit("%s:", lT);
            emit("    mov x%d, 1", tr); emit("    jmp %s", lE);
            emit("    mov x%d, 0", tr);
            emit("%s:", lE);
        }
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"isspace")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"spT"); new_label(lE,"spE");
        /* space=32, \t=9, \n=10, \r=13, \f=12, \v=11 */
        emit("    cmp x%d, #32", a0); emit("    jeq %s", lT);
        emit("    cmp x%d, #9",  a0); emit("    jlt %s", lE);
        emit("    cmp x%d, #13", a0); emit("    jle %s", lT);
        emit("    jmp %s", lE);
        emit("%s:", lT); emit("    mov x%d, 1", tr); emit("    jmp %s", lE);
        emit("    mov x%d, 0", tr); emit("%s:", lE);
        out_n-=10; /* redo cleanly */
        {
            char lt2[MAX_LABEL_LEN],le2[MAX_LABEL_LEN];
            new_label(lt2,"spT2"); new_label(le2,"spE2");
            emit("    cmp x%d, #32", a0); emit("    jeq %s", lt2);
            emit("    cmp x%d, #9", a0);  emit("    jlt %s", le2);
            emit("    cmp x%d, #13", a0); emit("    jgt %s", le2);
            emit("%s:", lt2); emit("    mov x%d, 1", tr); emit("    jmp %s", le2);
            emit("    mov x%d, 0", tr); emit("%s:", le2);
        }
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"isupper")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"upT"); new_label(lE,"upE");
        emit("    cmp x%d, #65", a0); emit("    jlt %s", lE);
        emit("    cmp x%d, #90", a0); emit("    jgt %s", lE);
        emit("%s:", lT); emit("    mov x%d, 1", tr); emit("    jmp %s", lE);
        emit("    mov x%d, 0", tr); emit("%s:", lE);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"islower")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"loT"); new_label(lE,"loE");
        emit("    cmp x%d, #97", a0);  emit("    jlt %s", lE);
        emit("    cmp x%d, #122", a0); emit("    jgt %s", lE);
        emit("%s:", lT); emit("    mov x%d, 1", tr); emit("    jmp %s", lE);
        emit("    mov x%d, 0", tr); emit("%s:", lE);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"isalnum")==0&&nargs>=1) {
        /* isalpha || isdigit */
        int tr=alloc_tmp();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"anT"); new_label(lE,"anE");
        emit("    cmp x%d, #48", a0);  emit("    jlt %s", lE);
        emit("    cmp x%d, #57", a0);  emit("    jle %s", lT);
        emit("    cmp x%d, #65", a0);  emit("    jlt %s", lE);
        emit("    cmp x%d, #90", a0);  emit("    jle %s", lT);
        emit("    cmp x%d, #97", a0);  emit("    jlt %s", lE);
        emit("    cmp x%d, #122", a0); emit("    jgt %s", lE);
        emit("%s:", lT); emit("    mov x%d, 1", tr); emit("    jmp %s", lE);
        emit("    mov x%d, 0", tr); emit("%s:", lE);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"toupper")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"tuT"); new_label(lE,"tuE");
        emit("    cmp x%d, #97", a0);  emit("    jlt %s", lE);
        emit("    cmp x%d, #122", a0); emit("    jgt %s", lE);
        emit("    mov x%d, x%d", tr, a0);
        emit("    add x%d, x%d, -32", tr, tr);
        emit("    jmp %s", lT);
        emit("%s:", lE); emit("    mov x%d, x%d", tr, a0);
        emit("%s:", lT);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"tolower")==0&&nargs>=1) {
        int tr=alloc_tmp();
        char lT[MAX_LABEL_LEN],lE[MAX_LABEL_LEN];
        new_label(lT,"tlT"); new_label(lE,"tlE");
        emit("    cmp x%d, #65", a0); emit("    jlt %s", lE);
        emit("    cmp x%d, #90", a0); emit("    jgt %s", lE);
        emit("    mov x%d, x%d", tr, a0);
        emit("    add x%d, x%d, 32", tr, tr);
        emit("    jmp %s", lT);
        emit("%s:", lE); emit("    mov x%d, x%d", tr, a0);
        emit("%s:", lT);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if((strcmp(fname,"sprintf")==0||strcmp(fname,"snprintf")==0)&&nargs>=2) {
        /* sprintf(dest_reg, fmt, ...) — build string in dest register */
        /* a0=dest, a1=fmt (string register).  For simplicity just move fmt to dest. */
        emit("    strcpy x%d, x%d", a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(a0);
    }
    if(strcmp(fname,"sscanf")==0&&nargs>=2) {
        /* sscanf(src, fmt, &var) — just do atoi on src for now */
        if(nargs>=3) {
            emit("    atoi x%d, x%d", a2, a0);
        }
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vimm(0);
    }
    if(strcmp(fname,"__builtin_expect")==0&&nargs>=1) {
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        return vreg(a0);
    }
    if(strcmp(fname,"max")==0&&nargs>=2) {
        int tr=alloc_tmp();
        emit("    max2 x%d, x%d, x%d", tr, a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }
    if(strcmp(fname,"min")==0&&nargs>=2) {
        int tr=alloc_tmp();
        emit("    min2 x%d, x%d, x%d", tr, a0, a1);
        for(int i=0;i<nargs;i++)if(arg_is_tmp[i])free_tmp();
        VReg r=vreg(tr); free_tmp(); return r;
    }

    /* ---------- user-defined function call ---------- */
    /* Move evaluated args into x21..x26 */
    for(int i=0;i<nargs&&i<(VREG_ARG_HI-VREG_ARG_LO+1);i++) {
        int argreg=VREG_ARG_LO+i;
        if(arg_regs[i]!=argreg)
            emit("    mov x%d, x%d", argreg, arg_regs[i]);
    }
    for(int i=0;i<nargs;i++) if(arg_is_tmp[i]) free_tmp();

    emit("    call %s", fname);

    /* result in x0 */
    int tr=alloc_tmp();
    emit("    mov x%d, x%d", tr, 0);
    VReg r=vreg(tr); free_tmp(); return r;
}

/* ═══════════════════════════════════════════════════════════════
 * Primary expression compiler
 * ═══════════════════════════════════════════════════════════════ */
static VReg compile_primary(Lexer *L) {
    Token t=lex_peek(L);

    if(t.kind==TK_NUM) { lex_consume(L); return vimm(t.num); }

    if(t.kind==TK_STR) {
        lex_consume(L);
        int tr=alloc_tmp();
        emit_lds(tr, t.text);
        VReg r=vreg(tr); free_tmp(); return r;
    }

    if(t.kind==TK_LPAREN) {
        lex_consume(L);
        /* Cast detection: (type) expr  or  (type *) expr
           A cast starts with an identifier that is a type keyword,
           and ends with ')' possibly preceded by more type keywords and '*'. */
        Token p2=lex_peek(L);
        if(p2.kind==TK_IDENT) {
            /* save state */
            int saved_pos=L->pos; int saved_peaked=L->peaked; Token saved_peek=L->peek;
            /* consume type identifier(s) and stars */
            int consumed=0;
            while(lex_peek(L).kind==TK_IDENT||lex_peek(L).kind==TK_STAR) {
                lex_consume(L); consumed++;
            }
            if(lex_peek(L).kind==TK_RPAREN&&consumed>0) {
                lex_consume(L); /* consume ) */
                /* it's a cast — evaluate sub-expr, ignore cast */
                return compile_expr(L);
            }
            /* not a cast — restore */
            L->pos=saved_pos; L->peaked=saved_peaked; L->peek=saved_peek;
        }
        VReg v=compile_expr(L);
        lex_match(L,TK_RPAREN);
        return v;
    }

    /* sizeof */
    if(t.kind==TK_IDENT&&strcmp(t.text,"sizeof")==0) {
        lex_consume(L);
        if(lex_peek(L).kind==TK_LPAREN) {
            lex_consume(L);
            int dep=1;
            while(dep>0&&lex_peek(L).kind!=TK_EOF) {
                Token tk=lex_consume(L);
                if(tk.kind==TK_LPAREN)dep++;
                else if(tk.kind==TK_RPAREN)dep--;
            }
        }
        return vimm(8);
    }

    /* NULL */
    if(t.kind==TK_IDENT&&strcmp(t.text,"NULL")==0) {
        lex_consume(L); return vimm(0);
    }
    /* true / false */
    if(t.kind==TK_IDENT&&strcmp(t.text,"true")==0)  { lex_consume(L); return vimm(1); }
    if(t.kind==TK_IDENT&&strcmp(t.text,"false")==0) { lex_consume(L); return vimm(0); }

    /* identifier: variable, function call, pre-inc/dec */
    if(t.kind==TK_IDENT) {
        lex_consume(L);
        char name[512]; strncpy(name,t.text,511);

        /* function call */
        if(lex_peek(L).kind==TK_LPAREN) {
            lex_consume(L); /* consume ( */
            return compile_call(L, name);
        }

        Var *v=find_var(name);
        if(!v) v=add_var(name,TY_INT);

        /* array subscript: v[idx] */
        if(lex_peek(L).kind==TK_LBRACKET) {
            lex_consume(L);
            VReg idx=compile_expr(L);
            lex_match(L,TK_RBRACKET);
            int ir=mat_to_tmp(idx);
            int tr=alloc_tmp();
            /* base address is stored in v->reg; elements are 8 bytes apart */
            emit("    mov x%d, x%d", tr, v->reg);
            emit("    add x%d, x%d, x%d", tr, tr, ir);
            emit("    ldr x%d, x%d", tr, tr);
            if(idx.kind==VK_IMM) { /* ir was alloc_tmp'd */ free_tmp(); }
            VReg r=vreg(tr); free_tmp(); return r;
        }

        /* struct/pointer member access — simplified: just return the base variable */
        if(lex_peek(L).kind==TK_DOT||lex_peek(L).kind==TK_ARROW) {
            lex_consume(L); /* skip . or -> */
            lex_consume(L); /* skip member name */
            return vreg(v->reg);
        }

        /* post-increment / post-decrement */
        if(lex_peek(L).kind==TK_PLUSPLUS) {
            lex_consume(L);
            int tr=alloc_tmp();
            emit("    mov x%d, x%d", tr, v->reg);
            emit("    inc x%d", v->reg);
            VReg r=vreg(tr); free_tmp(); return r;
        }
        if(lex_peek(L).kind==TK_MINUSMINUS) {
            lex_consume(L);
            int tr=alloc_tmp();
            emit("    mov x%d, x%d", tr, v->reg);
            emit("    dec x%d", v->reg);
            VReg r=vreg(tr); free_tmp(); return r;
        }

        return vreg(v->reg);
    }

    /* fall-through: skip unknown token */
    lex_consume(L);
    return vimm(0);
}

/* ── Unary ── */
static VReg compile_unary(Lexer *L) {
    Token t=lex_peek(L);

    if(t.kind==TK_MINUS) {
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat_to_tmp(v);
        int tr=alloc_tmp();
        emit("    neg x%d, x%d", tr, r);
        if(v.kind==VK_IMM)free_tmp(); /* r was tmp */
        VReg res=vreg(tr); free_tmp(); return res;
    }
    if(t.kind==TK_PLUS) {
        lex_consume(L);
        return compile_unary(L);
    }
    if(t.kind==TK_BANG) {
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat_to_tmp(v);
        int tr=alloc_tmp();
        char ltrue[MAX_LABEL_LEN], lend[MAX_LABEL_LEN];
        new_label(ltrue,"ut"); new_label(lend,"ue");
        emit("    cmp x%d, #0", r);
        emit("    jeq %s", ltrue);
        emit("    mov x%d, 0", tr);
        emit("    jmp %s", lend);
        emit("%s:", ltrue);
        emit("    mov x%d, 1", tr);
        emit("%s:", lend);
        if(v.kind==VK_IMM)free_tmp();
        VReg res=vreg(tr); free_tmp(); return res;
    }
    if(t.kind==TK_TILDE) {
        lex_consume(L);
        VReg v=compile_unary(L);
        int r=mat_to_tmp(v);
        int tr=alloc_tmp();
        emit("    mov x%d, x%d", tr, r);
        emit("    not x%d", tr);
        if(v.kind==VK_IMM)free_tmp();
        VReg res=vreg(tr); free_tmp(); return res;
    }
    /* pre-increment */
    if(t.kind==TK_PLUSPLUS) {
        lex_consume(L);
        Token nm=lex_consume(L);
        if(nm.kind==TK_IDENT) {
            Var *v=find_var(nm.text); if(!v)v=add_var(nm.text,TY_INT);
            emit("    inc x%d", v->reg);
            return vreg(v->reg);
        }
        return vimm(0);
    }
    if(t.kind==TK_MINUSMINUS) {
        lex_consume(L);
        Token nm=lex_consume(L);
        if(nm.kind==TK_IDENT) {
            Var *v=find_var(nm.text); if(!v)v=add_var(nm.text,TY_INT);
            emit("    dec x%d", v->reg);
            return vreg(v->reg);
        }
        return vimm(0);
    }
    /* address-of / dereference — treat transparently */
    if(t.kind==TK_AMP||t.kind==TK_STAR) {
        lex_consume(L);
        return compile_unary(L);
    }
    return compile_primary(L);
}

/* ── Binary helper: emit a 3-register op ── */
static VReg emit_binop(const char *op, VReg lv, VReg rv) {
    /* Materialise both operands without burning extra temps when they're
       already in registers. */
    int lr=mat_to_tmp(lv);
    int rr=mat_to_tmp(rv);
    int tr=alloc_tmp();
    emit("    %s x%d, x%d, x%d", op, tr, lr, rr);
    if(rv.kind==VK_IMM)free_tmp(); /* rr was tmp */
    if(lv.kind==VK_IMM)free_tmp(); /* lr was tmp */
    VReg res=vreg(tr); free_tmp(); return res;
}

static VReg emit_shift(const char *op, VReg lv, VReg rv) {
    int lr=mat_to_tmp(lv);
    if(rv.kind==VK_IMM) {
        int tr=alloc_tmp();
        emit("    %s x%d, x%d, %lld", op, tr, lr, rv.imm);
        if(lv.kind==VK_IMM)free_tmp();
        VReg res=vreg(tr); free_tmp(); return res;
    }
    int rr=mat_to_tmp(rv);
    int tr=alloc_tmp();
    emit("    %s x%d, x%d, x%d", op, tr, lr, rr);
    free_tmp(); /* rr */
    if(lv.kind==VK_IMM)free_tmp();
    VReg res=vreg(tr); free_tmp(); return res;
}

/* ── Multiplicative: * / % ── */
static VReg compile_mul(Lexer *L) {
    VReg lv=compile_unary(L);
    while(1) {
        Token t=lex_peek(L);
        if(t.kind==TK_STAR)        { lex_consume(L); lv=emit_binop("mul", lv,compile_unary(L)); }
        else if(t.kind==TK_SLASH)  { lex_consume(L); lv=emit_binop("sdiv",lv,compile_unary(L)); }
        else if(t.kind==TK_PERCENT){ lex_consume(L); lv=emit_binop("mod", lv,compile_unary(L)); }
        else break;
    }
    return lv;
}

/* ── Additive: + - ── */
static VReg compile_add(Lexer *L) {
    VReg lv=compile_mul(L);
    while(1) {
        Token t=lex_peek(L);
        if(t.kind==TK_PLUS)       { lex_consume(L); lv=emit_binop("add",lv,compile_mul(L)); }
        else if(t.kind==TK_MINUS) { lex_consume(L); lv=emit_binop("sub",lv,compile_mul(L)); }
        else break;
    }
    return lv;
}

/* ── Shift ── */
static VReg compile_shift(Lexer *L) {
    VReg lv=compile_add(L);
    while(1) {
        Token t=lex_peek(L);
        if(t.kind==TK_LSHIFT)       { lex_consume(L); lv=emit_shift("lsl",lv,compile_add(L)); }
        else if(t.kind==TK_RSHIFT)  { lex_consume(L); lv=emit_shift("asr",lv,compile_add(L)); }
        else break;
    }
    return lv;
}

/* ── Relational: < > <= >= ── */
static VReg compile_rel(Lexer *L) {
    VReg lv=compile_shift(L);
    while(1) {
        Token t=lex_peek(L); const char *br=NULL; int matched=1;
        if(t.kind==TK_LT)       br="jlt";
        else if(t.kind==TK_GT)  br="jgt";
        else if(t.kind==TK_LEQ) br="jle";
        else if(t.kind==TK_GEQ) br="jge";
        else matched=0;
        if(!matched) break;
        lex_consume(L);
        VReg rv=compile_shift(L);
        int lr=mat_to_tmp(lv), rr=mat_to_tmp(rv);
        int tr=alloc_tmp();
        char ltrue[MAX_LABEL_LEN], lend[MAX_LABEL_LEN];
        new_label(ltrue,"rt"); new_label(lend,"re");
        emit("    cmp x%d, x%d", lr, rr);
        emit("    %s %s", br, ltrue);
        emit("    mov x%d, 0", tr);
        emit("    jmp %s", lend);
        emit("%s:", ltrue);
        emit("    mov x%d, 1", tr);
        emit("%s:", lend);
        if(rv.kind==VK_IMM)free_tmp();
        if(lv.kind==VK_IMM)free_tmp();
        lv=vreg(tr); free_tmp();
    }
    return lv;
}

/* ── Equality: == != ── */
static VReg compile_eq(Lexer *L) {
    VReg lv=compile_rel(L);
    while(1) {
        Token t=lex_peek(L); const char *br=NULL; int matched=1;
        if(t.kind==TK_EQ)       br="jeq";
        else if(t.kind==TK_NEQ) br="jne";
        else matched=0;
        if(!matched) break;
        lex_consume(L);
        VReg rv=compile_rel(L);
        int lr=mat_to_tmp(lv), rr=mat_to_tmp(rv);
        int tr=alloc_tmp();
        char ltrue[MAX_LABEL_LEN], lend[MAX_LABEL_LEN];
        new_label(ltrue,"et"); new_label(lend,"ee");
        emit("    cmp x%d, x%d", lr, rr);
        emit("    %s %s", br, ltrue);
        emit("    mov x%d, 0", tr);
        emit("    jmp %s", lend);
        emit("%s:", ltrue);
        emit("    mov x%d, 1", tr);
        emit("%s:", lend);
        if(rv.kind==VK_IMM)free_tmp();
        if(lv.kind==VK_IMM)free_tmp();
        lv=vreg(tr); free_tmp();
    }
    return lv;
}

/* ── Bitwise AND / XOR / OR ── */
static VReg compile_bitand(Lexer *L) {
    VReg lv=compile_eq(L);
    while(lex_peek(L).kind==TK_AMP) { lex_consume(L); lv=emit_binop("and",lv,compile_eq(L)); }
    return lv;
}
static VReg compile_bitxor(Lexer *L) {
    VReg lv=compile_bitand(L);
    while(lex_peek(L).kind==TK_CARET) { lex_consume(L); lv=emit_binop("eor",lv,compile_bitand(L)); }
    return lv;
}
static VReg compile_bitor(Lexer *L) {
    VReg lv=compile_bitxor(L);
    while(lex_peek(L).kind==TK_PIPE) { lex_consume(L); lv=emit_binop("orr",lv,compile_bitxor(L)); }
    return lv;
}

/* ── Logical AND ── */
static VReg compile_logand(Lexer *L) {
    VReg lv=compile_bitor(L);
    while(lex_peek(L).kind==TK_AND) {
        lex_consume(L);
        int lr=mat_to_tmp(lv);
        int tr=alloc_tmp();
        char lfalse[MAX_LABEL_LEN], lend[MAX_LABEL_LEN];
        new_label(lfalse,"af"); new_label(lend,"ae");
        emit("    cmp x%d, #0", lr);
        emit("    jeq %s", lfalse);
        if(lv.kind==VK_IMM)free_tmp();
        VReg rv=compile_bitor(L);
        int rr=mat_to_tmp(rv);
        emit("    cmp x%d, #0", rr);
        emit("    jeq %s", lfalse);
        if(rv.kind==VK_IMM)free_tmp();
        emit("    mov x%d, 1", tr);
        emit("    jmp %s", lend);
        emit("%s:", lfalse);
        emit("    mov x%d, 0", tr);
        emit("%s:", lend);
        lv=vreg(tr); free_tmp();
    }
    return lv;
}

/* ── Logical OR ── */
static VReg compile_logor(Lexer *L) {
    VReg lv=compile_logand(L);
    while(lex_peek(L).kind==TK_OR) {
        lex_consume(L);
        int lr=mat_to_tmp(lv);
        int tr=alloc_tmp();
        char ltrue[MAX_LABEL_LEN], lend[MAX_LABEL_LEN];
        new_label(ltrue,"ot"); new_label(lend,"oe");
        emit("    cmp x%d, #0", lr);
        emit("    jne %s", ltrue);
        if(lv.kind==VK_IMM)free_tmp();
        VReg rv=compile_logand(L);
        int rr=mat_to_tmp(rv);
        emit("    cmp x%d, #0", rr);
        emit("    jne %s", ltrue);
        if(rv.kind==VK_IMM)free_tmp();
        emit("    mov x%d, 0", tr);
        emit("    jmp %s", lend);
        emit("%s:", ltrue);
        emit("    mov x%d, 1", tr);
        emit("%s:", lend);
        lv=vreg(tr); free_tmp();
    }
    return lv;
}

/* ── Ternary ── */
static VReg compile_ternary(Lexer *L) {
    VReg cond=compile_logor(L);
    if(lex_peek(L).kind!=TK_QUESTION) return cond;
    lex_consume(L);
    int cr=mat_to_tmp(cond);
    int tr=alloc_tmp();
    char lfalse[MAX_LABEL_LEN], lend[MAX_LABEL_LEN];
    new_label(lfalse,"tf"); new_label(lend,"te");
    emit("    cmp x%d, #0", cr);
    emit("    jeq %s", lfalse);
    if(cond.kind==VK_IMM)free_tmp();
    VReg tv=compile_expr(L);
    int tvr=mat_to_tmp(tv);
    emit("    mov x%d, x%d", tr, tvr);
    if(tv.kind==VK_IMM)free_tmp();
    emit("    jmp %s", lend);
    lex_match(L,TK_COLON);
    emit("%s:", lfalse);
    VReg fv=compile_expr(L);
    int fvr=mat_to_tmp(fv);
    emit("    mov x%d, x%d", tr, fvr);
    if(fv.kind==VK_IMM)free_tmp();
    emit("%s:", lend);
    VReg res=vreg(tr); free_tmp(); return res;
}

/* ── Assignment (including compound and array-element write) ── */
static VReg compile_assign(Lexer *L) {
    VReg lv=compile_ternary(L);
    Token t=lex_peek(L);

    const char *binop=NULL;
    TKind ak=t.kind;
    if(ak==TK_ASSIGN||ak==TK_PLUSEQ||ak==TK_MINUSEQ||ak==TK_STAREQ||
       ak==TK_SLASHEQ||ak==TK_PERCENTEQ||ak==TK_AMPEQ||ak==TK_PIPEEQ||
       ak==TK_CARETEQ||ak==TK_LSHIFTEQ||ak==TK_RSHIFTEQ) {

        lex_consume(L);
        if(ak==TK_PLUSEQ)    binop="add";
        else if(ak==TK_MINUSEQ)   binop="sub";
        else if(ak==TK_STAREQ)    binop="mul";
        else if(ak==TK_SLASHEQ)   binop="sdiv";
        else if(ak==TK_PERCENTEQ) binop="mod";
        else if(ak==TK_AMPEQ)     binop="and";
        else if(ak==TK_PIPEEQ)    binop="orr";
        else if(ak==TK_CARETEQ)   binop="eor";
        else if(ak==TK_LSHIFTEQ)  binop="lsl";
        else if(ak==TK_RSHIFTEQ)  binop="asr";

        VReg rv=compile_assign(L);

        if(lv.kind!=VK_REG) {
            /* can't assign to non-lvalue (e.g. a literal) */
            return rv;
        }
        int rr=mat_to_tmp(rv);
        if(binop) {
            emit("    %s x%d, x%d, x%d", binop, lv.reg, lv.reg, rr);
        } else {
            if(rr!=lv.reg) emit("    mov x%d, x%d", lv.reg, rr);
        }
        if(rv.kind==VK_IMM)free_tmp();
        return lv;
    }
    return lv;
}

static VReg compile_expr(Lexer *L) { return compile_assign(L); }

/* Compile an expression from a string, return the register holding the result.
   tmp_depth is reset to 0 on entry — each call is independent. */
static int compile_expr_str(const char *s) {
    Lexer L; lex_init(&L,s);
    tmp_depth=0;
    VReg v=compile_expr(&L);
    return materialise(v,VREG_SCRATCH_A);
}

/* ═══════════════════════════════════════════════════════════════
 * Condition emitter
 * ═══════════════════════════════════════════════════════════════ */
static void emit_cond_jump(const char *cond_s, const char *true_lbl, const char *false_lbl) {
    Lexer L; lex_init(&L,cond_s);
    tmp_depth=0;
    VReg v=compile_expr(&L);
    int r=materialise(v,VREG_SCRATCH_A);
    emit("    cmp x%d, #0", r);
    if(true_lbl&&false_lbl) {
        emit("    jne %s", true_lbl);
        emit("    jmp %s", false_lbl);
    } else if(true_lbl) {
        emit("    jne %s", true_lbl);
    } else if(false_lbl) {
        emit("    jeq %s", false_lbl);
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Printf compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_printf_args(const char *args_str) {
    const char *p=args_str;
    while(*p&&isspace((unsigned char)*p))p++;

    if(*p!='"') {
        /* Non-literal format string: just puts it */
        int r=compile_expr_str(args_str);
        emit("    puts x%d", r);
        return;
    }

    char fmt[4096];
    p=extract_string_lit(p,fmt,sizeof(fmt));

    /* Collect remaining argument expression strings */
    char arg_strs[MAX_FMT_ARGS][1024];
    int  nargs=0;
    while(*p&&nargs<MAX_FMT_ARGS) {
        while(*p&&(*p==','||isspace((unsigned char)*p)))p++;
        if(!*p) break;
        int dep=0, i=0; int in_str=0;
        while(*p) {
            if(*p=='"') in_str=!in_str;
            if(!in_str){if(*p=='(')dep++;else if(*p==')')dep--;}
            if(!in_str&&dep==0&&*p==',') break;
            if(i<1023) arg_strs[nargs][i++]=*p;
            p++;
        }
        arg_strs[nargs][i]='\0';
        strip_ws(arg_strs[nargs]);
        if(arg_strs[nargs][0]) nargs++;
    }

    /* Process format string, building output string in x0 */
    int first=1;
    char piece[2048]; int pi=0;
    int arg_idx=0;

    #define FLUSH_PIECE() do { if(pi>0){ piece[pi]='\0'; \
        char esc[4096]; c_str_to_iwa(piece,esc,sizeof(esc)); \
        if(first){emit("    lds x0, \"%s\"",esc);first=0;} \
        else{emit("    lds x%d, \"%s\"",VREG_SCRATCH_A,esc); \
             emit("    strcat x0, x%d",VREG_SCRATCH_A);} \
        pi=0;} } while(0)

    for(int i=0;fmt[i];i++) {
        if(fmt[i]=='%'&&fmt[i+1]) {
            i++;
            /* skip flags, width, precision */
            while(fmt[i]&&(fmt[i]=='-'||fmt[i]=='+'||fmt[i]==' '||
                           fmt[i]=='0'||isdigit((unsigned char)fmt[i])||
                           fmt[i]=='.'||fmt[i]=='*'))i++;
            /* skip length modifiers */
            while(fmt[i]=='l'||fmt[i]=='h'||fmt[i]=='z'||fmt[i]=='L') i++;
            char spec=fmt[i];
            FLUSH_PIECE();

            if(spec=='%') { piece[pi++]='%'; continue; }

            if(arg_idx<nargs) {
                int ar=compile_expr_str(arg_strs[arg_idx++]);
                if(spec=='s') {
                    if(first){emit("    mov x0, x%d",ar);first=0;}
                    else      emit("    strcat x0, x%d", ar);
                } else if(spec=='c') {
                    /* character: emit as single-char string via itoa */
                    emit("    itoa x%d, x%d", VREG_SCRATCH_A, ar);
                    if(first){emit("    mov x0, x%d",VREG_SCRATCH_A);first=0;}
                    else      emit("    strcat x0, x%d", VREG_SCRATCH_A);
                } else {
                    emit("    itoa x%d, x%d", VREG_SCRATCH_A, ar);
                    if(first){emit("    mov x0, x%d",VREG_SCRATCH_A);first=0;}
                    else      emit("    strcat x0, x%d", VREG_SCRATCH_A);
                }
            }
        } else {
            if(pi<2046) piece[pi++]=fmt[i];
        }
    }
    FLUSH_PIECE();
    #undef FLUSH_PIECE

    if(first) emit("    lds x0, \"\"");
    emit("    puts x0");
}

/* ═══════════════════════════════════════════════════════════════
 * Source-level parser
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    const char *src;
    int pos, len;
} SrcParser;

static void sp_skip_ws(SrcParser *p){
    while(p->pos<p->len&&isspace((unsigned char)p->src[p->pos]))p->pos++;
}
static void sp_skip_line_comment(SrcParser *p){
    while(p->pos<p->len&&p->src[p->pos]!='\n')p->pos++;
}
static void sp_skip_block_comment(SrcParser *p){
    p->pos+=2;
    while(p->pos+1<p->len){
        if(p->src[p->pos]=='*'&&p->src[p->pos+1]=='/')
            { p->pos+=2; return; }
        p->pos++;
    }
}
static void sp_skip(SrcParser *p){
    again:
    sp_skip_ws(p);
    if(p->pos+1<p->len&&p->src[p->pos]=='/'&&p->src[p->pos+1]=='/')
        { sp_skip_line_comment(p); goto again; }
    if(p->pos+1<p->len&&p->src[p->pos]=='/'&&p->src[p->pos+1]=='*')
        { sp_skip_block_comment(p); goto again; }
}

static char sp_peek(SrcParser *p){
    sp_skip(p); return p->pos<p->len?p->src[p->pos]:'\0';
}

static int sp_read_block(SrcParser *p, char *out, int outlen){
    sp_skip(p);
    if(p->pos>=p->len||p->src[p->pos]!='{') return 0;
    p->pos++;
    int depth=1, i=0, in_str=0;
    while(p->pos<p->len&&depth>0){
        char c=p->src[p->pos++];
        if(c=='"') in_str=!in_str;
        if(!in_str){
            if(c=='{'){ depth++;if(i<outlen-1)out[i++]=c; }
            else if(c=='}'){ depth--; if(depth>0&&i<outlen-1)out[i++]=c; }
            else if(i<outlen-1)out[i++]=c;
        } else { if(i<outlen-1)out[i++]=c; }
    }
    out[i]='\0';
    return 1;
}

static int sp_read_stmt(SrcParser *p, char *out, int outlen){
    sp_skip(p);
    int i=0, in_str=0, dep=0;
    while(p->pos<p->len){
        char c=p->src[p->pos++];
        if(c=='"') in_str=!in_str;
        if(!in_str){
            if(c=='('||c=='[')dep++;
            else if(c==')'||c==']')dep--;
            else if(c==';'&&dep==0) break;
        }
        if(i<outlen-1)out[i++]=c;
    }
    out[i]='\0';
    strip_ws(out);
    return i>0;
}

static int sp_starts(SrcParser *p, const char *kw){
    sp_skip(p);
    int kl=(int)strlen(kw);
    if(p->pos+kl>p->len) return 0;
    if(strncmp(p->src+p->pos,kw,kl)!=0) return 0;
    if(p->pos+kl<p->len&&(isalnum((unsigned char)p->src[p->pos+kl])||p->src[p->pos+kl]=='_'))
        return 0;
    return 1;
}

static int sp_read_parens(SrcParser *p, char *out, int outlen){
    sp_skip(p);
    int n=extract_parens(p->src+p->pos,out,outlen);
    p->pos+=n;
    return n;
}

static int is_type_kw(const char *s){
    return strncmp(s,"int",3)==0||strncmp(s,"long",4)==0||strncmp(s,"short",5)==0||
           strncmp(s,"char",4)==0||strncmp(s,"unsigned",8)==0||strncmp(s,"signed",6)==0||
           strncmp(s,"void",4)==0||strncmp(s,"float",5)==0||strncmp(s,"double",6)==0||
           strncmp(s,"const",5)==0||strncmp(s,"static",6)==0||strncmp(s,"extern",6)==0||
           strncmp(s,"register",8)==0||strncmp(s,"volatile",8)==0||
           strncmp(s,"auto",4)==0||strncmp(s,"inline",6)==0||
           strncmp(s,"size_t",6)==0||strncmp(s,"ssize_t",7)==0||
           strncmp(s,"uint8_t",7)==0||strncmp(s,"uint16_t",8)==0||
           strncmp(s,"uint32_t",8)==0||strncmp(s,"uint64_t",8)==0||
           strncmp(s,"int8_t",6)==0||strncmp(s,"int16_t",7)==0||
           strncmp(s,"int32_t",7)==0||strncmp(s,"int64_t",7)==0||
           strncmp(s,"bool",4)==0||strncmp(s,"_Bool",5)==0||
           strncmp(s,"struct",6)==0||strncmp(s,"union",5)==0||strncmp(s,"enum",4)==0||
           strncmp(s,"typedef",7)==0;
}

static const char *skip_type(const char *s){
    const char *p=s;
    while(*p){
        while(*p&&isspace((unsigned char)*p))p++;
        if(!is_type_kw(p)) break;
        while(*p&&(isalnum((unsigned char)*p)||*p=='_'))p++;
    }
    while(*p&&(*p=='*'||isspace((unsigned char)*p)))p++;
    return p;
}

/* ═══════════════════════════════════════════════════════════════
 * Statement compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_block_src(const char *src, int len);

static void compile_stmt_str(const char *stmt_in) {
    char stmt[MAX_LINE*2]; 
    strncpy(stmt,stmt_in,MAX_LINE*2-1); stmt[MAX_LINE*2-1]='\0';
    strip_ws(stmt); strip_semi(stmt); strip_ws(stmt);
    if(!*stmt||stmt[0]=='{')return;

    /* ── printf / fprintf ── */
    if((strncmp(stmt,"printf",6)==0&&(stmt[6]=='('||isspace((unsigned char)stmt[6])))||
       (strncmp(stmt,"fprintf",7)==0&&(stmt[7]=='('||isspace((unsigned char)stmt[7])))) {
        const char *start=stmt;
        if(strncmp(start,"fprintf",7)==0) {
            /* skip stream arg */
            start+=7; while(*start&&*start!='(')start++;
            start++;
            while(*start&&*start!=',')start++;
            if(*start==',')start++;
            while(*start&&isspace((unsigned char)*start))start++;
        } else { start+=6; while(*start&&*start!='(')start++; start++; }
        char args[MAX_LINE*2]; int n=0;
        /* collect balanced parens content */
        int dep=1; const char *q=start; int in_str=0;
        while(*q&&dep>0){
            if(*q=='"')in_str=!in_str;
            if(!in_str){if(*q=='(')dep++;else if(*q==')')dep--;}
            if(dep>0&&n<MAX_LINE*2-1)args[n++]=*q;
            q++;
        }
        args[n]='\0';
        compile_printf_args(args);
        return;
    }

    /* ── scanf / fscanf ── */
    if((strncmp(stmt,"scanf",5)==0&&(stmt[5]=='('||isspace((unsigned char)stmt[5])))||
       (strncmp(stmt,"fscanf",6)==0&&(stmt[6]=='('||isspace((unsigned char)stmt[6])))) {
        char args[MAX_LINE]; extract_parens(strchr(stmt,'('),args,sizeof(args));
        const char *p=args;
        while(*p&&isspace((unsigned char)*p))p++;
        /* skip stream for fscanf */
        if(strncmp(stmt,"fscanf",6)==0){
            while(*p&&*p!=',')p++;
            if(*p==',')p++;
            while(*p&&isspace((unsigned char)*p))p++;
        }
        if(*p=='"'){
            char fmt[256]; p=extract_string_lit(p,fmt,sizeof(fmt));
            while(*p&&(*p==','||isspace((unsigned char)*p)))p++;
            int fi=0;
            while(*p&&fi<(int)strlen(fmt)) {
                while(*p&&(*p==','||isspace((unsigned char)*p)))p++;
                if(!*p)break;
                if(*p=='&')p++;
                char vname[64]; int ni=0;
                while(*p&&(isalnum((unsigned char)*p)||*p=='_'))vname[ni++]=*p++;
                vname[ni]='\0';
                if(!*vname)break;
                /* find next % */
                while(fi<(int)strlen(fmt)&&fmt[fi]!='%')fi++;
                fi++;
                Var *v=find_var(vname); if(!v)v=add_var(vname,TY_INT);
                if(fi<=(int)strlen(fmt)&&(fmt[fi]=='s'||fmt[fi]=='c'))
                    emit("    gets x%d", v->reg);
                else
                    emit("    geti x%d", v->reg);
                fi++;
            }
        }
        return;
    }

    /* ── puts(expr) ── */
    if(strncmp(stmt,"puts",4)==0&&stmt[4]=='(') {
        char args[MAX_LINE]; extract_parens(stmt+4,args,sizeof(args));
        strip_ws(args);
        if(args[0]=='"') {
            char s[2048]; char tmp2[2048];
            extract_string_lit(args,tmp2,sizeof(tmp2));
            snprintf(s,sizeof(s),"%s\n",tmp2);
            emit_lds(0,s); emit("    puts x0");
        } else {
            int r=compile_expr_str(args);
            emit("    puts x%d", r);
        }
        return;
    }

    /* ── return ── */
    if(strncmp(stmt,"return",6)==0&&(isspace((unsigned char)stmt[6])||!stmt[6])) {
        char rval[MAX_LINE*2]="";
        if(stmt[6]){ strncpy(rval,stmt+7,MAX_LINE*2-1); strip_ws(rval); }
        int is_void=(current_func&&current_func->ret_type==TY_VOID);
        if(*rval&&!is_void) {
            tmp_depth=0;
            Lexer L; lex_init(&L,rval);
            VReg v=compile_expr(&L);
            if(v.kind==VK_REG) {
                if(v.reg!=0) emit("    mov x0, x%d", v.reg);
            } else {
                emit("    mov x0, %lld", v.imm);
            }
        } else if(!is_void) {
            emit("    mov x0, 0");
        }
        emit("    ret");
        return;
    }

    /* ── break ── */
    if(strcmp(stmt,"break")==0) {
        if(sw_depth>0) { emit("    jmp %s", sw_stack[sw_depth-1].end_lbl); return; }
        if(loop_depth>0) { emit("    jmp %s", loop_stack[loop_depth-1].brk); return; }
        fprintf(stderr,"c2iwa: break outside loop/switch\n");
        return;
    }

    /* ── continue ── */
    if(strcmp(stmt,"continue")==0) {
        if(loop_depth>0) { emit("    jmp %s", loop_stack[loop_depth-1].cont); return; }
        fprintf(stderr,"c2iwa: continue outside loop\n");
        return;
    }

    /* ── goto LABEL ── */
    if(strncmp(stmt,"goto",4)==0&&isspace((unsigned char)stmt[4])) {
        char lname[64]=""; int li=0;
        const char *p=stmt+5;
        while(*p&&isspace((unsigned char)*p))p++;
        while(*p&&(isalnum((unsigned char)*p)||*p=='_'))lname[li++]=*p++;
        lname[li]='\0';
        /* look up IWA label */
        const char *il=find_goto_label(lname);
        if(il) { emit("    jmp %s", il); }
        else   { /* forward goto — emit FIXUP-style; just emit with the C name */
            emit("    jmp %s", lname);
        }
        return;
    }

    /* ── variable declaration ── */
    if(is_type_kw(stmt)) {
        /* skip "struct Tag" / "enum Tag" / typedef aliases */
        const char *body=skip_type(stmt);
        /* skip ahead past potential pointer stars */
        while(*body&&(*body=='*'||isspace((unsigned char)*body)))body++;

        /* check for function forward declaration (has '(' before ';') */
        const char *lp=strchr(body,'(');
        if(lp) {
            /* it's a function prototype / call-expr, not a variable decl */
            /* fall through to expression-statement handling */
            goto expr_stmt;
        }

        /* parse comma-separated declarators */
        char decl_copy[MAX_LINE*2]; strncpy(decl_copy,body,MAX_LINE*2-1);
        char *tok_p=decl_copy;
        while(tok_p&&*tok_p) {
            int dep2=0; char *comma=NULL;
            for(char *q=tok_p;*q;q++){
                if(*q=='(')dep2++;else if(*q==')')dep2--;
                if(dep2==0&&*q==','){ comma=q; break; }
            }
            char one[MAX_LINE];
            if(comma){ int n2=(int)(comma-tok_p); strncpy(one,tok_p,n2); one[n2]='\0'; tok_p=comma+1; }
            else      { strncpy(one,tok_p,MAX_LINE-1); tok_p=NULL; }
            strip_ws(one);

            /* strip leading stars */
            char *op2=one; while(*op2=='*')op2++;

            /* get var name */
            char vname[80]; int ni=0;
            while(*op2&&(isalnum((unsigned char)*op2)||*op2=='_'))vname[ni++]=*op2++;
            vname[ni]='\0';
            if(!*vname) continue;
            strip_ws(op2);

            /* array? */
            int is_array=0; int array_sz=0;
            if(*op2=='[') {
                is_array=1; op2++;
                if(isdigit((unsigned char)*op2)||(*op2=='-'&&isdigit((unsigned char)*(op2+1))))
                    array_sz=(int)strtol(op2,&op2,10);
                while(*op2&&*op2!=']')op2++;
                if(*op2==']')op2++;
            }

            /* determine type */
            CType ct=TY_INT;
            { const char *ts=stmt;
              while(*ts&&isspace((unsigned char)*ts))ts++;
              if((strncmp(ts,"char",4)==0||strncmp(ts,"const",5)==0)&&
                 (strchr(stmt,'*')||is_array)) ct=TY_STR; }

            Var *v=add_var(vname,ct);
            v->is_array=is_array; v->array_size=array_sz;

            while(*op2&&isspace((unsigned char)*op2))op2++;
            if(*op2=='=') {
                op2++; strip_ws(op2);
                if(op2[0]=='"') {
                    char s[2048]; extract_string_lit(op2,s,sizeof(s));
                    emit_lds(v->reg,s);
                    v->type=TY_STR;
                } else if(op2[0]=='{'&&is_array) {
                    /* array initialiser — skip for now */
                } else {
                    tmp_depth=0;
                    int r=compile_expr_str(op2);
                    if(r!=v->reg) emit("    mov x%d, x%d", v->reg, r);
                }
            } else if(!is_array) {
                emit("    mov x%d, 0", v->reg);
            }
        }
        return;
    }

expr_stmt:
    /* ── general expression statement ── */
    tmp_depth=0;
    Lexer L; lex_init(&L,stmt);
    compile_expr(&L);
}

/* ═══════════════════════════════════════════════════════════════
 * Block / control-flow compiler
 * ═══════════════════════════════════════════════════════════════ */
static void compile_block_src(const char *src, int len) {
    SrcParser sp; sp.src=src; sp.pos=0; sp.len=len;
    SrcParser *p=&sp;

    while(p->pos<p->len) {
        sp_skip(p);
        if(p->pos>=p->len) break;
        char c=p->src[p->pos];
        if(c=='}') break;

        /* ── if ── */
        if(sp_starts(p,"if")) {
            p->pos+=2;
            char cond[MAX_LINE]; sp_read_parens(p,cond,sizeof(cond));
            sp_skip(p);
            char lbl_else[MAX_LABEL_LEN], lbl_end[MAX_LABEL_LEN];
            new_label(lbl_else,"ielse"); new_label(lbl_end,"iend");
            emit_cond_jump(cond, NULL, lbl_else);
            /* then branch */
            if(sp_peek(p)=='{') {
                char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
                int sv=scope_save_nvar();
                compile_block_src(blk,(int)strlen(blk));
                scope_restore_nvar(sv);
            } else { char st[MAX_LINE]; sp_read_stmt(p,st,sizeof(st)); compile_stmt_str(st); }
            sp_skip(p);
            if(sp_starts(p,"else")) {
                p->pos+=4;
                emit("    jmp %s", lbl_end);
                emit("%s:", lbl_else);
                sp_skip(p);
                if(sp_starts(p,"if")) {
                    /* else if — inline, don't recurse into compile_block_src which would
                       pick up the WHOLE remaining block; instead handle the if directly */
                    /* Just re-enter main loop logic by falling into the outer loop body. */
                    /* We'll emit the else-if chain without a new compile_block_src call. */
                    /* This is handled naturally because we don't advance past the 'if'
                       keyword, so the outer loop will pick it up. */
                    /* But we're not in the outer loop here... */
                    /* Simple: just call compile_block_src on the remaining src from here */
                    const char *remaining=p->src+p->pos;
                    int remlen=p->len-p->pos;
                    /* We need to parse just ONE statement (the else-if), not the whole rest */
                    /* Trick: parse the if-statement manually */
                    p->pos+=2; /* skip 'if' */
                    char cond2[MAX_LINE]; sp_read_parens(p,cond2,sizeof(cond2));
                    sp_skip(p);
                    char lbl_else2[MAX_LABEL_LEN], lbl_end2[MAX_LABEL_LEN];
                    new_label(lbl_else2,"ielse"); new_label(lbl_end2,"iend");
                    emit_cond_jump(cond2, NULL, lbl_else2);
                    if(sp_peek(p)=='{') {
                        char blk2[MAX_SRC/4]; sp_read_block(p,blk2,sizeof(blk2));
                        int sv2=scope_save_nvar();
                        compile_block_src(blk2,(int)strlen(blk2));
                        scope_restore_nvar(sv2);
                    } else { char st2[MAX_LINE]; sp_read_stmt(p,st2,sizeof(st2)); compile_stmt_str(st2); }
                    sp_skip(p);
                    if(sp_starts(p,"else")) {
                        p->pos+=4;
                        emit("    jmp %s", lbl_end2);
                        emit("%s:", lbl_else2);
                        sp_skip(p);
                        if(sp_peek(p)=='{') {
                            char blk3[MAX_SRC/4]; sp_read_block(p,blk3,sizeof(blk3));
                            int sv3=scope_save_nvar();
                            compile_block_src(blk3,(int)strlen(blk3));
                            scope_restore_nvar(sv3);
                        } else { char st3[MAX_LINE]; sp_read_stmt(p,st3,sizeof(st3)); compile_stmt_str(st3); }
                        emit("%s:", lbl_end2);
                    } else {
                        emit("%s:", lbl_else2);
                    }
                    (void)remaining; (void)remlen;
                } else if(sp_peek(p)=='{') {
                    char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
                    int sv=scope_save_nvar();
                    compile_block_src(blk,(int)strlen(blk));
                    scope_restore_nvar(sv);
                } else { char st[MAX_LINE]; sp_read_stmt(p,st,sizeof(st)); compile_stmt_str(st); }
                emit("%s:", lbl_end);
            } else {
                emit("%s:", lbl_else);
            }
            continue;
        }

        /* ── while ── */
        if(sp_starts(p,"while")) {
            p->pos+=5;
            char cond[MAX_LINE]; sp_read_parens(p,cond,sizeof(cond));
            sp_skip(p);
            char lbl_top[MAX_LABEL_LEN], lbl_end[MAX_LABEL_LEN], lbl_cont[MAX_LABEL_LEN];
            new_label(lbl_top,"wtop"); new_label(lbl_end,"wend"); new_label(lbl_cont,"wcont");
            emit("%s:", lbl_top);
            emit_cond_jump(cond, NULL, lbl_end);
            push_loop(lbl_end, lbl_top);
            if(sp_peek(p)=='{') {
                char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
                int sv=scope_save_nvar();
                compile_block_src(blk,(int)strlen(blk));
                scope_restore_nvar(sv);
            } else { char st[MAX_LINE]; sp_read_stmt(p,st,sizeof(st)); compile_stmt_str(st); }
            pop_loop();
            emit("    jmp %s", lbl_top);
            emit("%s:", lbl_end);
            continue;
        }

        /* ── do { } while ── */
        if(sp_starts(p,"do")) {
            p->pos+=2; sp_skip(p);
            char lbl_top[MAX_LABEL_LEN], lbl_end[MAX_LABEL_LEN], lbl_cond[MAX_LABEL_LEN];
            new_label(lbl_top,"dtop"); new_label(lbl_end,"dend"); new_label(lbl_cond,"dcond");
            emit("%s:", lbl_top);
            push_loop(lbl_end, lbl_cond);
            if(sp_peek(p)=='{') {
                char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
                int sv=scope_save_nvar();
                compile_block_src(blk,(int)strlen(blk));
                scope_restore_nvar(sv);
            } else { char st[MAX_LINE]; sp_read_stmt(p,st,sizeof(st)); compile_stmt_str(st); }
            pop_loop();
            emit("%s:", lbl_cond);
            sp_skip(p);
            if(sp_starts(p,"while")) p->pos+=5;
            char cond[MAX_LINE]; sp_read_parens(p,cond,sizeof(cond));
            sp_skip(p); if(p->pos<p->len&&p->src[p->pos]==';')p->pos++;
            emit_cond_jump(cond, lbl_top, NULL);
            emit("%s:", lbl_end);
            continue;
        }

        /* ── for ── */
        if(sp_starts(p,"for")) {
            p->pos+=3; sp_skip(p);
            if(p->pos>=p->len||p->src[p->pos]!='(') continue;
            p->pos++;

            /* init — read to ';' */
            char init_s[MAX_LINE]=""; int ii=0, in_str=0;
            while(p->pos<p->len&&!(p->src[p->pos]==';'&&!in_str)) {
                if(p->src[p->pos]=='"')in_str=!in_str;
                if(ii<MAX_LINE-1)init_s[ii++]=p->src[p->pos];
                p->pos++;
            }
            init_s[ii]='\0'; if(p->pos<p->len)p->pos++; /* skip ; */
            strip_ws(init_s);

            /* condition */
            char cond_s[MAX_LINE]=""; ii=0;
            while(p->pos<p->len&&p->src[p->pos]!=';') {
                if(ii<MAX_LINE-1)cond_s[ii++]=p->src[p->pos++];
            }
            cond_s[ii]='\0'; if(p->pos<p->len)p->pos++;
            strip_ws(cond_s);

            /* update — read to matching ')' */
            char upd_s[MAX_LINE]=""; ii=0; int dep3=0;
            while(p->pos<p->len) {
                char cc=p->src[p->pos];
                if(cc=='(')dep3++;
                else if(cc==')'){ if(dep3==0){p->pos++;break;} dep3--; }
                if(ii<MAX_LINE-1)upd_s[ii++]=cc;
                p->pos++;
            }
            upd_s[ii]='\0'; strip_ws(upd_s);

            sp_skip(p);
            char lbl_top[MAX_LABEL_LEN], lbl_end[MAX_LABEL_LEN], lbl_cont[MAX_LABEL_LEN];
            new_label(lbl_top,"ftop"); new_label(lbl_end,"fend"); new_label(lbl_cont,"fcont");

            int sv=scope_save_nvar();
            if(*init_s) compile_stmt_str(init_s);
            emit("%s:", lbl_top);
            if(*cond_s) emit_cond_jump(cond_s, NULL, lbl_end);
            push_loop(lbl_end, lbl_cont);
            if(sp_peek(p)=='{') {
                char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
                int sv2=scope_save_nvar();
                compile_block_src(blk,(int)strlen(blk));
                scope_restore_nvar(sv2);
            } else { char st[MAX_LINE]; sp_read_stmt(p,st,sizeof(st)); compile_stmt_str(st); }
            pop_loop();
            emit("%s:", lbl_cont);
            if(*upd_s) compile_stmt_str(upd_s);
            emit("    jmp %s", lbl_top);
            emit("%s:", lbl_end);
            scope_restore_nvar(sv);
            continue;
        }

        /* ── switch ── */
        if(sp_starts(p,"switch")) {
            p->pos+=6;
            char expr_s[MAX_LINE]; sp_read_parens(p,expr_s,sizeof(expr_s));
            sp_skip(p);
            char lbl_end[MAX_LABEL_LEN]; new_label(lbl_end,"swend");
            char lbl_first[MAX_LABEL_LEN]; new_label(lbl_first,"swfirst");

            /* evaluate the switch expression into a dedicated register */
            tmp_depth=0;
            int er=compile_expr_str(expr_s);
            int sr=alloc_reg(); next_user_reg--; /* borrow */
            emit("    mov x%d, x%d", sr, er);

            /* read body */
            char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));

            /* emit a jump to the first case dispatch, then the body.
               We use a two-pass approach: jump to a dispatch block at the top,
               compile the body, then emit the dispatch block at the end.
               Simpler: emit dispatch inline at the start, but we don't know
               the case values yet.  Use the "dispatch at top" model: */

            /* Jump to dispatch */
            char lbl_dispatch[MAX_LABEL_LEN]; new_label(lbl_dispatch,"swdisp");
            emit("    jmp %s", lbl_dispatch);
            char lbl_body_start[MAX_LABEL_LEN]; new_label(lbl_body_start,"swbody");
            emit("%s:", lbl_body_start);

            /* push switch context */
            if(sw_depth>=MAX_SCOPE){fprintf(stderr,"c2iwa: switch nesting too deep\n");exit(1);}
            sw_stack[sw_depth].cmp_reg=sr;
            strncpy(sw_stack[sw_depth].end_lbl,lbl_end,MAX_LABEL_LEN-1);
            sw_stack[sw_depth].has_pending=0;
            sw_stack[sw_depth].in_default=0;
            sw_depth++;
            push_loop(lbl_end, lbl_end);

            /* ---- pre-scan for case values and default ---- */
            /* We do a simple pre-scan to build a case-value list, then compile the body */
            /* Record case entry points and values */
            typedef struct { long long val; char lbl[MAX_LABEL_LEN]; int is_default; } CaseEntry;
            CaseEntry cases[256]; int ncases=0;

            /* Pre-scan pass: find all case/default labels and assign IWA labels */
            {
                SrcParser bp2; bp2.src=blk; bp2.pos=0; bp2.len=(int)strlen(blk);
                while(bp2.pos<bp2.len) {
                    sp_skip(&bp2);
                    if(bp2.pos>=bp2.len)break;
                    if(sp_starts(&bp2,"case")&&ncases<256) {
                        bp2.pos+=4;
                        char val_s[64]; int vi=0; sp_skip(&bp2);
                        while(bp2.pos<bp2.len&&bp2.src[bp2.pos]!=':'){
                            if(vi<63)val_s[vi++]=bp2.src[bp2.pos];
                            bp2.pos++;
                        }
                        val_s[vi]='\0'; if(bp2.pos<bp2.len)bp2.pos++;
                        strip_ws(val_s);
                        cases[ncases].val=strtoll(val_s,NULL,0);
                        cases[ncases].is_default=0;
                        new_label(cases[ncases].lbl,"case");
                        ncases++;
                    } else if(sp_starts(&bp2,"default")&&ncases<256) {
                        bp2.pos+=7; sp_skip(&bp2);
                        if(bp2.pos<bp2.len&&bp2.src[bp2.pos]==':')bp2.pos++;
                        cases[ncases].val=0;
                        cases[ncases].is_default=1;
                        new_label(cases[ncases].lbl,"swdef");
                        ncases++;
                    } else {
                        /* skip one token / statement */
                        if(bp2.src[bp2.pos]=='{') {
                            char tmp_blk[4096]; sp_read_block(&bp2,tmp_blk,sizeof(tmp_blk));
                        } else {
                            while(bp2.pos<bp2.len&&bp2.src[bp2.pos]!='\n'&&
                                  bp2.src[bp2.pos]!=';'&&bp2.src[bp2.pos]!='{'&&
                                  !sp_starts(&bp2,"case")&&!sp_starts(&bp2,"default"))
                                bp2.pos++;
                            if(bp2.pos<bp2.len&&(bp2.src[bp2.pos]==';'||bp2.src[bp2.pos]=='\n'))
                                bp2.pos++;
                        }
                    }
                }
            }

            /* compile body — but we need to intercept case/default labels */
            /* We'll do it by injecting a case_label_index into the sw_stack */
            /* and using a special body-compile that emits the right labels */

            /* Store case labels so compile_block_src can look them up */
            /* Simpler: compile the body now but emit placeholder labels */
            int case_idx=0;
            /* save current sw_stack state */
            if(ncases>0)
                strncpy(sw_stack[sw_depth-1].pending_lbl, cases[0].lbl, MAX_LABEL_LEN-1);

            /* Compile body using a modified parser that handles case: */
            {
                SrcParser bp2; bp2.src=blk; bp2.pos=0; bp2.len=(int)strlen(blk);
                int ci=0; /* case index */
                while(bp2.pos<bp2.len) {
                    sp_skip(&bp2);
                    if(bp2.pos>=bp2.len)break;
                    if(sp_starts(&bp2,"case")) {
                        bp2.pos+=4;
                        /* skip to ':' */
                        while(bp2.pos<bp2.len&&bp2.src[bp2.pos]!=':')bp2.pos++;
                        if(bp2.pos<bp2.len)bp2.pos++;
                        /* emit case body label */
                        if(ci<ncases) emit("%s:", cases[ci].lbl);
                        ci++;
                        continue;
                    }
                    if(sp_starts(&bp2,"default")) {
                        bp2.pos+=7; sp_skip(&bp2);
                        if(bp2.pos<bp2.len&&bp2.src[bp2.pos]==':')bp2.pos++;
                        /* find default in cases array */
                        for(int di=0;di<ncases;di++) {
                            if(cases[di].is_default) { emit("%s:", cases[di].lbl); break; }
                        }
                        ci++;
                        continue;
                    }
                    if(bp2.src[bp2.pos]=='}')break;

                    /* regular statement */
                    if(sp_peek(&bp2)=='{') {
                        char inner[MAX_SRC/8]; sp_read_block(&bp2,inner,sizeof(inner));
                        int sv2=scope_save_nvar();
                        compile_block_src(inner,(int)strlen(inner));
                        scope_restore_nvar(sv2);
                    } else {
                        char st[MAX_LINE]; sp_read_stmt(&bp2,st,sizeof(st));
                        compile_stmt_str(st);
                    }
                }
            }
            (void)case_idx;

            /* emit dispatch block */
            emit("    jmp %s", lbl_end); /* fall off end of last case */
            emit("%s:", lbl_dispatch);
            for(int ci=0;ci<ncases;ci++) {
                if(cases[ci].is_default) continue;
                tmp_depth=0;
                int vr=alloc_tmp();
                emit("    mov x%d, %lld", vr, cases[ci].val);
                emit("    cmp x%d, x%d", sr, vr);
                free_tmp();
                emit("    jeq %s", cases[ci].lbl);
            }
            /* emit default jump (if any) */
            for(int ci=0;ci<ncases;ci++) {
                if(cases[ci].is_default) { emit("    jmp %s", cases[ci].lbl); break; }
            }
            emit("    jmp %s", lbl_end); /* no match, no default */

            pop_loop();
            sw_depth--;
            emit("%s:", lbl_end);
            continue;
        }

        /* ── function definition ── */
        {
            const char *look=p->src+p->pos;
            if(is_type_kw(look)) {
                const char *body=skip_type(look);
                while(*body&&isspace((unsigned char)*body))body++;
                if(isalpha((unsigned char)*body)||*body=='_') {
                    const char *nstart=body;
                    while(*body&&(isalnum((unsigned char)*body)||*body=='_'))body++;
                    int namelen=(int)(body-nstart); if(namelen>63)namelen=63;
                    char fname[64]; strncpy(fname,nstart,namelen); fname[namelen]='\0';
                    while(*body&&isspace((unsigned char)*body))body++;
                    if(*body=='(') {
                        const char *pe=skip_balanced(body,'(',')');
                        while(*pe&&isspace((unsigned char)*pe))pe++;
                        if(*pe=='{') {
                            /* It's a function definition */
                            p->pos=(int)(body-p->src);

                            /* determine return type */
                            CType rtype=TY_INT;
                            { const char *ts=look;
                              while(*ts&&isspace((unsigned char)*ts))ts++;
                              if(strncmp(ts,"void",4)==0&&
                                 !isalnum((unsigned char)ts[4])&&ts[4]!='_') rtype=TY_VOID; }

                            FuncInfo *fi=add_func(fname);
                            fi->ret_type=rtype;

                            int is_main=(strcmp(fname,"main")==0);

                            /* save globals before compiling function */
                            int saved_nvar=nvar;
                            int saved_next_reg=next_user_reg;
                            FuncInfo *saved_func=current_func;
                            current_func=fi;
                            ngoto=0;

                            /* read and parse parameter list */
                            char param_str[MAX_LINE]; sp_read_parens(p,param_str,sizeof(param_str));
                            sp_skip(p);

                            if(!is_main) emit("%s:", fname);

                            /* parse parameters */
                            char ps[MAX_LINE]; strncpy(ps,param_str,MAX_LINE-1);
                            char *pp=ps; int pi2=0;
                            while(*pp&&pi2<MAX_PARAMS) {
                                while(*pp&&isspace((unsigned char)*pp))pp++;
                                if(!*pp||*pp==')')break;
                                if(strcmp(pp,"void")==0||strncmp(pp,"void)",5)==0)break;
                                const char *pb=skip_type(pp);
                                while(*pb&&isspace((unsigned char)*pb))pb++;
                                while(*pb=='*')pb++;
                                char pname[64]; int pni=0;
                                while(*pb&&(isalnum((unsigned char)*pb)||*pb=='_'))pname[pni++]=(char)*pb++;
                                pname[pni]='\0';
                                pp=(char*)pb;
                                while(*pp&&*pp!=','&&*pp!=')')pp++;
                                if(*pp==',')pp++;
                                if(!*pname||strcmp(pname,"void")==0)continue;
                                Var *pv=add_var(pname,TY_INT);
                                emit("    mov x%d, x%d", pv->reg, VREG_ARG_LO+pi2);
                                strncpy(fi->param_names[pi2],pname,63);
                                fi->param_types[pi2]=TY_INT;
                                fi->nparams++; pi2++;
                            }

                            /* compile body */
                            char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
                            compile_block_src(blk,(int)strlen(blk));

                            /* restore */
                            nvar=saved_nvar;
                            next_user_reg=saved_next_reg;
                            current_func=saved_func;
                            continue;
                        }
                    }
                }
            }
        }

        /* ── user-defined goto label: "labelname:" ── */
        {
            const char *look=p->src+p->pos;
            /* check if it looks like "ident:" */
            if(isalpha((unsigned char)*look)||*look=='_') {
                const char *q=look;
                while(*q&&(isalnum((unsigned char)*q)||*q=='_'))q++;
                while(*q&&isspace((unsigned char)*q))q++;
                if(*q==':') {
                    int namelen=(int)(q-look)-((*(q-1)==' ')?1:0);
                    /* re-measure without spaces */
                    namelen=(int)(q-look);
                    while(namelen>0&&isspace((unsigned char)look[namelen-1]))namelen--;
                    char cname[64]; int ni=namelen<63?namelen:63;
                    strncpy(cname,look,ni); cname[ni]='\0';
                    char iwa_lbl[MAX_LABEL_LEN]; new_label(iwa_lbl,"gl");
                    add_goto_label(cname, iwa_lbl);
                    emit("%s:", iwa_lbl);
                    p->pos=(int)(q-p->src)+1; /* skip past ':' */
                    continue;
                }
            }
        }

        /* ── nested block ── */
        if(c=='{') {
            char blk[MAX_SRC/4]; sp_read_block(p,blk,sizeof(blk));
            int sv=scope_save_nvar();
            compile_block_src(blk,(int)strlen(blk));
            scope_restore_nvar(sv);
            continue;
        }

        /* ── regular statement ── */
        {
            char st[MAX_LINE];
            if(!sp_read_stmt(p,st,sizeof(st))) { p->pos++; continue; }
            compile_stmt_str(st);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    if(argc<3) {
        fprintf(stderr,"c2iwa v3 — C to IWA transpiler\n");
        fprintf(stderr,"Usage: %s <input.c> <output.iwa>\n",argv[0]);
        return 1;
    }

    FILE *fin=fopen(argv[1],"r");
    if(!fin){ perror(argv[1]); return 1; }
    char *src=(char*)malloc(MAX_SRC);
    if(!src){ fprintf(stderr,"Out of memory\n"); return 1; }
    int slen=(int)fread(src,1,MAX_SRC-1,fin);
    src[slen]='\0'; fclose(fin);

    /* Remove preprocessor directives */
    char *clean=(char*)malloc(MAX_SRC);
    if(!clean){ fprintf(stderr,"Out of memory\n"); return 1; }
    int ci=0;
    for(int i=0;i<slen;) {
        if((i==0||src[i-1]=='\n')&&src[i]=='#') {
            /* handle line continuation */
            while(i<slen&&src[i]!='\n') {
                if(src[i]=='\\'&&src[i+1]=='\n'){ i+=2; continue; }
                i++;
            }
        } else {
            clean[ci++]=src[i++];
        }
    }
    clean[ci]='\0';
    free(src);

    emit("; 2WA generated structure");
    emit("; Source of %s", argv[1]);
    emit("; ; ; ; ;");

    compile_block_src(clean, ci);
    emit("    halt");

    free(clean);

    FILE *fout=fopen(argv[2],"w");
    if(!fout){ perror(argv[2]); return 1; }
    for(int i=0;i<out_n;i++) fprintf(fout,"%s\n",out_buf[i]);
    fclose(fout);

    fprintf(stdout,"Transpiled %d lines -> %s\n", out_n, argv[2]);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

#define MAX_SRC      (1 << 22)   /* 4 MB source */
#define MAX_OUT      (1 << 23)   /* 8 MB output */
#define MAX_IDENT    128
#define MAX_VARS     256
#define MAX_FUNCS    128
#define MAX_PARAMS   16
#define MAX_LABELS   2048
#define MAX_BREAKS   128
#define MAX_CONTS    128
#define EXPR_REGS    20          /* x0–x19 for temporaries */

typedef enum {
    TK_EOF=0,
    TK_IDENT, TK_INT_LIT, TK_STR_LIT, TK_CHAR_LIT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE, TK_BANG,
    TK_LSHIFT, TK_RSHIFT,
    TK_AMPAMP, TK_PIPEPIPE,
    TK_EQ, TK_NEQ, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_ASSIGN, TK_PLUS_ASSIGN, TK_MINUS_ASSIGN,
    TK_STAR_ASSIGN, TK_SLASH_ASSIGN, TK_PERCENT_ASSIGN,
    TK_PLUSPLUS, TK_MINUSMINUS,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_SEMICOLON, TK_COMMA, TK_DOT, TK_ARROW,
    TK_DCOLON,   /* :: */
    TK_LANGLE_LANGLE, /* already TK_LSHIFT; kept for clarity */
    /* keywords */
    TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_DO,
    TK_RETURN, TK_BREAK, TK_CONTINUE,
    TK_INT, TK_LONG, TK_CHAR, TK_BOOL, TK_VOID,
    TK_CONST, TK_STATIC, TK_UNSIGNED, TK_SIGNED,
    TK_STRING_TYPE,  /* std::string / string */
    TK_TRUE, TK_FALSE,
    TK_ENDL,         /* std::endl */
    TK_NAMESPACE_STD,/* std */
} TkType;

typedef struct {
    TkType  type;
    char    text[MAX_IDENT];
    long long ival;
    char    sval[1024];
    int     line;
} Token;

/* ── variable info ───────────────────────────────────────────────────────── */
typedef enum { VT_INT=0, VT_LONG, VT_CHAR, VT_BOOL, VT_STRING, VT_VOID } VarType;

typedef struct {
    char    name[MAX_IDENT];
    VarType vtype;
    int     scope;       /* 0=global, 1=local */
    int     slot;        /* register x20+slot for locals; -1 = stack */
    int     is_param;
    int     stack_off;   /* byte offset from sp (if slot==-1) */
} Var;

/* ── function info ───────────────────────────────────────────────────────── */
typedef struct {
    char    name[MAX_IDENT];
    VarType ret_type;
    int     nparams;
    char    param_names[MAX_PARAMS][MAX_IDENT];
    VarType param_types[MAX_PARAMS];
} Func;

/* ── global compiler state ───────────────────────────────────────────────── */
static char  *src;
static int    src_pos;
static int    src_len;
static int    cur_line;

static Token  tok;          /* current lookahead */
static Token  peek;         /* one extra peek */
static int    peek_valid;

static char  *out_buf;
static int    out_pos;

static Var    vars[MAX_VARS];
static int    nvar;

static Func   funcs[MAX_FUNCS];
static int    nfunc;

static int    cur_scope;     /* 0=global, 1=inside function */
static int    label_counter;
static int    str_counter;   /* for string literal labels */
static int    local_slot;    /* next register slot (x20+slot) */
static int    temp_reg;      /* next temporary expression register */

/* break/continue target stacks */
static int    break_stack[MAX_BREAKS];
static int    break_top;
static int    cont_stack[MAX_CONTS];
static int    cont_top;

static int    cur_func_idx;  /* index into funcs[] */

/* ── output helpers ──────────────────────────────────────────────────────── */
static void out(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    out_pos += vsnprintf(out_buf + out_pos, MAX_OUT - out_pos - 1, fmt, ap);
    va_end(ap);
}

/* ── label generation ────────────────────────────────────────────────────── */
static int new_label(void) { return label_counter++; }
static void emit_label(int n) { out(".L%d:\n", n); }

/* ── temporary register allocator ───────────────────────────────────────── */
/* Simple watermark: callers save/restore by recording temp_reg before a call */
static int alloc_tmp(void) {
    if (temp_reg >= EXPR_REGS) {
        fprintf(stderr, "Too many nested expression temporaries (line %d)\n", cur_line);
        exit(1);
    }
    return temp_reg++;
}
static void free_tmp(int r) {
    if (r == temp_reg - 1) temp_reg--;
}
static const char *xreg(int n) {
    static char buf[8][16];
    static int bi=0;
    bi=(bi+1)%8;
    snprintf(buf[bi], 16, "x%d", n);
    return buf[bi];
}

/* ── variable lookup / creation ──────────────────────────────────────────── */
static Var *find_var(const char *name) {
    /* search innermost scope first */
    for (int i = nvar-1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    return NULL;
}

static Var *add_var(const char *name, VarType vt, int scope) {
    if (nvar >= MAX_VARS) { fprintf(stderr,"Too many variables\n"); exit(1); }
    Var *v = &vars[nvar++];
    strncpy(v->name, name, MAX_IDENT-1);
    v->vtype  = vt;
    v->scope  = scope;
    v->is_param = 0;
    if (scope == 0) {
        v->slot = -2;  /* global: lives in string map */
    } else {
        /* assign register slot x20..x27 (8 slots); overflow → stack */
        if (local_slot < 8) {
            v->slot = local_slot++;
        } else {
            v->slot = -1;
            v->stack_off = (local_slot - 8) * 8;
            local_slot++;
        }
    }
    return v;
}

/* String to hold the register name for a variable */
static void var_reg(const Var *v, char *buf, int bsz) {
    if (v->scope == 0) {
        /* global: we use a synthetic label-named register via lds/adl */
        snprintf(buf, bsz, "x28"); /* loaded on demand */
    } else if (v->slot >= 0) {
        snprintf(buf, bsz, "x%d", 20 + v->slot);
    } else {
        snprintf(buf, bsz, "x28"); /* loaded from stack on demand */
    }
}

/* Emit code to load variable into a fresh temp register; return reg index */
static int load_var(const Var *v) {
    int r = alloc_tmp();
    if (v->scope == 0) {
        /* global: we store globals as named string/int in a pseudo-global area.
         * We use a label like _g_<name> for globals and ldr from it.
         * For simplicity we use the string store: lds sets both string and int. */
        out("    ldr x%d, [sp, #-8]  ; load global %s (simplified)\n", r, v->name);
        /* Actually: globals are emitted as labels; we use adr+ldr */
        /* Real emit: */
        out("    ; NOTE: global '%s' — use adr/ldr pattern\n", v->name);
    } else if (v->slot >= 0) {
        out("    mov x%d, x%d  ; load var %s\n", r, 20+v->slot, v->name);
    } else {
        out("    ldr x%d, [sp, #%d]  ; load spilled var %s\n", r, v->stack_off, v->name);
    }
    return r;
}

static void store_var(const Var *v, int src_r) {
    if (v->scope == 0) {
        out("    ; store global %s (simplified)\n", v->name);
    } else if (v->slot >= 0) {
        out("    mov x%d, x%d  ; store var %s\n", 20+v->slot, src_r, v->name);
    } else {
        out("    str x%d, [sp, #%d]  ; spill var %s\n", src_r, v->stack_off, v->name);
    }
}

/* ── lexer ───────────────────────────────────────────────────────────────── */
static void skip_whitespace_comments(void) {
restart:
    while (src_pos < src_len && isspace((unsigned char)src[src_pos])) {
        if (src[src_pos] == '\n') cur_line++;
        src_pos++;
    }
    if (src_pos+1 < src_len && src[src_pos]=='/' && src[src_pos+1]=='/') {
        while (src_pos < src_len && src[src_pos] != '\n') src_pos++;
        goto restart;
    }
    if (src_pos+1 < src_len && src[src_pos]=='/' && src[src_pos+1]=='*') {
        src_pos += 2;
        while (src_pos+1 < src_len &&
               !(src[src_pos]=='*' && src[src_pos+1]=='/')) {
            if (src[src_pos]=='\n') cur_line++;
            src_pos++;
        }
        src_pos += 2;
        goto restart;
    }
}

static Token lex_one(void) {
    skip_whitespace_comments();
    Token t; memset(&t, 0, sizeof t);
    t.line = cur_line;

    if (src_pos >= src_len) { t.type=TK_EOF; return t; }

    char c = src[src_pos];

    /* string literal */
    if (c == '"') {
        src_pos++;
        int i=0;
        while (src_pos < src_len && src[src_pos] != '"') {
            if (src[src_pos]=='\\' && src_pos+1<src_len) {
                src_pos++;
                switch(src[src_pos]) {
                    case 'n': t.sval[i++]='\n'; break;
                    case 't': t.sval[i++]='\t'; break;
                    case '\\':t.sval[i++]='\\'; break;
                    case '"': t.sval[i++]='"';  break;
                    default:  t.sval[i++]='\\'; t.sval[i++]=src[src_pos]; break;
                }
            } else {
                t.sval[i++]=src[src_pos];
            }
            src_pos++;
        }
        t.sval[i]='\0';
        if (src_pos < src_len) src_pos++;
        t.type=TK_STR_LIT;
        return t;
    }

    /* char literal */
    if (c == '\'') {
        src_pos++;
        if (src_pos < src_len && src[src_pos]=='\\') {
            src_pos++;
            switch(src[src_pos]) {
                case 'n': t.ival='\n'; break;
                case 't': t.ival='\t'; break;
                case '0': t.ival='\0'; break;
                default:  t.ival=src[src_pos]; break;
            }
        } else {
            t.ival=(unsigned char)src[src_pos];
        }
        src_pos++;
        if (src_pos<src_len && src[src_pos]=='\'') src_pos++;
        t.type=TK_CHAR_LIT;
        return t;
    }

    /* integer literal */
    if (isdigit((unsigned char)c)) {
        char buf[64]; int i=0;
        int base=10;
        if (c=='0' && src_pos+1<src_len &&
            (src[src_pos+1]=='x'||src[src_pos+1]=='X')) {
            src_pos+=2; base=16;
            while (src_pos<src_len && isxdigit((unsigned char)src[src_pos]))
                buf[i++]=src[src_pos++];
        } else {
            while (src_pos<src_len && isdigit((unsigned char)src[src_pos]))
                buf[i++]=src[src_pos++];
        }
        buf[i]='\0';
        t.ival=strtoll(buf,NULL,base);
        t.type=TK_INT_LIT;
        /* skip L/LL/u/U suffixes */
        while (src_pos<src_len && (src[src_pos]=='L'||src[src_pos]=='l'||
               src[src_pos]=='u'||src[src_pos]=='U')) src_pos++;
        return t;
    }

    /* identifiers / keywords */
    if (isalpha((unsigned char)c) || c=='_') {
        int i=0;
        while (src_pos<src_len && (isalnum((unsigned char)src[src_pos])||src[src_pos]=='_'))
            t.text[i++]=src[src_pos++];
        t.text[i]='\0';
        /* keyword check */
        if (!strcmp(t.text,"if"))       { t.type=TK_IF; return t; }
        if (!strcmp(t.text,"else"))     { t.type=TK_ELSE; return t; }
        if (!strcmp(t.text,"while"))    { t.type=TK_WHILE; return t; }
        if (!strcmp(t.text,"for"))      { t.type=TK_FOR; return t; }
        if (!strcmp(t.text,"do"))       { t.type=TK_DO; return t; }
        if (!strcmp(t.text,"return"))   { t.type=TK_RETURN; return t; }
        if (!strcmp(t.text,"break"))    { t.type=TK_BREAK; return t; }
        if (!strcmp(t.text,"continue")) { t.type=TK_CONTINUE; return t; }
        if (!strcmp(t.text,"int"))      { t.type=TK_INT; return t; }
        if (!strcmp(t.text,"long"))     { t.type=TK_LONG; return t; }
        if (!strcmp(t.text,"char"))     { t.type=TK_CHAR; return t; }
        if (!strcmp(t.text,"bool"))     { t.type=TK_BOOL; return t; }
        if (!strcmp(t.text,"void"))     { t.type=TK_VOID; return t; }
        if (!strcmp(t.text,"const"))    { t.type=TK_CONST; return t; }
        if (!strcmp(t.text,"static"))   { t.type=TK_STATIC; return t; }
        if (!strcmp(t.text,"unsigned")) { t.type=TK_UNSIGNED; return t; }
        if (!strcmp(t.text,"signed"))   { t.type=TK_SIGNED; return t; }
        if (!strcmp(t.text,"string"))   { t.type=TK_STRING_TYPE; return t; }
        if (!strcmp(t.text,"true"))     { t.type=TK_TRUE; t.ival=1; return t; }
        if (!strcmp(t.text,"false"))    { t.type=TK_FALSE; t.ival=0; return t; }
        if (!strcmp(t.text,"endl"))     { t.type=TK_ENDL; return t; }
        if (!strcmp(t.text,"std"))      { t.type=TK_NAMESPACE_STD; return t; }
        t.type=TK_IDENT;
        return t;
    }

    src_pos++;
    switch(c) {
        case '+':
            if (src_pos<src_len && src[src_pos]=='+') { src_pos++; t.type=TK_PLUSPLUS; }
            else if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_PLUS_ASSIGN; }
            else t.type=TK_PLUS;
            return t;
        case '-':
            if (src_pos<src_len && src[src_pos]=='-') { src_pos++; t.type=TK_MINUSMINUS; }
            else if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_MINUS_ASSIGN; }
            else if (src_pos<src_len && src[src_pos]=='>') { src_pos++; t.type=TK_ARROW; }
            else t.type=TK_MINUS;
            return t;
        case '*':
            if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_STAR_ASSIGN; }
            else t.type=TK_STAR;
            return t;
        case '/':
            if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_SLASH_ASSIGN; }
            else t.type=TK_SLASH;
            return t;
        case '%':
            if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_PERCENT_ASSIGN; }
            else t.type=TK_PERCENT;
            return t;
        case '&':
            if (src_pos<src_len && src[src_pos]=='&') { src_pos++; t.type=TK_AMPAMP; }
            else t.type=TK_AMP;
            return t;
        case '|':
            if (src_pos<src_len && src[src_pos]=='|') { src_pos++; t.type=TK_PIPEPIPE; }
            else t.type=TK_PIPE;
            return t;
        case '^': t.type=TK_CARET; return t;
        case '~': t.type=TK_TILDE; return t;
        case '!':
            if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_NEQ; }
            else t.type=TK_BANG;
            return t;
        case '<':
            if (src_pos<src_len && src[src_pos]=='<') { src_pos++; t.type=TK_LSHIFT; }
            else if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_LE; }
            else t.type=TK_LT;
            return t;
        case '>':
            if (src_pos<src_len && src[src_pos]=='>') { src_pos++; t.type=TK_RSHIFT; }
            else if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_GE; }
            else t.type=TK_GT;
            return t;
        case '=':
            if (src_pos<src_len && src[src_pos]=='=') { src_pos++; t.type=TK_EQ; }
            else t.type=TK_ASSIGN;
            return t;
        case '(': t.type=TK_LPAREN; return t;
        case ')': t.type=TK_RPAREN; return t;
        case '{': t.type=TK_LBRACE; return t;
        case '}': t.type=TK_RBRACE; return t;
        case '[': t.type=TK_LBRACKET; return t;
        case ']': t.type=TK_RBRACKET; return t;
        case ';': t.type=TK_SEMICOLON; return t;
        case ',': t.type=TK_COMMA; return t;
        case '.': t.type=TK_DOT; return t;
        case ':':
            if (src_pos<src_len && src[src_pos]==':') { src_pos++; t.type=TK_DCOLON; }
            return t;
        case '#':
            /* preprocessor directive — skip the whole line */
            while (src_pos<src_len && src[src_pos]!='\n') src_pos++;
            return lex_one();
        default:
            /* skip unknown */
            return lex_one();
    }
}

static void next(void) {
    if (peek_valid) { tok=peek; peek_valid=0; }
    else            tok=lex_one();
}

/* look at the token after current without consuming current */
static Token peek_tok(void) {
    if (!peek_valid) { peek=lex_one(); peek_valid=1; }
    return peek;
}

static void expect(TkType type) {
    if (tok.type != type) {
        fprintf(stderr, "Line %d: expected token type %d, got %d ('%s')\n",
                cur_line, type, tok.type, tok.text);
        exit(1);
    }
    next();
}

/* ── forward declarations ────────────────────────────────────────────────── */
static int parse_expr(void);        /* returns temp reg holding result */
static void parse_stmt(void);
static void parse_block(void);

/* ── type parser ─────────────────────────────────────────────────────────── */
static VarType parse_type(void) {
    /* consume optional qualifiers */
    while (tok.type==TK_CONST||tok.type==TK_STATIC||
           tok.type==TK_UNSIGNED||tok.type==TK_SIGNED) next();

    /* std:: prefix */
    if (tok.type==TK_NAMESPACE_STD) {
        next();
        if (tok.type==TK_DCOLON) next();
    }

    VarType vt=VT_INT;
    switch(tok.type) {
        case TK_INT:         vt=VT_INT;    next(); break;
        case TK_LONG:        vt=VT_LONG;   next();
                             if(tok.type==TK_LONG) next(); /* long long */
                             break;
        case TK_CHAR:        vt=VT_CHAR;   next(); break;
        case TK_BOOL:        vt=VT_BOOL;   next(); break;
        case TK_VOID:        vt=VT_VOID;   next(); break;
        case TK_STRING_TYPE: vt=VT_STRING; next(); break;
        default:
            fprintf(stderr,"Line %d: expected type, got '%s' (type %d)\n",
                    cur_line, tok.text, tok.type);
            exit(1);
    }
    return vt;
}

/* check whether current token starts a type */
static int is_type_start(void) {
    TkType t=tok.type;
    if(t==TK_INT||t==TK_LONG||t==TK_CHAR||t==TK_BOOL||t==TK_VOID||
       t==TK_CONST||t==TK_STATIC||t==TK_UNSIGNED||t==TK_SIGNED||
       t==TK_STRING_TYPE) return 1;
    /* std:: — only a type if followed by string, not cout/cin/endl */
    if(t==TK_NAMESPACE_STD) {
        Token nx=peek_tok();
        if(nx.type==TK_DCOLON) {
            /* peek one more token — but we only have one-token peek.
             * Heuristic: if it's just "std::" and the next after :: is
             * string/cout/cin, we check the source directly for "cout" or "cin" */
            /* Safe heuristic: save pos, lex past ::, check next ident */
            int sp=src_pos, sl=cur_line;
            /* skip whitespace after current :: */
            int tmp_pos=src_pos;
            /* skip the :: tokens that haven't been consumed yet */
            /* Actually peek only shows us the :: token. We check the raw source. */
            /* skip whitespace */
            while(tmp_pos<src_len && isspace((unsigned char)src[tmp_pos])) tmp_pos++;
            /* skip :: */
            if(tmp_pos+1<src_len && src[tmp_pos]==':' && src[tmp_pos+1]==':')
                tmp_pos+=2;
            while(tmp_pos<src_len && isspace((unsigned char)src[tmp_pos])) tmp_pos++;
            /* read word */
            char word[32]=""; int wi=0;
            while(tmp_pos<src_len && isalpha((unsigned char)src[tmp_pos]) && wi<31)
                word[wi++]=src[tmp_pos++];
            word[wi]=0;
            if(!strcmp(word,"cout")||!strcmp(word,"cin")||!strcmp(word,"endl"))
                return 0; /* statement, not type */
        }
        return 1;
    }
    return 0;
}

/* ── expression parser (recursive descent, Pratt-ish) ───────────────────── */

/*
 * Each parse_* function returns the index of the temp register (x0..x19)
 * holding the result.  The caller is responsible for freeing temps it no
 * longer needs via free_tmp().
 */

/* primary: literal, identifier, (expr), function call */
static int parse_primary(void) {

    /* integer literal */
    if (tok.type==TK_INT_LIT||tok.type==TK_CHAR_LIT) {
        long long v=tok.ival; next();
        int r=alloc_tmp();
        out("    mov x%d, #%lld\n", r, v);
        return r;
    }
    if (tok.type==TK_TRUE)  { next(); int r=alloc_tmp(); out("    mov x%d, #1\n",r); return r; }
    if (tok.type==TK_FALSE) { next(); int r=alloc_tmp(); out("    mov x%d, #0\n",r); return r; }

    /* string literal */
    if (tok.type==TK_STR_LIT) {
        char sbuf[1024]; strncpy(sbuf,tok.sval,1023); next();
        int r=alloc_tmp();
        /* escape for lds */
        char escaped[2048]; int ei=0;
        for(int i=0;sbuf[i];i++){
            if(sbuf[i]=='\n'){escaped[ei++]='\\';escaped[ei++]='n';}
            else if(sbuf[i]=='\t'){escaped[ei++]='\\';escaped[ei++]='t';}
            else escaped[ei++]=sbuf[i];
        }
        escaped[ei]='\0';
        out("    lds x%d, \"%s\"\n", r, escaped);
        return r;
    }

    /* endl */
    if (tok.type==TK_ENDL) {
        next();
        int r=alloc_tmp();
        out("    lds x%d, \"\\n\"\n", r);
        return r;
    }

    /* parenthesised expression */
    if (tok.type==TK_LPAREN) {
        next();
        int r=parse_expr();
        expect(TK_RPAREN);
        return r;
    }

    /* std:: namespace prefix — eat it and continue */
    if (tok.type==TK_NAMESPACE_STD) {
        next();
        if (tok.type==TK_DCOLON) next();
        /* after eating std:: check for endl directly */
        if (tok.type==TK_ENDL) {
            next();
            int r=alloc_tmp();
            out("    lds x%d, \"\\n\"\n", r);
            return r;
        }
        /* fall through to identifier */
    }

    if (tok.type==TK_IDENT) {
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();

        /* function call */
        if (tok.type==TK_LPAREN) {
            next();
            /* collect arguments */
            int args[MAX_PARAMS], nargs=0;
            while (tok.type!=TK_RPAREN && tok.type!=TK_EOF) {
                args[nargs++]=parse_expr();
                if (tok.type==TK_COMMA) next();
            }
            expect(TK_RPAREN);

            /* Built-in printf */
            if (!strcmp(name,"printf")) {
                if (nargs>=1) {
                    /* if first arg is a string reg, puts it */
                    out("    puts x%d\n", args[0]);
                }
                for(int i=0;i<nargs;i++) free_tmp(args[i]);
                int r=alloc_tmp(); out("    mov x%d, #0\n",r); return r;
            }
            /* Built-in scanf — read one integer into the second arg var */
            if (!strcmp(name,"scanf")) {
                if (nargs>=2) {
                    out("    geti x%d\n", args[1]);
                }
                for(int i=0;i<nargs;i++) free_tmp(args[i]);
                int r=alloc_tmp(); out("    mov x%d, #0\n",r); return r;
            }

            /* pass args in x0..xN (ABI: first 8 params in x0-x7) */
            for(int i=0;i<nargs && i<8;i++) {
                if(args[i]!=i) out("    mov x%d, x%d\n", i, args[i]);
            }
            for(int i=0;i<nargs;i++) free_tmp(args[i]);
            out("    call %s\n", name);
            /* result is in x0 — move to a fresh temp */
            int r=alloc_tmp();
            if(r!=0) out("    mov x%d, x0\n", r);
            return r;
        }

        /* variable read */
        Var *v=find_var(name);
        if (!v) {
            fprintf(stderr,"Line %d: undefined variable '%s'\n",cur_line,name);
            exit(1);
        }
        int r=alloc_tmp();
        if (v->scope==0) {
            /* global — we don't implement full global storage here;
             * store globals in high register x28 by convention with a
             * named load. We emit a comment explaining the limitation. */
            out("    ; load global %s into x%d\n", name, r);
            out("    mov x%d, x%d  ; global placeholder\n", r, 20+0);
        } else if (v->slot>=0) {
            out("    mov x%d, x%d  ; load %s\n", r, 20+v->slot, name);
        } else {
            out("    ldr x%d, [sp, #%d]  ; load spilled %s\n", r, v->stack_off, name);
        }
        return r;
    }

    fprintf(stderr,"Line %d: unexpected token in expression: type=%d text='%s'\n",
            cur_line, tok.type, tok.text);
    exit(1);
}

/* unary */
static int parse_unary(void) {
    if (tok.type==TK_MINUS) {
        next();
        int r=parse_unary();
        out("    neg x%d, x%d\n", r, r);
        return r;
    }
    if (tok.type==TK_BANG) {
        next();
        int r=parse_unary();
        int t2=alloc_tmp();
        out("    cmp x%d, #0\n", r);
        out("    cset x%d, eq\n", t2);
        free_tmp(r);
        return t2;
    }
    if (tok.type==TK_TILDE) {
        next();
        int r=parse_unary();
        out("    not x%d\n", r);
        return r;
    }
    /* pre-increment / pre-decrement */
    if (tok.type==TK_PLUSPLUS||tok.type==TK_MINUSMINUS) {
        int is_inc=(tok.type==TK_PLUSPLUS); next();
        /* next token must be an identifier */
        if(tok.type!=TK_IDENT){fprintf(stderr,"Line %d: expected lvalue after ++/--\n",cur_line);exit(1);}
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();
        Var *v=find_var(name);
        if(!v){fprintf(stderr,"Line %d: undefined '%s'\n",cur_line,name);exit(1);}
        int r=alloc_tmp();
        if(v->slot>=0) {
            int vr=20+v->slot;
            if(is_inc) out("    inc x%d\n",vr);
            else        out("    dec x%d\n",vr);
            out("    mov x%d, x%d\n",r,vr);
        } else {
            out("    ldr x%d, [sp, #%d]\n",r,v->stack_off);
            if(is_inc) out("    inc x%d\n",r);
            else        out("    dec x%d\n",r);
            out("    str x%d, [sp, #%d]\n",r,v->stack_off);
        }
        return r;
    }
    int r=parse_primary();
    /* post-increment / post-decrement */
    if (tok.type==TK_PLUSPLUS||tok.type==TK_MINUSMINUS) {
        /* We need the original value; the side-effect happens after */
        int is_inc=(tok.type==TK_PLUSPLUS); next();
        /* The previous parse_primary already loaded the var into r.
         * We need to identify WHICH variable that was — we do this by
         * checking what parse_primary emitted.  Since we don't have SSA
         * tracking, we just peek at the last identifier before the ++ */
        /* Simpler: post inc on a var: save old, increment slot */
        /* Actually we need to re-identify the variable. We can do this
         * by storing the last-read variable name in a global. */
        /* For now emit a generic inc on the same register and note that
         * this won't write back to the var properly in all cases.
         * A full implementation would need an lvalue system. */
        int old=alloc_tmp();
        out("    mov x%d, x%d  ; save pre-increment value\n", old, r);
        if(is_inc) out("    inc x%d\n", r);
        else        out("    dec x%d\n", r);
        /* We can't write back without knowing the variable.
         * This is handled in the statement parser for standalone i++; */
        free_tmp(r);
        return old;
    }
    return r;
}

static char *last_assigned_ident = NULL; /* used for post-inc writeback */
static char _last_ident_buf[MAX_IDENT];

/* multiplicative */
static int parse_mul(void) {
    int l=parse_unary();
    while (tok.type==TK_STAR||tok.type==TK_SLASH||tok.type==TK_PERCENT) {
        TkType op=tok.type; next();
        int r=parse_unary();
        if(op==TK_STAR) {
            out("    mul x%d, x%d, x%d\n",l,l,r);
        } else if(op==TK_SLASH) {
            out("    sdiv x%d, x%d, x%d\n",l,l,r);
        } else {
            out("    mod x%d, x%d, x%d\n",l,l,r);
        }
        free_tmp(r);
    }
    return l;
}

/* additive */
static int parse_add(void) {
    int l=parse_mul();
    while (tok.type==TK_PLUS||tok.type==TK_MINUS) {
        TkType op=tok.type; next();
        int r=parse_mul();
        if(op==TK_PLUS)
            out("    add x%d, x%d, x%d\n",l,l,r);
        else
            out("    sub x%d, x%d, x%d\n",l,l,r);
        free_tmp(r);
    }
    return l;
}

/* shift */
static int parse_shift(void) {
    int l=parse_add();
    while (tok.type==TK_LSHIFT||tok.type==TK_RSHIFT) {
        TkType op=tok.type; next();
        int r=parse_add();
        if(op==TK_LSHIFT)
            out("    lsl x%d, x%d, x%d\n",l,l,r);
        else
            out("    lsr x%d, x%d, x%d\n",l,l,r);
        free_tmp(r);
    }
    return l;
}

/* relational */
static int parse_rel(void) {
    int l=parse_shift();
    if(tok.type==TK_LT||tok.type==TK_LE||tok.type==TK_GT||tok.type==TK_GE) {
        TkType op=tok.type; next();
        int r=parse_shift();
        out("    cmp x%d, x%d\n",l,r);
        int res=alloc_tmp();
        const char *cc = op==TK_LT?"lt":op==TK_LE?"le":op==TK_GT?"gt":"ge";
        out("    cset x%d, %s\n",res,cc);
        free_tmp(l); free_tmp(r);
        return res;
    }
    return l;
}

/* equality */
static int parse_eq(void) {
    int l=parse_rel();
    if(tok.type==TK_EQ||tok.type==TK_NEQ) {
        TkType op=tok.type; next();
        int r=parse_rel();
        out("    cmp x%d, x%d\n",l,r);
        int res=alloc_tmp();
        out("    cset x%d, %s\n",res,op==TK_EQ?"eq":"ne");
        free_tmp(l); free_tmp(r);
        return res;
    }
    return l;
}

/* bitwise and */
static int parse_band(void) {
    int l=parse_eq();
    while(tok.type==TK_AMP) {
        next(); int r=parse_eq();
        out("    and x%d, x%d, x%d\n",l,l,r);
        free_tmp(r);
    }
    return l;
}

/* bitwise xor */
static int parse_bxor(void) {
    int l=parse_band();
    while(tok.type==TK_CARET) {
        next(); int r=parse_band();
        out("    eor x%d, x%d, x%d\n",l,l,r);
        free_tmp(r);
    }
    return l;
}

/* bitwise or */
static int parse_bor(void) {
    int l=parse_bxor();
    while(tok.type==TK_PIPE) {
        next(); int r=parse_bxor();
        out("    orr x%d, x%d, x%d\n",l,l,r);
        free_tmp(r);
    }
    return l;
}

/* logical and */
static int parse_land(void) {
    int l=parse_bor();
    while(tok.type==TK_AMPAMP) {
        next();
        int end_label=new_label();
        /* short-circuit: if l==0 skip right side */
        out("    cmp x%d, #0\n",l);
        out("    beq .L%d\n",end_label);
        free_tmp(l);
        l=parse_bor();
        out("    cmp x%d, #0\n",l);
        out("    cset x%d, ne\n",l);
        emit_label(end_label);
    }
    return l;
}

/* logical or */
static int parse_lor(void) {
    int l=parse_land();
    while(tok.type==TK_PIPEPIPE) {
        next();
        int end_label=new_label();
        out("    cmp x%d, #0\n",l);
        out("    bne .L%d\n",end_label);
        free_tmp(l);
        l=parse_land();
        out("    cmp x%d, #0\n",l);
        out("    cset x%d, ne\n",l);
        emit_label(end_label);
    }
    return l;
}

/* assignment */
static int parse_assign(void) {
    /* We need to detect "ident <assign_op> expr" patterns */
    int saved_pos=src_pos, saved_line=cur_line;
    Token saved_tok=tok;
    int saved_peek=peek_valid;
    Token saved_peek_tok=peek;

    if(tok.type==TK_IDENT) {
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1);
        Token nx=peek_tok();
        TkType nxt=nx.type;
        if(nxt==TK_ASSIGN||nxt==TK_PLUS_ASSIGN||nxt==TK_MINUS_ASSIGN||
           nxt==TK_STAR_ASSIGN||nxt==TK_SLASH_ASSIGN||nxt==TK_PERCENT_ASSIGN) {
            next(); /* consume ident */
            TkType op=tok.type; next(); /* consume op */
            int rhs=parse_assign();
            Var *v=find_var(name);
            if(!v){fprintf(stderr,"Line %d: undefined '%s'\n",cur_line,name);exit(1);}
            if(op!=TK_ASSIGN) {
                /* load current value */
                int cur=alloc_tmp();
                if(v->slot>=0) out("    mov x%d, x%d\n",cur,20+v->slot);
                else out("    ldr x%d, [sp, #%d]\n",cur,v->stack_off);
                switch(op) {
                    case TK_PLUS_ASSIGN:    out("    add x%d, x%d, x%d\n",rhs,cur,rhs); break;
                    case TK_MINUS_ASSIGN:   out("    sub x%d, x%d, x%d\n",rhs,cur,rhs); break;
                    case TK_STAR_ASSIGN:    out("    mul x%d, x%d, x%d\n",rhs,cur,rhs); break;
                    case TK_SLASH_ASSIGN:   out("    sdiv x%d, x%d, x%d\n",rhs,cur,rhs); break;
                    case TK_PERCENT_ASSIGN: out("    mod x%d, x%d, x%d\n",rhs,cur,rhs); break;
                    default: break;
                }
                free_tmp(cur);
            }
            /* store */
            if(v->slot>=0) out("    mov x%d, x%d  ; store %s\n",20+v->slot,rhs,name);
            else out("    str x%d, [sp, #%d]  ; store %s\n",rhs,v->stack_off,name);
            return rhs;
        }
    }
    return parse_lor();
}

static int parse_expr(void) { return parse_assign(); }

/* ── cout / cin handler ──────────────────────────────────────────────────── */

/*
 * Parse a chain of   << expr  fragments after the initial "cout" has been consumed.
 * Each value is printed; strings via puts, integers via itoa+puts.
 */
/* cout operand: parse up to add level to avoid consuming << as bit-shift */
static int parse_cout_operand(void) { return parse_add(); }

static void parse_cout_chain(void) {
    while(tok.type==TK_LSHIFT) {
        next();
        /* handle std::endl */
        if(tok.type==TK_NAMESPACE_STD) {
            next();
            if(tok.type==TK_DCOLON) next();
        }
        if(tok.type==TK_ENDL) {
            next();
            int r=alloc_tmp();
            out("    lds x%d, \"\\n\"\n",r);
            out("    puts x%d\n",r);
            free_tmp(r);
        } else {
            int r=parse_cout_operand();
            out("    puts x%d\n",r);
            free_tmp(r);
        }
    }
}

/*
 * Parse  >> var  chains after "cin" has been consumed.
 */
static void parse_cin_chain(void) {
    while(tok.type==TK_RSHIFT) {
        next();
        if(tok.type==TK_IDENT) {
            char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();
            Var *v=find_var(name);
            if(!v){fprintf(stderr,"Line %d: undefined '%s'\n",cur_line,name);exit(1);}
            if(v->vtype==VT_STRING) {
                if(v->slot>=0) out("    gets x%d  ; cin >> %s\n",20+v->slot,name);
                else { out("    gets x28\n    str x28, [sp, #%d]\n",v->stack_off); }
            } else {
                if(v->slot>=0) out("    geti x%d  ; cin >> %s\n",20+v->slot,name);
                else { out("    geti x28\n    str x28, [sp, #%d]\n",v->stack_off); }
            }
        }
    }
}

/* ── statement parser ────────────────────────────────────────────────────── */

static void parse_stmt(void) {
    /* variable declaration */
    if (is_type_start()) {
        VarType vt=parse_type();
        /* could be a function definition or variable */
        if(tok.type!=TK_IDENT){
            fprintf(stderr,"Line %d: expected identifier after type\n",cur_line);exit(1);
        }
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();

        /* function definition handled in parse_toplevel; here we only handle local var */
        if(tok.type==TK_LPAREN) {
            fprintf(stderr,"Line %d: unexpected function definition inside function\n",cur_line);
            exit(1);
        }

        Var *v=add_var(name,vt,cur_scope);
        out("    ; var %s (x%d)\n", name, v->slot>=0 ? 20+v->slot : -1);

        if(tok.type==TK_ASSIGN) {
            next();
            int r=parse_expr();
            if(v->slot>=0) {
                if(vt==VT_STRING)
                    out("    mov x%d, x%d  ; init string %s\n",20+v->slot,r,name);
                else
                    out("    mov x%d, x%d  ; init %s\n",20+v->slot,r,name);
                /* copy string metadata */
            } else {
                out("    str x%d, [sp, #%d]  ; init spill %s\n",r,v->stack_off,name);
            }
            /* if string, copy string register state too */
            if(vt==VT_STRING && v->slot>=0) {
                /* The lds instruction sets both int length and string value in the register.
                 * Moving the register copies the integer; string metadata is per-register in VM.
                 * We use strcpy to copy string value to target reg. */
                out("    strcpy x%d, x%d  ; copy string to %s\n",20+v->slot,r,name);
            }
            free_tmp(r);
        } else if(vt==VT_STRING) {
            /* default init to empty string */
            if(v->slot>=0) out("    lds x%d, \"\"\n",20+v->slot);
        } else {
            if(v->slot>=0) out("    mov x%d, #0  ; zero-init %s\n",20+v->slot,name);
            else out("    mov x28, #0\n    str x28, [sp, #%d]\n",v->stack_off);
        }
        expect(TK_SEMICOLON);
        return;
    }

    /* if statement */
    if(tok.type==TK_IF) {
        next();
        expect(TK_LPAREN);
        int cond=parse_expr();
        expect(TK_RPAREN);
        int else_label=new_label(), end_label=new_label();
        out("    cmp x%d, #0\n",cond);
        out("    beq .L%d\n",else_label);
        free_tmp(cond);
        parse_stmt();
        out("    b .L%d\n",end_label);
        emit_label(else_label);
        if(tok.type==TK_ELSE) {
            next(); parse_stmt();
        }
        emit_label(end_label);
        return;
    }

    /* while loop */
    if(tok.type==TK_WHILE) {
        next();
        int loop_label=new_label(), end_label=new_label();
        break_stack[break_top++]=end_label;
        cont_stack[cont_top++]=loop_label;
        emit_label(loop_label);
        expect(TK_LPAREN);
        int cond=parse_expr();
        expect(TK_RPAREN);
        out("    cmp x%d, #0\n",cond);
        out("    beq .L%d\n",end_label);
        free_tmp(cond);
        parse_stmt();
        out("    b .L%d\n",loop_label);
        emit_label(end_label);
        break_top--; cont_top--;
        return;
    }

    /* do-while */
    if(tok.type==TK_DO) {
        next();
        int loop_label=new_label(), end_label=new_label();
        break_stack[break_top++]=end_label;
        cont_stack[cont_top++]=loop_label;
        emit_label(loop_label);
        parse_stmt();
        expect(TK_WHILE);
        expect(TK_LPAREN);
        int cond=parse_expr();
        expect(TK_RPAREN);
        expect(TK_SEMICOLON);
        out("    cmp x%d, #0\n",cond);
        out("    bne .L%d\n",loop_label);
        emit_label(end_label);
        free_tmp(cond);
        break_top--; cont_top--;
        return;
    }

    /* for loop */
    if(tok.type==TK_FOR) {
        next();
        expect(TK_LPAREN);
        int loop_label=new_label(), inc_label=new_label(), end_label=new_label();
        break_stack[break_top++]=end_label;
        cont_stack[cont_top++]=inc_label;

        /* init */
        if(tok.type!=TK_SEMICOLON) {
            if(is_type_start()) {
                /* variable declaration in init */
                VarType vt=parse_type();
                char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();
                Var *v=add_var(name,vt,cur_scope);
                if(tok.type==TK_ASSIGN) {
                    next();
                    int r=parse_expr();
                    if(v->slot>=0) out("    mov x%d, x%d  ; for-init %s\n",20+v->slot,r,name);
                    else out("    str x%d, [sp, #%d]\n",r,v->stack_off);
                    free_tmp(r);
                } else {
                    if(v->slot>=0) out("    mov x%d, #0\n",20+v->slot);
                }
            } else {
                int r=parse_expr(); free_tmp(r);
            }
        }
        expect(TK_SEMICOLON);

        emit_label(loop_label);
        /* condition */
        if(tok.type!=TK_SEMICOLON) {
            int cond=parse_expr();
            out("    cmp x%d, #0\n",cond);
            out("    beq .L%d\n",end_label);
            free_tmp(cond);
        }
        expect(TK_SEMICOLON);

        /* increment: save tokens and emit after body */
        /* We can't easily save tokens, so we emit the increment inline after body.
         * Strategy: emit a jump over the increment, then the body jumps here. */
        /* Actually: standard approach — emit body, then increment, then loop back */
        /* We defer by jumping over the increment block first */
        int body_start=new_label();
        out("    b .L%d\n",body_start);

        emit_label(inc_label);
        if(tok.type!=TK_RPAREN) {
            /* Handle i++ / i-- / ++i / --i as proper writeback statements */
            if(tok.type==TK_IDENT) {
                char iname[MAX_IDENT]; strncpy(iname,tok.text,MAX_IDENT-1);
                Token nx2=peek_tok();
                if(nx2.type==TK_PLUSPLUS||nx2.type==TK_MINUSMINUS) {
                    next(); /* consume ident */
                    int is_inc2=(tok.type==TK_PLUSPLUS); next(); /* consume op */
                    Var *iv=find_var(iname);
                    if(iv) {
                        if(iv->slot>=0) {
                            if(is_inc2) out("    inc x%d  ; for %s++\n",20+iv->slot,iname);
                            else         out("    dec x%d  ; for %s--\n",20+iv->slot,iname);
                        } else {
                            out("    ldr x28, [sp, #%d]\n",iv->stack_off);
                            if(is_inc2) out("    inc x28\n");
                            else         out("    dec x28\n");
                            out("    str x28, [sp, #%d]\n",iv->stack_off);
                        }
                    }
                    goto for_inc_done;
                }
            }
            if(tok.type==TK_PLUSPLUS||tok.type==TK_MINUSMINUS) {
                int is_inc2=(tok.type==TK_PLUSPLUS); next();
                if(tok.type==TK_IDENT) {
                    char iname[MAX_IDENT]; strncpy(iname,tok.text,MAX_IDENT-1); next();
                    Var *iv=find_var(iname);
                    if(iv) {
                        if(iv->slot>=0) {
                            if(is_inc2) out("    inc x%d\n",20+iv->slot);
                            else         out("    dec x%d\n",20+iv->slot);
                        } else {
                            out("    ldr x28, [sp, #%d]\n",iv->stack_off);
                            if(is_inc2) out("    inc x28\n");
                            else         out("    dec x28\n");
                            out("    str x28, [sp, #%d]\n",iv->stack_off);
                        }
                    }
                    goto for_inc_done;
                }
            }
            /* fallback: generic expression (e.g. x += 2) */
            {int r=parse_expr(); free_tmp(r);}
            for_inc_done:;
        }
        out("    b .L%d\n",loop_label);

        expect(TK_RPAREN);
        emit_label(body_start);
        parse_stmt();
        out("    b .L%d\n",inc_label);
        emit_label(end_label);
        break_top--; cont_top--;
        return;
    }

    /* return */
    if(tok.type==TK_RETURN) {
        next();
        if(tok.type!=TK_SEMICOLON) {
            int r=parse_expr();
            if(r!=0) out("    mov x0, x%d  ; return value\n",r);
            free_tmp(r);
        }
        expect(TK_SEMICOLON);
        out("    ret\n");
        return;
    }

    /* break */
    if(tok.type==TK_BREAK) {
        next(); expect(TK_SEMICOLON);
        if(break_top==0){fprintf(stderr,"Line %d: break outside loop\n",cur_line);exit(1);}
        out("    b .L%d\n",break_stack[break_top-1]);
        return;
    }

    /* continue */
    if(tok.type==TK_CONTINUE) {
        next(); expect(TK_SEMICOLON);
        if(cont_top==0){fprintf(stderr,"Line %d: continue outside loop\n",cur_line);exit(1);}
        out("    b .L%d\n",cont_stack[cont_top-1]);
        return;
    }

    /* block */
    if(tok.type==TK_LBRACE) { parse_block(); return; }

    /* empty statement */
    if(tok.type==TK_SEMICOLON) { next(); return; }

    /* expression statement — including cout/cin */

    /* std:: prefix */
    int had_std=0;
    if(tok.type==TK_NAMESPACE_STD) { next(); if(tok.type==TK_DCOLON) next(); had_std=1; }

    /* cout */
    if(tok.type==TK_IDENT && !strcmp(tok.text,"cout")) {
        next();
        parse_cout_chain();
        expect(TK_SEMICOLON);
        return;
    }
    /* cin */
    if(tok.type==TK_IDENT && !strcmp(tok.text,"cin")) {
        next();
        parse_cin_chain();
        expect(TK_SEMICOLON);
        return;
    }

    /* Handle standalone ++i / --i / i++ / i-- as statements properly */
    if(tok.type==TK_IDENT) {
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1);
        Token nx=peek_tok();
        if(nx.type==TK_PLUSPLUS||nx.type==TK_MINUSMINUS) {
            next(); /* consume ident */
            int is_inc=(tok.type==TK_PLUSPLUS); next(); /* consume op */
            Var *v=find_var(name);
            if(!v){fprintf(stderr,"Line %d: undefined '%s'\n",cur_line,name);exit(1);}
            if(v->slot>=0) {
                if(is_inc) out("    inc x%d  ; %s++\n",20+v->slot,name);
                else        out("    dec x%d  ; %s--\n",20+v->slot,name);
            } else {
                out("    ldr x28, [sp, #%d]\n",v->stack_off);
                if(is_inc) out("    inc x28\n");
                else        out("    dec x28\n");
                out("    str x28, [sp, #%d]\n",v->stack_off);
            }
            expect(TK_SEMICOLON);
            return;
        }
    }
    if(tok.type==TK_PLUSPLUS||tok.type==TK_MINUSMINUS) {
        int is_inc=(tok.type==TK_PLUSPLUS); next();
        if(tok.type!=TK_IDENT){fprintf(stderr,"Line %d: expected lvalue\n",cur_line);exit(1);}
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();
        Var *v=find_var(name);
        if(!v){fprintf(stderr,"Line %d: undefined '%s'\n",cur_line,name);exit(1);}
        if(v->slot>=0) {
            if(is_inc) out("    inc x%d\n",20+v->slot);
            else        out("    dec x%d\n",20+v->slot);
        } else {
            out("    ldr x28, [sp, #%d]\n",v->stack_off);
            if(is_inc) out("    inc x28\n");
            else        out("    dec x28\n");
            out("    str x28, [sp, #%d]\n",v->stack_off);
        }
        expect(TK_SEMICOLON);
        return;
    }

    /* generic expression statement */
    int r=parse_expr();
    free_tmp(r);
    expect(TK_SEMICOLON);
}

static void parse_block(void) {
    expect(TK_LBRACE);
    int saved_nvar=nvar;
    int saved_slot=local_slot;
    while(tok.type!=TK_RBRACE && tok.type!=TK_EOF)
        parse_stmt();
    expect(TK_RBRACE);
    /* pop local vars declared in this block */
    nvar=saved_nvar;
    local_slot=saved_slot;
}

/* ── top-level parser ────────────────────────────────────────────────────── */

static void parse_function(VarType ret_type, const char *fname) {
    /* register function */
    if(nfunc>=MAX_FUNCS){fprintf(stderr,"Too many functions\n");exit(1);}
    Func *fn=&funcs[nfunc++];
    strncpy(fn->name,fname,MAX_IDENT-1);
    fn->ret_type=ret_type;
    fn->nparams=0;
    cur_func_idx=nfunc-1;

    /* save state */
    int saved_scope=cur_scope;
    int saved_slot=local_slot;
    int saved_nvar=nvar;
    cur_scope=1;
    local_slot=0;
    temp_reg=0;

    out("\n%s:\n",fname);

    /* parameter list */
    expect(TK_LPAREN);
    int param_reg=0;
    while(tok.type!=TK_RPAREN && tok.type!=TK_EOF) {
        VarType pt=parse_type();
        char pname[MAX_IDENT]="";
        if(tok.type==TK_IDENT){strncpy(pname,tok.text,MAX_IDENT-1);next();}
        if(strlen(pname)) {
            Var *pv=add_var(pname,pt,1);
            /* params arrive in x0..x7 */
            if(param_reg<8) {
                /* assign to local slot and move from param reg */
                if(pv->slot>=0) {
                    if(20+pv->slot != param_reg)
                        out("    mov x%d, x%d  ; param %s\n",20+pv->slot,param_reg,pname);
                } else {
                    out("    str x%d, [sp, #%d]  ; spill param %s\n",param_reg,pv->stack_off,pname);
                }
                param_reg++;
            }
            strncpy(fn->param_names[fn->nparams],pname,MAX_IDENT-1);
            fn->param_types[fn->nparams]=pt;
            fn->nparams++;
        }
        if(tok.type==TK_COMMA) next();
    }
    expect(TK_RPAREN);

    parse_block();

    cur_scope=saved_scope;
    local_slot=saved_slot;
    nvar=saved_nvar;
    temp_reg=0;
}

static void parse_toplevel(void) {
    next(); /* prime lexer */

    /* entry point: emit a call to main then halt */
    out("    call main\n");
    out("    halt\n\n");

    while(tok.type!=TK_EOF) {
        /* skip bare semicolons */
        if(tok.type==TK_SEMICOLON){next();continue;}

        /* must be a type followed by an identifier */
        if(!is_type_start()){
            fprintf(stderr,"Line %d: expected type or declaration, got '%s' (type %d)\n",
                    cur_line,tok.text,tok.type);
            next(); continue;
        }

        VarType vt=parse_type();

        if(tok.type!=TK_IDENT){
            fprintf(stderr,"Line %d: expected identifier\n",cur_line); next(); continue;
        }
        char name[MAX_IDENT]; strncpy(name,tok.text,MAX_IDENT-1); next();

        /* function or global variable? */
        if(tok.type==TK_LPAREN) {
            parse_function(vt,name);
        } else {
            /* global variable */
            Var *v=add_var(name,vt,0);
            v->slot=0; /* map to x20 — globals share a slot conceptually */
            if(tok.type==TK_ASSIGN) {
                next();
                /* we can only handle constant initialisers here */
                if(tok.type==TK_INT_LIT) {
                    out("; global %s = %lld\n",name,tok.ival); next();
                } else if(tok.type==TK_STR_LIT) {
                    out("; global string %s = \"%s\"\n",name,tok.sval); next();
                }
            }
            expect(TK_SEMICOLON);
        }
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc,char *argv[]) {
    if(argc<3){
        fprintf(stderr,"c2iwa — C/C++ to .iwa transpiler\n");
        fprintf(stderr,"Usage: %s <input.cpp> <output.iwa>\n",argv[0]);
        return 1;
    }

    FILE *fin=fopen(argv[1],"r");
    if(!fin){fprintf(stderr,"Cannot open '%s': %s\n",argv[1],strerror(errno));return 1;}
    fseek(fin,0,SEEK_END); long fsz=ftell(fin); rewind(fin);
    if(fsz>MAX_SRC){fprintf(stderr,"Source file too large\n");return 1;}
    src=(char*)malloc(fsz+1);
    if(!src){fprintf(stderr,"OOM\n");return 1;}
    fread(src,1,fsz,fin); src[fsz]='\0'; fclose(fin);
    src_len=(int)fsz;
    src_pos=0; cur_line=1;

    out_buf=(char*)calloc(1,MAX_OUT);
    if(!out_buf){fprintf(stderr,"OOM\n");return 1;}
    out_pos=0;

    /* init */
    label_counter=0; str_counter=0;
    nvar=0; nfunc=0; local_slot=0; temp_reg=0;
    cur_scope=0; break_top=0; cont_top=0;
    peek_valid=0;

    parse_toplevel();

    FILE *fout=fopen(argv[2],"w");
    if(!fout){fprintf(stderr,"Cannot open '%s' for writing: %s\n",argv[2],strerror(errno));return 1;}
    fprintf(fout,"; 2WA generated structure\n; Source of %s\n; ; ; ; ;\n",argv[1]);
    fwrite(out_buf,1,out_pos,fout);
    fclose(fout);

    fprintf(stdout,"Transpiled → %s\n",argv[2]);
    free(src); free(out_buf);
    return 0;
}

#include "shell_defs.h"
#include "cmd_bossc.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  BOSSC — BOS Script Interpreter  (v2 — parser limpio)
 *  Lenguaje de scripting nativo de TP-BOS (.tpbi)
 *
 *  Arquitectura:
 *    - Un cursor (const char **pp) recorre el source caracter a caracter.
 *    - bs_next_line() extrae la siguiente linea util y avanza el cursor.
 *    - bs_run_line() reconoce TODAS las estructuras de control y
 *      puede llamarse desde cualquier nivel de anidamiento.
 *    - bs_run_block() ejecuta o salta un bloque '[.....]'.
 *    - bs_skip_block() salta un bloque sin ejecutarlo (ramas no tomadas).
 *    - Para repeat/while/infinity se guarda el puntero al inicio del bloque
 *      y se re-ejecuta desde ese punto en cada iteracion.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct { char name[BOSSC_VAR_NAME]; char val[BOSSC_VAR_VAL]; int used; } BVar;
typedef struct {
    char name[BOSSC_VAR_NAME];
    char args[BOSSC_MAX_ARGS][BOSSC_VAR_NAME];
    int  argc;
    char body[BOSSC_FUNC_BODY];
    int  used;
} BFunc;

static BVar  bs_vars [BOSSC_MAX_VARS];
static BFunc bs_funcs[BOSSC_MAX_FUNCS];
static int   bs_steps;
static int   bs_error;

/* ── Utilidades string ──────────────────────────────────────────────────── */

static int bs_strlen(const char *s)  { int n=0; while(s[n]) n++; return n; }

static void bs_strcpy(char *d, const char *s, int max) {
    int i=0; while(s[i]&&i<max-1){d[i]=s[i];i++;} d[i]='\0';
}

static int bs_strcmp(const char *a, const char *b) {
    while(*a&&*a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b;
}

static int bs_strncmp(const char *a, const char *b, int n) {
    for(int i=0;i<n;i++){if(a[i]!=b[i])return(unsigned char)a[i]-(unsigned char)b[i];if(!a[i])return 0;} return 0;
}

static const char *bs_ltrim(const char *s) { while(*s==' '||*s=='\t')s++; return s; }

static void bs_rtrim(char *s) {
    int n=bs_strlen(s); while(n>0&&(s[n-1]==' '||s[n-1]=='\t'))s[--n]='\0';
}

static long bs_atol(const char *s) {
    long v=0; int neg=0; if(*s=='-'){neg=1;s++;}
    while(*s>='0'&&*s<='9') v=v*10+(*s++)-'0';
    return neg?-v:v;
}

static char *bs_ltoa(long v, char *buf, int max) {
    if(max<2){buf[0]='\0';return buf;}
    int neg=(v<0); if(neg)v=-v;
    char tmp[24]; int i=0;
    if(v==0)tmp[i++]='0';
    while(v>0&&i<23){tmp[i++]=(char)('0'+v%10);v/=10;}
    if(neg&&i<23)tmp[i++]='-';
    int j=0; for(int k=i-1;k>=0&&j<max-1;k--)buf[j++]=tmp[k]; buf[j]='\0';
    return buf;
}

/* ── Variables ──────────────────────────────────────────────────────────── */

static BVar *bs_var_find(const char *name) {
    for(int i=0;i<BOSSC_MAX_VARS;i++)
        if(bs_vars[i].used&&bs_strcmp(bs_vars[i].name,name)==0) return &bs_vars[i];
    return 0;
}

static BVar *bs_var_set(const char *name, const char *val) {
    BVar *v=bs_var_find(name);
    if(!v){for(int i=0;i<BOSSC_MAX_VARS;i++){if(!bs_vars[i].used){v=&bs_vars[i];break;}}}
    if(!v){vga_print_color("Error BOSSC: demasiadas variables.\n",VGA_LIGHT_RED,VGA_BLACK);bs_error=1;return 0;}
    v->used=1; bs_strcpy(v->name,name,BOSSC_VAR_NAME); bs_strcpy(v->val,val,BOSSC_VAR_VAL);
    return v;
}

/* Expansion de $VAR en un string */
static void bs_expand(const char *src, char *dst, int max) {
    int di=0;
    for(int si=0;src[si]&&di<max-1;){
        if(src[si]=='$'&&((src[si+1]>='a'&&src[si+1]<='z')||(src[si+1]>='A'&&src[si+1]<='Z')||src[si+1]=='_')){
            si++;
            char vn[BOSSC_VAR_NAME]; int vi=0;
            while(src[si]&&((src[si]>='a'&&src[si]<='z')||(src[si]>='A'&&src[si]<='Z')||(src[si]>='0'&&src[si]<='9')||src[si]=='_')&&vi<BOSSC_VAR_NAME-1)
                vn[vi++]=src[si++];
            vn[vi]='\0';
            BVar *v=bs_var_find(vn);
            const char *rep=v?v->val:"";
            while(*rep&&di<max-1) dst[di++]=*rep++;
        } else { dst[di++]=src[si++]; }
    }
    dst[di]='\0';
}

/* ── Aritmetica (parser recursivo descendente) ──────────────────────────── */

static const char *ap; static int ae;
static void as(void){while(*ap==' '||*ap=='\t')ap++;}
static long arith_expr(void);
static long arith_primary(void){
    as(); if(ae) return 0;
    if(*ap=='('){ap++;long v=arith_expr();as();if(*ap==')')ap++;else ae=1;return v;}
    if(*ap=='-'){ap++;return -arith_primary();}
    if(*ap>='0'&&*ap<='9'){long v=0;while(*ap>='0'&&*ap<='9')v=v*10+(*ap++)-'0';return v;}
    if((*ap>='a'&&*ap<='z')||(*ap>='A'&&*ap<='Z')||*ap=='_'){
        char vn[BOSSC_VAR_NAME];int vi=0;
        while(vi<BOSSC_VAR_NAME-1&&((*ap>='a'&&*ap<='z')||(*ap>='A'&&*ap<='Z')||(*ap>='0'&&*ap<='9')||*ap=='_'))vn[vi++]=*ap++;
        vn[vi]='\0'; BVar *v=bs_var_find(vn); return v?bs_atol(v->val):0;
    }
    ae=1; return 0;
}
static long arith_mul(void){
    long v=arith_primary();
    while(!ae){as();
        if(*ap=='*'){ap++;v*=arith_primary();}
        else if(*ap=='/'){ap++;long d=arith_primary();if(d==0){ae=1;return 0;}v/=d;}
        else if(*ap=='%'){ap++;long d=arith_primary();if(d==0){ae=1;return 0;}v%=d;}
        else break;}
    return v;
}
static long arith_expr(void){
    long v=arith_mul();
    while(!ae){as();
        if(*ap=='+'){ap++;v+=arith_mul();}
        else if(*ap=='-'){ap++;v-=arith_mul();}
        else break;}
    return v;
}
static void bs_eval_arith(const char *expr, char *buf, int max){
    char exp[BOSSC_VAR_VAL*2]; bs_expand(expr,exp,sizeof(exp));
    ap=exp; ae=0; long r=arith_expr(); as();
    if(!ae&&*ap=='\0') bs_ltoa(r,buf,max);
    else bs_strcpy(buf,exp,max);
}

/* ── Evaluacion de condiciones ──────────────────────────────────────────── */

static int bs_eval_cond(const char *expr){
    char buf[256]; bs_expand(expr,buf,sizeof(buf));
    const char *e=bs_ltrim(buf);
    char lhs[64],op[4],rhs[64]; int li=0,oi=0,ri=0;
    while(*e&&*e!=' '&&li<63)lhs[li++]=*e++; lhs[li]='\0'; e=bs_ltrim(e);
    while(*e&&*e!=' '&&oi<3&&(*e=='='||*e=='!'||*e=='<'||*e=='>'))op[oi++]=*e++; op[oi]='\0'; e=bs_ltrim(e);
    while(*e&&ri<63)rhs[ri++]=*e++; rhs[ri]='\0';
    if(oi==0) return bs_strlen(lhs)>0&&bs_strcmp(lhs,"0")!=0;
    /* Comparacion numerica si ambos lados son numeros */
    int lnum=1,rnum=1;
    const char *lp=lhs; if(*lp=='-')lp++;
    const char *rp=rhs; if(*rp=='-')rp++;
    if(*lp=='\0')lnum=0; if(*rp=='\0')rnum=0;
    while(*lp){if(*lp<'0'||*lp>'9'){lnum=0;break;}lp++;}
    while(*rp){if(*rp<'0'||*rp>'9'){rnum=0;break;}rp++;}
    if(lnum&&rnum){
        long lv=bs_atol(lhs),rv=bs_atol(rhs);
        if(bs_strcmp(op,"==")==0)return lv==rv; if(bs_strcmp(op,"!=")==0)return lv!=rv;
        if(bs_strcmp(op,">")==0) return lv>rv;  if(bs_strcmp(op,"<")==0) return lv<rv;
        if(bs_strcmp(op,">=")==0)return lv>=rv; if(bs_strcmp(op,"<=")==0)return lv<=rv;
    } else {
        int cmp=bs_strcmp(lhs,rhs);
        if(bs_strcmp(op,"==")==0)return cmp==0; if(bs_strcmp(op,"!=")==0)return cmp!=0;
        if(bs_strcmp(op,">")==0) return cmp>0;  if(bs_strcmp(op,"<")==0) return cmp<0;
        if(bs_strcmp(op,">=")==0)return cmp>=0; if(bs_strcmp(op,"<=")==0)return cmp<=0;
    }
    return 0;
}

/* ── Funciones BOSSC ────────────────────────────────────────────────────── */

static BFunc *bs_func_find(const char *name){
    for(int i=0;i<BOSSC_MAX_FUNCS;i++)
        if(bs_funcs[i].used&&bs_strcmp(bs_funcs[i].name,name)==0) return &bs_funcs[i];
    return 0;
}

static void bs_parse_declaratef(const char *line){
    line=bs_ltrim(line+10);
    char fname[BOSSC_VAR_NAME]; int fi=0;
    while(*line&&*line!='('&&fi<BOSSC_VAR_NAME-1) fname[fi++]=*line++;
    while(fi>0&&fname[fi-1]==' ')fi--; fname[fi]='\0';
    if(*line!='('){vga_print_color("Error BOSSC: declaratef sin '('.\n",VGA_LIGHT_RED,VGA_BLACK);bs_error=1;return;}
    line++;
    char args[BOSSC_MAX_ARGS][BOSSC_VAR_NAME]; int argc=0;
    while(*line&&*line!=')'){
        while(*line==' ')line++;
        if(*line==')')break;
        char an[BOSSC_VAR_NAME]; int ai=0;
        while(*line&&*line!=','&&*line!=')'&&ai<BOSSC_VAR_NAME-1) an[ai++]=*line++;
        while(ai>0&&an[ai-1]==' ')ai--; an[ai]='\0';
        if(ai>0&&argc<BOSSC_MAX_ARGS) bs_strcpy(args[argc++],an,BOSSC_VAR_NAME);
        if(*line==',')line++;
    }
    if(*line==')')line++;
    line=bs_ltrim(line); if(*line=='=')line++; line=bs_ltrim(line);
    BFunc *f=bs_func_find(fname);
    if(!f){for(int i=0;i<BOSSC_MAX_FUNCS;i++){if(!bs_funcs[i].used){f=&bs_funcs[i];break;}}}
    if(!f){vga_print_color("Error BOSSC: demasiadas funciones.\n",VGA_LIGHT_RED,VGA_BLACK);bs_error=1;return;}
    f->used=1; bs_strcpy(f->name,fname,BOSSC_VAR_NAME); f->argc=argc;
    for(int i=0;i<argc;i++) bs_strcpy(f->args[i],args[i],BOSSC_VAR_NAME);
    bs_strcpy(f->body,line,BOSSC_FUNC_BODY);
}

static int bs_call_func(const char *line){
    char fname[BOSSC_VAR_NAME]; int fi=0;
    const char *p=line;
    while(*p&&*p!=' '&&fi<BOSSC_VAR_NAME-1)fname[fi++]=*p++; fname[fi]='\0';
    p=bs_ltrim(p);
    BFunc *f=bs_func_find(fname); if(!f) return -1;
    char ca[BOSSC_MAX_ARGS][BOSSC_VAR_VAL]; int cn=0;
    while(*p&&cn<BOSSC_MAX_ARGS){
        while(*p==' ')p++;
        if(!*p)break;
        char ab[BOSSC_VAR_VAL]; int ai=0;
        while(*p&&*p!=' '&&ai<BOSSC_VAR_VAL-1)ab[ai++]=*p++; ab[ai]='\0';
        char ex[BOSSC_VAR_VAL]; bs_expand(ab,ex,sizeof(ex));
        bs_strcpy(ca[cn++],ex,BOSSC_VAR_VAL);
    }
    char pv[BOSSC_MAX_ARGS][BOSSC_VAR_VAL]; int pu[BOSSC_MAX_ARGS];
    for(int i=0;i<f->argc;i++){
        BVar *v=bs_var_find(f->args[i]);
        if(v){bs_strcpy(pv[i],v->val,BOSSC_VAR_VAL);pu[i]=1;}else pu[i]=0;
        bs_var_set(f->args[i],i<cn?ca[i]:"0");
    }
    char result[BOSSC_VAR_VAL]; bs_eval_arith(f->body,result,sizeof(result));
    bs_var_set("resultado",result);
    vga_print_color("= ",VGA_LIGHT_CYAN,VGA_BLACK); vga_println(result);
    for(int i=0;i<f->argc;i++){
        if(pu[i]) bs_var_set(f->args[i],pv[i]);
        else { BVar *tmp=bs_var_find(f->args[i]); if(tmp) tmp->used=0; }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Cursor y parser
 * ═══════════════════════════════════════════════════════════════════════════ */

/*  bs_next_line: extrae la siguiente linea util (no vacia, no comentario).
    Avanza *pp. Retorna 1 si hay linea, 0 si fin de fuente. */
static int bs_next_line(const char **pp, char *buf, int max){
    while(**pp){
        while(**pp==' '||**pp=='\t'||**pp=='\n'||**pp=='\r')(*pp)++;
        if(!**pp) return 0;
        int li=0;
        while(**pp&&**pp!='\n'&&li<max-1) buf[li++]=*(*pp)++;
        buf[li]='\0'; if(**pp=='\n')(*pp)++;
        const char *t=bs_ltrim(buf);
        if(t[0]=='$'&&t[1]=='$') continue;   /* comentario */
        if(t[0]=='\0') continue;              /* linea vacia */
        bs_strcpy(buf,t,max);
        return 1;
    }
    return 0;
}

/* Forward declarations */
static void bs_run_line(const char **pp, char *line);
static void bs_skip_block(const char **pp);
static void bs_run_block(const char **pp, int active);

/*  bs_skip_block: salta un bloque [...] en el stream.
    *pp debe apuntar INMEDIATAMENTE DESPUES del '[' de apertura.
    Al retornar, *pp esta DESPUES del ']' de cierre. */
static void bs_skip_block(const char **pp){
    int depth=1;
    while(**pp&&depth>0){
        if(**pp=='[') depth++;
        else if(**pp==']'){ depth--; if(depth==0){(*pp)++;return;} }
        (*pp)++;
    }
}

/*  bs_consume_open_bracket: busca y consume el '[' de apertura.
    Primero lo busca en 'hint' (lo que quedo de la linea ya leida);
    si no lo encuentra ahi, lo busca avanzando en *pp.
    Retorna 1 si OK, 0 si no se encontro. */
static int bs_consume_open_bracket(const char **pp, const char *hint){
    /* Buscar en hint (parte de la linea ya leida) */
    while(*hint&&*hint!='[') hint++;
    if(*hint=='[') return 1;  /* el '[' ya fue pasado; *pp ya listo */
    /* No estaba en hint: buscarlo en el stream */
    while(**pp){
        if(**pp=='['){(*pp)++;return 1;}
        if(**pp==' '||**pp=='\t'||**pp=='\n'||**pp=='\r'){(*pp)++;continue;}
        break; /* caracter inesperado */
    }
    return 0;
}

/*  bs_run_block: ejecuta (active=1) o salta (active=0) un bloque.
    *pp debe apuntar INMEDIATAMENTE DESPUES del '[' de apertura. */
static void bs_run_block(const char **pp, int active){
    if(!active){bs_skip_block(pp);return;}
    while(**pp&&!bs_error){
        while(**pp==' '||**pp=='\t'||**pp=='\n'||**pp=='\r')(*pp)++;
        if(!**pp) break;
        if(**pp==']'){(*pp)++;return;}
        char lb[256]; int li=0;
        while(**pp&&**pp!='\n'&&li<255) lb[li++]=*(*pp)++;
        lb[li]='\0'; if(**pp=='\n')(*pp)++;
        char *line=(char *)bs_ltrim(lb);
        if(!*line||(line[0]=='$'&&line[1]=='$')) continue;
        bs_steps++;
        if(bs_steps>BOSSC_MAX_STEPS){
            vga_print_color("Error BOSSC: limite de pasos.\n",VGA_LIGHT_RED,VGA_BLACK);
            bs_error=1;return;
        }
        bs_run_line(pp,line);
    }
}

/*  bs_run_line: ejecuta una linea de BOSSC.
    'line' ya esta limpia (sin whitespace inicial, sin '\n').
    *pp apunta a lo que sigue en el stream (necesario para bloques). */
static void bs_run_line(const char **pp, char *line){
    if(bs_error) return;

    /* echo */
    if(bs_strncmp(line,"echo",4)==0&&(line[4]==' '||line[4]=='\0')){
        char ex[256]; bs_expand(bs_ltrim(line+4),ex,sizeof(ex)); vga_println(ex); return;
    }

    /* declaratev VAR = EXPR */
    if(bs_strncmp(line,"declaratev",10)==0&&line[10]==' '){
        const char *r=bs_ltrim(line+11);
        char vn[BOSSC_VAR_NAME]; int vi=0;
        while(*r&&*r!=' '&&*r!='='&&vi<BOSSC_VAR_NAME-1) vn[vi++]=*r++;
        vn[vi]='\0'; r=bs_ltrim(r); if(*r=='=')r++; r=bs_ltrim(r);
        char val[BOSSC_VAR_VAL]; bs_eval_arith(r,val,sizeof(val));
        bs_var_set(vn,val); return;
    }

    /* declaratef */
    if(bs_strncmp(line,"declaratef",10)==0&&line[10]==' '){
        bs_parse_declaratef(line); return;
    }

    /* ══ IF / ELSIF / ELSE ══════════════════════════════════════════════════
     *
     *  Formato en el source:
     *    if COND [
     *        ...
     *    ]; elsif COND [         <- el ']' cierra el bloque anterior,
     *        ...                    ';' es separador opcional,
     *    ]; else [                  luego viene la siguiente rama
     *        ...
     *    ]
     *
     *  Estrategia: ejecutamos/saltamos el bloque de cada rama.
     *  Despues de ejecutar el bloque, hacemos peek de la siguiente linea.
     *  Si empieza con ']' seguido de 'elsif'/'else', es continuacion.
     *  Si no, la cadena if termino.
     * ═══════════════════════════════════════════════════════════════════════ */
    if(bs_strncmp(line,"if ",3)==0){
        int branch_taken=0;
        /* 'line' contiene la linea actual de la rama (if/elsif/else).
           Usamos un buffer propio para guardarla de forma estable. */
        char rama[256]; bs_strcpy(rama,line,sizeof(rama));

        while(1){
            char *cl=rama;
            int is_if    = bs_strncmp(cl,"if ",3)==0;
            int is_elsif = bs_strncmp(cl,"elsif ",6)==0;
            int is_else  = bs_strncmp(cl,"else",4)==0&&(cl[4]==' '||cl[4]=='['||cl[4]=='\0');
            if(!is_if&&!is_elsif&&!is_else) break;

            /* Extraer condicion */
            const char *craw = is_if ? cl+3 : is_elsif ? cl+6 : "";
            char cond[128]; int ci=0;
            const char *cp=bs_ltrim(craw);
            while(*cp&&*cp!='['&&ci<127) cond[ci++]=*cp++;
            cond[ci]='\0'; bs_rtrim(cond);

            int take = is_else ? (!branch_taken)
                               : (!branch_taken && bs_eval_cond(cond));

            /* Consumir '[': puede estar en 'cl' (en cp) o en el stream */
            if(!bs_consume_open_bracket(pp, cp)){
                vga_print_color("Error BOSSC: falta '[' en if/elsif/else.\n",VGA_LIGHT_RED,VGA_BLACK);
                bs_error=1; return;
            }

            bs_run_block(pp, take);
            if(take) branch_taken=1;

            /* ── Lookahead: ver si la siguiente linea es continuacion ──
               Una linea de continuacion tiene la forma:
                 ]; elsif COND [
                 ]; else [
                 ]        <- fin de if sin continuacion
               El ']' ya fue consumido por bs_run_block/bs_skip_block.
               Necesitamos ver si lo que sigue en el stream es elsif/else.

               Hacemos peek (guardamos cursor, leemos, restauramos si no
               es continuacion). */
            const char *saved = *pp;
            char nb[256];
            if(!bs_next_line(pp,nb,sizeof(nb))){
                /* Fin de source: terminamos */
                break;
            }

            /* Quitar ']' y ';' opcionales al inicio */
            char *nb2=nb;
            if(*nb2==']'){nb2++;while(*nb2==';'||*nb2==' ')nb2++;}
            nb2=(char *)bs_ltrim(nb2);

            if(bs_strncmp(nb2,"elsif ",6)==0||
               (bs_strncmp(nb2,"else",4)==0&&(nb2[4]==' '||nb2[4]=='['||nb2[4]=='\0'))){
                /* Es continuacion: preparar rama siguiente */
                bs_strcpy(rama,nb2,sizeof(rama));
            } else {
                /* No es continuacion: devolver la linea al stream */
                *pp=saved;
                break;
            }
        }
        return;
    }

    /* ══ CASECON ════════════════════════════════════════════════════════════
     *
     *  casecon [
     *      case COND [
     *          accion
     *      ]
     *      else [
     *          accion
     *      ]
     *  ]
     * ═══════════════════════════════════════════════════════════════════════ */
    if(bs_strncmp(line,"casecon",7)==0&&(line[7]==' '||line[7]=='['||line[7]=='\0')){
        if(!bs_consume_open_bracket(pp,line+7)){
            vga_print_color("Error BOSSC: falta '[' en casecon.\n",VGA_LIGHT_RED,VGA_BLACK);
            bs_error=1; return;
        }
        int case_taken=0;
        while(!bs_error){
            char cb[256];
            if(!bs_next_line(pp,cb,sizeof(cb))) break;
            if(cb[0]==']') break;
            int is_case=bs_strncmp(cb,"case ",5)==0;
            int is_else=bs_strncmp(cb,"else",4)==0&&(cb[4]==' '||cb[4]=='['||cb[4]=='\0');
            if(!is_case&&!is_else) continue;
            const char *cp2=is_case?bs_ltrim(cb+5):"";
            char cond2[128]; int ci2=0;
            while(*cp2&&*cp2!='['&&ci2<127) cond2[ci2++]=*cp2++;
            cond2[ci2]='\0'; bs_rtrim(cond2);
            int take2=is_else?(!case_taken):(!case_taken&&bs_eval_cond(cond2));
            if(!bs_consume_open_bracket(pp,cp2)){
                vga_print_color("Error BOSSC: falta '[' en case/else.\n",VGA_LIGHT_RED,VGA_BLACK);
                bs_error=1; return;
            }
            bs_run_block(pp,take2);
            if(take2) case_taken=1;
        }
        return;
    }

    /* ══ REPEAT N CMD / REPEAT N [ ══════════════════════════════════════════ */
    if(bs_strncmp(line,"repeat ",7)==0){
        const char *rest=bs_ltrim(line+7);
        char nbuf[BOSSC_VAR_VAL]; int ni=0;
        while(*rest&&*rest!=' '&&ni<BOSSC_VAR_VAL-1) nbuf[ni++]=*rest++;
        nbuf[ni]='\0'; rest=bs_ltrim(rest);
        char nval[BOSSC_VAR_VAL]; bs_expand(nbuf,nval,sizeof(nval));
        long n=bs_atol(nval);
        if(n<0||n>10000){
            vga_print_color("Error BOSSC: repeat N invalido (0-10000).\n",VGA_LIGHT_RED,VGA_BLACK);
            if(*rest=='[') bs_skip_block(pp);
            return;
        }
        if(*rest=='['){
            const char *bstart=*pp;
            for(long r=0;r<n&&!bs_error;r++){
                *pp=bstart; bs_run_block(pp,1);
                if(++bs_steps>BOSSC_MAX_STEPS){
                    vga_print_color("Error BOSSC: limite de pasos (repeat).\n",VGA_LIGHT_RED,VGA_BLACK);
                    bs_error=1;
                }
            }
            if(n==0){*pp=bstart;bs_skip_block(pp);}
        } else if(*rest){
            char cmd[256]; bs_strcpy(cmd,rest,sizeof(cmd));
            for(long r=0;r<n&&!bs_error;r++){
                bs_run_line(pp,cmd);
                if(++bs_steps>BOSSC_MAX_STEPS){
                    vga_print_color("Error BOSSC: limite de pasos (repeat).\n",VGA_LIGHT_RED,VGA_BLACK);
                    bs_error=1;
                }
            }
        } else { vga_println("Uso: repeat N cmd  o  repeat N ["); }
        return;
    }

    /* ══ INFINITY CMD / INFINITY [ ══════════════════════════════════════════ */
    if(bs_strncmp(line,"infinity",8)==0&&(line[8]==' '||line[8]=='['||line[8]=='\0')){
        const char *rest=bs_ltrim(line+8);
        if(*rest=='['){
            const char *bstart=*pp;
            while(!bs_error){
                *pp=bstart; bs_run_block(pp,1);
                if(++bs_steps>BOSSC_MAX_STEPS){
                    vga_print_color("Error BOSSC: limite de pasos (infinity).\n",VGA_LIGHT_RED,VGA_BLACK);
                    bs_error=1;
                }
            }
        } else if(*rest){
            char cmd[256]; bs_strcpy(cmd,rest,sizeof(cmd));
            while(!bs_error){
                bs_run_line(pp,cmd);
                if(++bs_steps>BOSSC_MAX_STEPS){
                    vga_print_color("Error BOSSC: limite de pasos (infinity).\n",VGA_LIGHT_RED,VGA_BLACK);
                    bs_error=1;
                }
            }
        } else { vga_println("Uso: infinity cmd  o  infinity ["); }
        return;
    }

    /* ══ WHILE COND CMD / WHILE COND [ ════════════════════════════════════ */
    if(bs_strncmp(line,"while ",6)==0){
        const char *rest=bs_ltrim(line+6);
        char cond[128]; int ci=0;
        const char *cp=rest;
        while(*cp&&*cp!='['&&ci<127) cond[ci++]=*cp++;
        cond[ci]='\0'; bs_rtrim(cond);
        if(*cp=='['){
            const char *bstart=*pp;
            while(!bs_error&&bs_eval_cond(cond)){
                *pp=bstart; bs_run_block(pp,1);
                if(++bs_steps>BOSSC_MAX_STEPS){
                    vga_print_color("Error BOSSC: limite de pasos (while).\n",VGA_LIGHT_RED,VGA_BLACK);
                    bs_error=1;
                }
            }
            /* Saltar el bloque (el cursor quedo al final de la ultima iteracion,
               o al inicio si nunca se ejecuto) */
            *pp=bstart; bs_skip_block(pp);
        } else if(*cp){
            char cmd[256]; bs_strcpy(cmd,(char*)bs_ltrim(cp),sizeof(cmd));
            while(!bs_error&&bs_eval_cond(cond)){
                bs_run_line(pp,cmd);
                if(++bs_steps>BOSSC_MAX_STEPS){
                    vga_print_color("Error BOSSC: limite de pasos (while).\n",VGA_LIGHT_RED,VGA_BLACK);
                    bs_error=1;
                }
            }
        } else { vga_println("Uso: while COND cmd  o  while COND ["); }
        return;
    }

    /* ── Funcion BOSSC o comando nativo del shell ─────────────────────── */
    char ex[256]; bs_expand(line,ex,sizeof(ex));
    if(bs_call_func(ex)==0) return;
    int j=0; while(ex[j]&&j<INPUT_MAX-1){input[j]=ex[j];j++;} input[j]='\0'; input_len=j;
    dispatch();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  bossc_exec: punto de entrada del interprete
 * ═══════════════════════════════════════════════════════════════════════════ */
void bossc_exec(const char *source){
    if(!source||!*source) return;
    bs_steps=0; bs_error=0;
    const char *p=source;
    while(!bs_error){
        char lb[256];
        if(!bs_next_line(&p,lb,sizeof(lb))) break;
        if(++bs_steps>BOSSC_MAX_STEPS){
            vga_print_color("Error BOSSC: limite de pasos alcanzado.\n",VGA_LIGHT_RED,VGA_BLACK);
            bs_error=1; break;
        }
        bs_run_line(&p,lb);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Comando: run archivo.tpbi
 * ═══════════════════════════════════════════════════════════════════════════ */
void cmd_run(const char *line){
    const char *name=arg1(line);
    if(k_strlen(name)==0){
        vga_println("Uso: run archivo.tpbi");
        vga_println("  Ejecuta un script BOSSC desde el sistema de archivos.");
        return;
    }
    int idx=resolve_path(name);
    if(idx<0){vga_print_color("No encontrado: ",VGA_LIGHT_RED,VGA_BLACK);vga_println(name);return;}
    FSNode *n=fs_get(idx);
    if(n->type!=NODE_FILE){vga_println("run solo acepta archivos.");return;}
    const char *source=fs_read(idx);
    if(!source||source[0]=='\0'){vga_print_color("(archivo vacio)\n",VGA_DARK_GRAY,VGA_BLACK);return;}
    for(int i=0;i<BOSSC_MAX_VARS;i++) bs_vars[i].used=0;
    for(int i=0;i<BOSSC_MAX_FUNCS;i++) bs_funcs[i].used=0;
    vga_print_color("[BOSSC] ",VGA_DARK_GRAY,VGA_BLACK);
    vga_print_color(name,VGA_YELLOW,VGA_BLACK); vga_putchar('\n');
    bossc_exec(source);
    if(!bs_error) vga_print_color("[BOSSC] Fin.\n",VGA_DARK_GRAY,VGA_BLACK);
}

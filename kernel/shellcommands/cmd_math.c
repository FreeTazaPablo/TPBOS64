#include "shell_defs.h"

/* ── calc — calculadora con +, -, *, /, ||, &&, ! ────────────────────────────
   Gramática (precedencia de menor a mayor):
     expr    → or_expr
     or_expr → and_expr  ( '||' and_expr )*
     and_expr→ not_expr  ( '&&' not_expr )*
     not_expr→ '!' not_expr  |  add_expr
     add_expr→ mul_expr  ( ('+' | '-') mul_expr )*
     mul_expr→ unary    ( ('*' | '/') unary )*
     unary   → '-' unary  |  primary
     primary → NUMBER | '(' expr ')'
   Resultado booleano: cualquier valor != 0 es verdadero; || y && devuelven 1 o 0.
   ──────────────────────────────────────────────────────────────────────────── */

/* Estado del parser — puntero avanzable sobre la expresión */
const char *calc_p;   /* puntero de lectura */
int calc_err;         /* 1 si hubo error */

void calc_skip_ws(void) {
    while (*calc_p == ' ' || *calc_p == '\t') calc_p++;
}

/* Forward declarations del parser */
static long calc_expr(void);
static long calc_or(void);
static long calc_and(void);
static long calc_not(void);
static long calc_add(void);
static long calc_mul(void);
static long calc_unary(void);
static long calc_primary(void);

static long calc_primary(void) {
    calc_skip_ws();
    if (calc_err) return 0;

    if (*calc_p == '(') {
        calc_p++;
        long v = calc_expr();
        calc_skip_ws();
        if (*calc_p == ')') calc_p++;
        else calc_err = 1;
        return v;
    }

    /* Número (solo dígitos, sin punto — enteros) */
    if (*calc_p >= '0' && *calc_p <= '9') {
        long v = 0;
        while (*calc_p >= '0' && *calc_p <= '9') v = v * 10 + (*calc_p++ - '0');
        return v;
    }

    calc_err = 1;
    return 0;
}

static long calc_unary(void) {
    calc_skip_ws();
    if (calc_err) return 0;
    if (*calc_p == '-') { calc_p++; return -calc_unary(); }
    return calc_primary();
}

static long calc_mul(void) {
    long v = calc_unary();
    while (!calc_err) {
        calc_skip_ws();
        if (*calc_p == '*') {
            calc_p++; v = v * calc_unary();
        } else if (*calc_p == '/') {
            calc_p++;
            long d = calc_unary();
            if (d == 0) { calc_err = 1; return 0; }   /* división por cero */
            v = v / d;
        } else break;
    }
    return v;
}

static long calc_add(void) {
    long v = calc_mul();
    while (!calc_err) {
        calc_skip_ws();
        if (*calc_p == '+') { calc_p++; v = v + calc_mul(); }
        else if (*calc_p == '-') { calc_p++; v = v - calc_mul(); }
        else break;
    }
    return v;
}

static long calc_not(void) {
    calc_skip_ws();
    if (!calc_err && *calc_p == '!') {
        /* Asegurarse de que no sea '!=' (no soportado, pero evitar confusión) */
        calc_p++;
        return calc_not() == 0 ? 1 : 0;
    }
    return calc_add();
}

static long calc_and(void) {
    long v = calc_not();
    while (!calc_err) {
        calc_skip_ws();
        if (calc_p[0] == '&' && calc_p[1] == '&') {
            calc_p += 2;
            long r = calc_not();
            v = (v != 0 && r != 0) ? 1 : 0;
        } else break;
    }
    return v;
}

static long calc_or(void) {
    long v = calc_and();
    while (!calc_err) {
        calc_skip_ws();
        if (calc_p[0] == '|' && calc_p[1] == '|') {
            calc_p += 2;
            long r = calc_and();
            v = (v != 0 || r != 0) ? 1 : 0;
        } else break;
    }
    return v;
}

static long calc_expr(void) {
    return calc_or();
}


void cmd_calc(const char *line) {
    const char *expr = arg1(line);
    if (k_strlen(expr) == 0) {
        vga_println("Uso: calc expresion");
        vga_println("  Ops: + - * / ! && ||  |  Ejemplo: calc 3+4*2");
        return;
    }

    calc_p   = expr;
    calc_err = 0;

    long result = calc_expr();
    calc_skip_ws();

    if (calc_err) {
        vga_print_color("Error: expresion invalida cerca de '", VGA_LIGHT_RED, VGA_BLACK);
        vga_print_color(calc_p, VGA_LIGHT_RED, VGA_BLACK);
        vga_println("'");
        return;
    }
    if (*calc_p != '\0') {
        vga_print_color("Error: caracter inesperado '", VGA_LIGHT_RED, VGA_BLACK);
        vga_print_color(calc_p, VGA_LIGHT_RED, VGA_BLACK);
        vga_println("'");
        return;
    }

    vga_print_color("= ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_long(result);
    vga_putchar('\n');
}

/* ── hex — convierte decimal a hexadecimal ───────────────────────────────────
   Uso: hex número
   ─────────────────────────────────────────────────────────────────────────── */

void cmd_hex(const char *line) {
    const char *nstr = arg1(line);
    if (k_strlen(nstr) == 0) {
        vga_println("Uso: hex numero");
        vga_println("  Ejemplo: hex 255  ->  0xFF");
        return;
    }

    /* Parsear número (acepta negativos) */
    int neg = 0;
    if (*nstr == '-') { neg = 1; nstr++; }
    long v = 0;
    int digits = 0;
    while (*nstr >= '0' && *nstr <= '9') { v = v * 10 + (*nstr++ - '0'); digits++; }
    if (digits == 0) { vga_print_color("Numero invalido.\n", VGA_LIGHT_RED, VGA_BLACK); return; }
    if (neg) v = -v;

    vga_print_color("Dec: ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_long(v);
    vga_print("  ->  ");
    vga_print_color("Hex: ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_hex(v);
    vga_putchar('\n');
}


/* ── bin — convierte decimal a binario ───────────────────────────────────────
   Uso: bin número
   ─────────────────────────────────────────────────────────────────────────── */

void cmd_bin(const char *line) {
    const char *nstr = arg1(line);
    if (k_strlen(nstr) == 0) {
        vga_println("Uso: bin numero");
        vga_println("  Ejemplo: bin 42  ->  0b10_1010");
        return;
    }

    int neg = 0;
    if (*nstr == '-') { neg = 1; nstr++; }
    long v = 0;
    int digits = 0;
    while (*nstr >= '0' && *nstr <= '9') { v = v * 10 + (*nstr++ - '0'); digits++; }
    if (digits == 0) { vga_print_color("Numero invalido.\n", VGA_LIGHT_RED, VGA_BLACK); return; }
    if (neg) v = -v;

    vga_print_color("Dec: ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_long(v);
    vga_print("  ->  ");
    vga_print_color("Bin: ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_bin(v);
    vga_putchar('\n');
}



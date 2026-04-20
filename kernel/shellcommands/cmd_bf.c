#include "shell_defs.h"
#include "cmd_bf.h"

/* ═══════════════════════════════════════════════════════════════════════════
   Intérprete Brainfuck para TP-BOS
   ─────────────────────────────────────────────────────────────────────────
   Modelo de ejecución:
     · Cinta de BF_TAPE_SIZE celdas unsigned char, inicializada a 0.
     · Puntero de datos empieza en la celda 0.
     · El programa es un string C terminado en '\0'.
     · Caracteres que no son operadores BF se ignoran (comentarios gratis).
     · Límite de BF_MAX_STEPS pasos para cortar bucles infinitos.
     · [ ] se resuelven con búsqueda lineal — sin pre-compilación.
   ═══════════════════════════════════════════════════════════════════════════ */

/* ── Cinta ────────────────────────────────────────────────────────────────── */
static unsigned char bf_tape[BF_TAPE_SIZE];

/* ── Buffer para modo interactivo ────────────────────────────────────────── */
static char bf_prog_buf[FS_MAX_CONTENT];

/* ── Buscar el ] que cierra el [ en posición 'from' ──────────────────────── */
static int bf_find_close(const char *prog, int from, int prog_len) {
    int depth = 1;
    int i = from + 1;
    while (i < prog_len && depth > 0) {
        if (prog[i] == '[') depth++;
        else if (prog[i] == ']') depth--;
        i++;
    }
    return (depth == 0) ? i - 1 : -1;
}

/* ── Buscar el [ que abre el ] en posición 'from' ────────────────────────── */
static int bf_find_open(const char *prog, int from) {
    int depth = 1;
    int i = from - 1;
    while (i >= 0 && depth > 0) {
        if (prog[i] == ']') depth++;
        else if (prog[i] == '[') depth--;
        i--;
    }
    return (depth == 0) ? i + 1 : -1;
}

/* ── Motor del intérprete ─────────────────────────────────────────────────── */
typedef enum {
    BF_OK = 0,
    BF_ERR_TAPE_UNDERFLOW,
    BF_ERR_TAPE_OVERFLOW,
    BF_ERR_UNMATCHED_OPEN,
    BF_ERR_UNMATCHED_CLOSE,
    BF_ERR_STEPS_EXCEEDED,
} BFResult;

static BFResult bf_run(const char *prog, int prog_len) {
    k_memset(bf_tape, 0, BF_TAPE_SIZE);

    int dp    = 0;
    int ip    = 0;
    int steps = 0;

    while (ip < prog_len) {
        char op = prog[ip];

        switch (op) {
        case '+': bf_tape[dp]++; break;
        case '-': bf_tape[dp]--; break;

        case '>':
            dp++;
            if (dp >= BF_TAPE_SIZE) return BF_ERR_TAPE_OVERFLOW;
            break;

        case '<':
            dp--;
            if (dp < 0) return BF_ERR_TAPE_UNDERFLOW;
            break;

        case '.':
            vga_putchar((char)bf_tape[dp]);
            break;

        case ',':
            bf_tape[dp] = (unsigned char)kb_getchar();
            break;

        case '[':
            if (bf_tape[dp] == 0) {
                int close = bf_find_close(prog, ip, prog_len);
                if (close < 0) return BF_ERR_UNMATCHED_OPEN;
                ip = close;
            }
            break;

        case ']':
            if (bf_tape[dp] != 0) {
                int open = bf_find_open(prog, ip);
                if (open < 0) return BF_ERR_UNMATCHED_CLOSE;
                ip = open;
            }
            break;

        default:
            break;
        }

        ip++;
        steps++;
        if (steps >= BF_MAX_STEPS) return BF_ERR_STEPS_EXCEEDED;
    }

    return BF_OK;
}

/* ── Reportar resultado ───────────────────────────────────────────────────── */
static void bf_report(BFResult r) {
    vga_putchar('\n');
    switch (r) {
    case BF_OK:
        break;
    case BF_ERR_TAPE_UNDERFLOW:
        vga_print_color("[BF] Error: puntero salio por la izquierda.\n",
                        VGA_LIGHT_RED, VGA_BLACK);
        break;
    case BF_ERR_TAPE_OVERFLOW:
        vga_print_color("[BF] Error: puntero salio por la derecha.\n",
                        VGA_LIGHT_RED, VGA_BLACK);
        break;
    case BF_ERR_UNMATCHED_OPEN:
        vga_print_color("[BF] Error: '[' sin ']' correspondiente.\n",
                        VGA_LIGHT_RED, VGA_BLACK);
        break;
    case BF_ERR_UNMATCHED_CLOSE:
        vga_print_color("[BF] Error: ']' sin '[' correspondiente.\n",
                        VGA_LIGHT_RED, VGA_BLACK);
        break;
    case BF_ERR_STEPS_EXCEEDED:
        vga_print_color("[BF] Detenido: limite de pasos alcanzado (bucle infinito?).\n",
                        VGA_LIGHT_CYAN, VGA_BLACK);
        break;
    }
}

/* ── bf <codigo inline>  |  bf (sin args → modo interactivo) ─────────────── */
void cmd_bf(const char *line) {
    const char *prog = arg1(line);

    /* Modo inline: bf +++.--- etc. */
    if (k_strlen(prog) > 0) {
        BFResult r = bf_run(prog, k_strlen(prog));
        bf_report(r);
        return;
    }

    /* Modo interactivo: leer el programa directo con kb_getchar,
       sin pasar por el read_line del shell (historial, autocompletado...) */
    vga_print_color("[BF] Escribe el programa y pulsa Enter:\n",
                    VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_color("> ", VGA_YELLOW, VGA_BLACK);

    int len = 0;
    while (len < FS_MAX_CONTENT - 1) {
        unsigned char c = kb_getchar();
        if (c == '\n') break;
        if (c == '\b') {
            if (len > 0) {
                len--;
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }
            continue;
        }
        if (c >= 32 && c < 128) {
            bf_prog_buf[len++] = (char)c;
            vga_putchar((char)c);
        }
    }
    bf_prog_buf[len] = '\0';
    vga_putchar('\n');

    if (len == 0) {
        vga_println("[BF] Programa vacio.");
        return;
    }

    BFResult r = bf_run(bf_prog_buf, len);
    bf_report(r);
}

/* ── bfrun <archivo.bf> ───────────────────────────────────────────────────── */
void cmd_bfrun(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) {
        vga_println("Uso: bfrun <archivo.bf>");
        return;
    }

    int idx = resolve_path(name);
    if (idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(name);
        return;
    }

    FSNode *n = fs_get(idx);
    if (n->type != NODE_FILE) {
        vga_println("bfrun solo funciona con archivos.");
        return;
    }

    const char *src = fs_read(idx);
    if (!src || src[0] == '\0') {
        vga_println("El archivo esta vacio.");
        return;
    }

    vga_print_color("[BF] Ejecutando: ", VGA_DARK_GRAY, VGA_BLACK);
    vga_println(name);

    BFResult r = bf_run(src, k_strlen(src));
    bf_report(r);
}

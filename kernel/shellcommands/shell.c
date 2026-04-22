#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"

/* ── Módulos de comandos ──────────────────────────────────────────────────── */
#include "shellcommands/shell_defs.h"
#include "shellcommands/cmd_system.h"
#include "shellcommands/cmd_fs.h"
#include "shellcommands/cmd_math.h"
#include "shellcommands/cmd_misc.h"
#include "shellcommands/cmd_bf.h"
#include "shellcommands/cmd_bossc.h"

/* ── Tipos del kernel ────────────────────────────────────────────────────── */
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

/* ── Utilidades string (sin libc) ─────────────────────────────────────────── */
int k_strlen(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}
int k_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
int k_strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
void k_memset(void *p, int v, int n) {
    unsigned char *b = p; while (n--) *b++ = v;
}

const char *ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ── Estado global del shell ──────────────────────────────────────────────── */
char input[INPUT_MAX];
int  input_len;
int  cwd;

/* ── Historial ────────────────────────────────────────────────────────────── */
static char history[HISTORY_MAX][INPUT_MAX];
int  hist_count = 0;
static int  hist_head = 0;

static void hist_push(const char *line) {
    if (k_strlen(line) == 0) return;
    int last = (hist_head - 1 + HISTORY_MAX) % HISTORY_MAX;
    if (hist_count > 0 && k_strcmp(history[last], line) == 0) return;
    int i = 0;
    while (line[i] && i < INPUT_MAX - 1) { history[hist_head][i] = line[i]; i++; }
    history[hist_head][i] = '\0';
    hist_head = (hist_head + 1) % HISTORY_MAX;
    if (hist_count < HISTORY_MAX) hist_count++;
}

const char *hist_get(int offset) {
    if (offset < 0 || offset >= hist_count) return 0;
    int idx = (hist_head - 1 - offset + HISTORY_MAX * 2) % HISTORY_MAX;
    return history[idx];
}

/* ── Helpers del shell (usados también por módulos vía shell_defs.h) ──────── */
int starts_with_cmd(const char *cmd, const char *prefix) {
    int plen = k_strlen(prefix);
    if (k_strncmp(cmd, prefix, plen) != 0) return 0;
    return cmd[plen] == ' ' || cmd[plen] == '\0';
}

const char *arg1(const char *cmd) {
    while (*cmd && *cmd != ' ') cmd++;
    return ltrim(cmd);
}

int resolve_path(const char *path) {
    if (path[0] == '#') {
        int cur = fs_root();
        const char *p = path + 1;
        if (*p == '\0') return cur;
        if (*p != ':') return -1;
        p++;
        while (*p) {
            char seg[FS_MAX_NAME];
            int i = 0;
            while (*p && *p != ':') seg[i++] = *p++;
            seg[i] = '\0';
            if (*p == ':') p++;
            if (i == 0) continue;
            cur = fs_find(cur, seg);
            if (cur == -1) return -1;
        }
        return cur;
    }
    if (k_strcmp(path, "..") == 0) {
        FSNode *n = fs_get(cwd);
        return (n && n->parent != -1) ? n->parent : cwd;
    }
    int cur = cwd;
    const char *p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == ':' || p[2] == '\0')) {
            FSNode *n = fs_get(cur);
            cur = (n && n->parent != -1) ? n->parent : cur;
            p += 2; if (*p == ':') p++;
            continue;
        }
        char seg[FS_MAX_NAME];
        int i = 0;
        while (*p && *p != ':') seg[i++] = *p++;
        seg[i] = '\0';
        if (*p == ':') p++;
        if (i == 0) continue;
        cur = fs_find(cur, seg);
        if (cur == -1) return -1;
    }
    return cur;
}

/* ── I/O de porta (CMOS / reboot / poweroff) ──────────────────────────────── */
uint8_t inb(uint16_t p) {
    uint8_t v; __asm__ volatile ("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
void outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0,%1"::"a"(v),"Nd"(p));
}

/* ── Helpers RTC ──────────────────────────────────────────────────────────── */
uint8_t cmos_read(uint8_t reg) { outb(0x70, reg); return inb(0x71); }
uint8_t bcd2bin(uint8_t v)     { return (v >> 4) * 10 + (v & 0x0F); }
void    print2d(uint8_t n)     { vga_putchar('0' + n/10); vga_putchar('0' + n%10); }

/* ── Helpers de impresión numérica (compartidos con cmd_math) ─────────────── */
void vga_print_long(long v) {
    if (v < 0) { vga_putchar('-'); v = -v; }
    if (v == 0) { vga_putchar('0'); return; }
    char tmp[24]; int tlen = 0;
    while (v > 0) { tmp[tlen++] = '0' + (int)(v % 10); v /= 10; }
    for (int i = tlen - 1; i >= 0; i--) vga_putchar(tmp[i]);
}
void vga_print_hex(long v) {
    if (v < 0) { vga_putchar('-'); v = -v; }
    vga_print("0x");
    if (v == 0) { vga_putchar('0'); return; }
    char tmp[16]; int tlen = 0;
    unsigned long uv = (unsigned long)v;
    while (uv > 0) { int d = (int)(uv & 0xF); tmp[tlen++] = d < 10 ? ('0'+d) : ('A'+d-10); uv >>= 4; }
    for (int i = tlen - 1; i >= 0; i--) vga_putchar(tmp[i]);
}
void vga_print_bin(long v) {
    if (v < 0) { vga_putchar('-'); v = -v; }
    vga_print("0b");
    if (v == 0) { vga_putchar('0'); return; }
    unsigned long uv = (unsigned long)v;
    int msb = 0;
    for (int i = 31; i >= 0; i--) { if (uv & (1UL << i)) { msb = i; break; } }
    for (int i = msb; i >= 0; i--) {
        vga_putchar((uv & (1UL << i)) ? '1' : '0');
        if (i > 0 && i % 4 == 0) vga_putchar('_');
    }
}

/* ── Autocompletado con Tab ───────────────────────────────────────────────── */
static void print_prompt(void);

static const char *cmd_list_tab[] = {
    "echo", "clean", "clear", "ver", "about", "memory", "time", "date",
    "aboutcpu", "fsinfo", "history", "reboot", "poweroff",
    "cat", "write", "append", "find", "copy", "new", "remove",
    "move", "rename", "open", "go", "list", "repeat", "calc", "help",
    "tree", "wordc", "head", "tail", "sleep", "hex", "bin",
    "bf", "bfrun", "run", 0
};

static void do_tab(int cur) {
    input[input_len] = '\0';
    int tok_start = cur;
    while (tok_start > 0 && input[tok_start-1] != ' ') tok_start--;
    int tok_len = cur - tok_start;
    const char *tok = input + tok_start;
    int is_first = 1;
    for (int i = 0; i < tok_start; i++) { if (input[i] != ' ') { is_first = 0; break; } }
    const char *match = 0; int matches = 0;
    if (is_first) {
        for (int i = 0; cmd_list_tab[i]; i++)
            if (k_strncmp(cmd_list_tab[i], tok, tok_len) == 0) { match = cmd_list_tab[i]; matches++; }
    } else {
        int iter = 0, child;
        while ((child = fs_next_child(cwd, &iter)) != -1) {
            FSNode *n = fs_get(child);
            if (k_strncmp(n->name, tok, tok_len) == 0) { match = n->name; matches++; }
        }
    }
    if (matches == 1) {
        int full_len = k_strlen(match);
        int add = full_len - tok_len;
        if (add <= 0 || input_len + add >= INPUT_MAX) return;
        for (int i = input_len; i >= cur; i--) input[i + add] = input[i];
        for (int i = 0; i < add; i++) input[tok_start + tok_len + i] = match[tok_len + i];
        input_len += add; input[input_len] = '\0';
        if (is_first && input_len < INPUT_MAX - 1 && input[tok_start + full_len] != ' ') {
            for (int i = input_len; i >= cur + add; i--) input[i+1] = input[i];
            input[cur + add] = ' '; input_len++; input[input_len] = '\0';
        }
        for (int i = 0; i < cur; i++) vga_putchar('\b');
        for (int i = 0; i < input_len; i++) vga_putchar(input[i]);
    } else if (matches > 1) {
        vga_putchar('\n');
        if (is_first) {
            for (int i = 0; cmd_list_tab[i]; i++)
                if (k_strncmp(cmd_list_tab[i], tok, tok_len) == 0) { vga_print(cmd_list_tab[i]); vga_putchar(' '); }
        } else {
            int iter = 0, child;
            while ((child = fs_next_child(cwd, &iter)) != -1) {
                FSNode *n = fs_get(child);
                if (k_strncmp(n->name, tok, tok_len) == 0) { vga_print(n->name); vga_putchar(' '); }
            }
        }
        vga_putchar('\n'); print_prompt();
        for (int i = 0; i < input_len; i++) vga_putchar(input[i]);
    }
}

/* ── Leer línea con historial, Tab, cursor y flechas ─────────────────────── */
static void read_line(void) {
    input_len = 0; int cur = 0; int hist_pos = -1;
    k_memset(input, 0, INPUT_MAX);
    while (1) {
        unsigned char c = kb_getchar();
        if (c == '\n') {
            for (int i = cur; i < input_len; i++) vga_putchar(input[i]);
            vga_putchar('\n'); input[input_len] = '\0'; hist_push(input); return;
        }
        if (c == KEY_TAB) {
            int old_len = input_len; do_tab(cur);
            int added = input_len - old_len; cur += added > 0 ? added : 0;
            for (int i = cur; i < input_len; i++) vga_putchar('\b');
            continue;
        }
        if (c == KEY_UP) {
            int next = hist_pos + 1; const char *entry = hist_get(next);
            if (!entry) continue;
            hist_pos = next;
            for (int i = 0; i < cur; i++) vga_putchar('\b');
            for (int i = 0; i < input_len; i++) vga_putchar(' ');
            for (int i = 0; i < input_len; i++) vga_putchar('\b');
            input_len = 0;
            while (entry[input_len] && input_len < INPUT_MAX-1) { input[input_len] = entry[input_len]; input_len++; }
            input[input_len] = '\0'; cur = input_len;
            for (int i = 0; i < input_len; i++) vga_putchar(input[i]);
            continue;
        }
        if (c == KEY_DOWN) {
            for (int i = 0; i < cur; i++) vga_putchar('\b');
            for (int i = 0; i < input_len; i++) vga_putchar(' ');
            for (int i = 0; i < input_len; i++) vga_putchar('\b');
            if (hist_pos <= 0) { hist_pos = -1; input_len = 0; cur = 0; input[0] = '\0'; }
            else {
                hist_pos--; const char *entry = hist_get(hist_pos); input_len = 0;
                while (entry[input_len] && input_len < INPUT_MAX-1) { input[input_len] = entry[input_len]; input_len++; }
                input[input_len] = '\0'; cur = input_len;
                for (int i = 0; i < input_len; i++) vga_putchar(input[i]);
            }
            continue;
        }
        if (c == KEY_LEFT)  { if (cur > 0) { cur--; vga_putchar('\b'); } continue; }
        if (c == KEY_RIGHT) { if (cur < input_len) { vga_putchar(input[cur]); cur++; } continue; }
        if (c == KEY_HOME)  { while (cur > 0) { cur--; vga_putchar('\b'); } continue; }
        if (c == KEY_END)   { while (cur < input_len) { vga_putchar(input[cur]); cur++; } continue; }
        if (c >= KEY_UP) continue;
        if (c == '\b') {
            if (cur == 0) continue;
            for (int i = cur-1; i < input_len-1; i++) input[i] = input[i+1];
            input_len--; cur--; input[input_len] = '\0';
            vga_putchar('\b');
            for (int i = cur; i < input_len; i++) vga_putchar(input[i]);
            vga_putchar(' ');
            for (int i = cur; i <= input_len; i++) vga_putchar('\b');
            continue;
        }
        if (input_len < INPUT_MAX - 1) {
            for (int i = input_len; i > cur; i--) input[i] = input[i-1];
            input[cur] = (char)c; input_len++; input[input_len] = '\0';
            for (int i = cur; i < input_len; i++) vga_putchar(input[i]);
            for (int i = cur+1; i < input_len; i++) vga_putchar('\b');
            cur++;
        }
    }
}

/* ── Prompt ───────────────────────────────────────────────────────────────── */
static void print_prompt(void) {
    char pathbuf[FS_PATH_MAX];
    fs_path(cwd, pathbuf, FS_PATH_MAX);
    vga_print_color("tp-bos ", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print_color(pathbuf,  VGA_LIGHT_CYAN,   VGA_BLACK);
    vga_print_color(" > ",    VGA_WHITE,         VGA_BLACK);
}

/* ── Despachador principal ────────────────────────────────────────────────── */
/*
 * Para añadir un nuevo intérprete o módulo de comandos:
 *   1. Crear kernel/shellcommands/cmd_miprog.c + cmd_miprog.h
 *   2. Añadir #include "shellcommands/cmd_miprog.h"  arriba en este archivo
 *   3. Añadir la entrada en dispatch() en la sección correspondiente
 *   4. Añadir el nombre en cmd_list_tab[] para el autocompletado
 */
void dispatch(void) {
    const char *cmd = ltrim(input);
    if (k_strlen(cmd) == 0) return;

    /* ── Sistema ── */
    if (starts_with_cmd(cmd, "echo"))      { cmd_echo(cmd);     return; }
    if (k_strcmp(cmd, "clean") == 0)       { cmd_clear();       return; }
    if (k_strcmp(cmd, "clear") == 0)       { cmd_clear();       return; }
    if (k_strcmp(cmd, "ver") == 0)         { cmd_ver();         return; }
    if (k_strcmp(cmd, "about") == 0)       { cmd_about();       return; }
    if (k_strcmp(cmd, "memory") == 0)      { cmd_memory();      return; }
    if (k_strcmp(cmd, "time") == 0)        { cmd_time();        return; }
    if (k_strcmp(cmd, "date") == 0)        { cmd_date();        return; }
    if (k_strcmp(cmd, "aboutcpu") == 0)    { cmd_aboutcpu();    return; }
    if (k_strcmp(cmd, "fsinfo") == 0)      { cmd_fsinfo();      return; }
    if (k_strcmp(cmd, "history") == 0)     { cmd_history();     return; }
    if (k_strcmp(cmd, "reboot") == 0)      { cmd_reboot();      return; }
    if (k_strcmp(cmd, "poweroff") == 0)    { cmd_poweroff();    return; }
    if (starts_with_cmd(cmd, "sleep"))     { cmd_sleep(cmd);    return; }

    /* ── Filesystem ── */
    if (starts_with_cmd(cmd, "cat"))       { cmd_cat(cmd);      return; }
    if (starts_with_cmd(cmd, "write"))     { cmd_write(cmd);    return; }
    if (starts_with_cmd(cmd, "append"))    { cmd_append(cmd);   return; }
    if (starts_with_cmd(cmd, "find"))      { cmd_find(cmd);     return; }
    if (starts_with_cmd(cmd, "copy"))      { cmd_copy(cmd);     return; }
    if (starts_with_cmd(cmd, "new"))       { cmd_new(cmd);      return; }
    if (starts_with_cmd(cmd, "remove"))    { cmd_remove(cmd);   return; }
    if (starts_with_cmd(cmd, "move"))      { cmd_move(cmd);     return; }
    if (starts_with_cmd(cmd, "rename"))    { cmd_rename(cmd);   return; }
    if (starts_with_cmd(cmd, "open"))      { cmd_open(cmd);     return; }
    if (starts_with_cmd(cmd, "go"))        { cmd_go(cmd);       return; }
    if (k_strcmp(cmd, "list") == 0)        { cmd_list();        return; }
    if (starts_with_cmd(cmd, "tree"))      { cmd_tree(cmd);     return; }
    if (starts_with_cmd(cmd, "wordc"))     { cmd_wordc(cmd);    return; }
    if (starts_with_cmd(cmd, "head"))      { cmd_head(cmd);     return; }
    if (starts_with_cmd(cmd, "tail"))      { cmd_tail(cmd);     return; }

    /* ── Math ── */
    if (starts_with_cmd(cmd, "calc"))      { cmd_calc(cmd);     return; }
    if (starts_with_cmd(cmd, "hex"))       { cmd_hex(cmd);      return; }
    if (starts_with_cmd(cmd, "bin"))       { cmd_bin(cmd);      return; }

    /* ── Misc ── */
    if (starts_with_cmd(cmd, "repeat"))    { cmd_repeat(cmd);   return; }
    if (k_strcmp(cmd, "help") == 0)        { cmd_help();        return; }

    /* ── Intérpretes ── */
    if (starts_with_cmd(cmd, "bf"))        { cmd_bf(cmd);     return; }
    if (starts_with_cmd(cmd, "bfrun"))     { cmd_bfrun(cmd);  return; }
    if (starts_with_cmd(cmd, "run"))       { cmd_run(cmd);    return; }
    /* ── [ Futuros intérpretes — añadir aquí ] ──────────────────────────────
     * if (k_strcmp(cmd, "forth") == 0) { cmd_forth(cmd); return; }
     * ────────────────────────────────────────────────────────────────────── */

    vga_print_color("Comando no reconocido: ", VGA_LIGHT_RED, VGA_BLACK);
    vga_println(cmd);
    vga_println("Escribe 'help' para ver los comandos disponibles.");
}

/* ── Punto de entrada del shell ───────────────────────────────────────────── */
static const char *help_content =
    "Comandos disponibles:\n"
    "  echo [texto] -> Imprime texto | clean -> Limpiar\n"
    "  ver -> Version                | about -> Info\n"
    "  memory -> Info de memoria     | time -> Hora\n"
    "  date -> Fecha RTC             | aboutcpu -> Info del CPU\n"
    "  fsinfo -> Estadisticas del FS | history -> Historial\n"
    "  reboot -> Reiniciar           | poweroff -> Apagar\n"
    "  cat [archivo] -> Ver archivo  | write [archivo] [texto] -> Escribir\n"
    "  find [nombre] -> Buscar       | copy [origen] [destino] -> Copiar\n"
    "  remove [nombre] -> Eliminar   | move [archivo] to [carpeta] -> Mover\n"
    "  help -> Mostrar esta ayuda    | open [archivo] -> Abrir/editar archivo\n"
    "  go [carpeta] -> Ir a carpeta  | list -> Listar contenido\n"
    "  repeat [N] [cmd] -> Repetir   | append [archivo] [texto] -> Anadir\n"
    "  new [file|dir] nombre -> Crear| rename [nombre] to [nuevo] -> Renombrar\n"
    "  calc [expr] -> Calculadora    | tree [dir] -> Arbol del FS\n"
    "  wordc [arch] -> Contar lineas | head [arch] [N] -> Primeras N lineas\n"
    "  tail [arch] [N] -> Ultimas N  | sleep [N] -> Pausa N segundos\n"
    "  hex [num] -> Decimal a hex    | bin [num] -> Decimal a binario\n"
    "  bfrun [archivo] -> Correr .bf | date -> Fecha\n";

void shell_run(void) {
    int root = fs_root();
    int home = fs_find(root, "home");
    if (home < 0) home = fs_create(root, "home", NODE_DIR);
    if (home >= 0) {
        int help_file = fs_find(home, "help.tpbi");
        if (help_file < 0) help_file = fs_create(home, "help.tpbi", NODE_FILE);
        if (help_file >= 0) fs_write(help_file, help_content);
    }
    cwd = (home >= 0) ? home : root;
    while (1) { print_prompt(); read_line(); dispatch(); }
}

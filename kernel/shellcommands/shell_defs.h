#ifndef SHELL_DEFS_H
#define SHELL_DEFS_H

/* ── Dependencias del kernel ──────────────────────────────────────────────── */
#include "../vga.h"
#include "../keyboard.h"
#include "../fs.h"

/* ── Tipos del kernel ─────────────────────────────────────────────────────── */
#ifndef UINT8_T_DEFINED
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
#define UINT8_T_DEFINED
#endif

/* ── Constantes del shell ─────────────────────────────────────────────────── */
#define INPUT_MAX   256
#define HISTORY_MAX  16

/* ── Estado global del shell (definido en shell.c) ───────────────────────── */
extern char        input[INPUT_MAX];
extern int         input_len;
extern int         cwd;

/* ── Historial (definido en shell.c) ─────────────────────────────────────── */
extern int         hist_count;
const char        *hist_get(int offset);

/* ── Forward declaration del despachador ─────────────────────────────────── */
void dispatch(void);

/* ── Utilidades string (definidas en shell.c) ────────────────────────────── */
int         k_strlen (const char *s);
int         k_strcmp (const char *a, const char *b);
int         k_strncmp(const char *a, const char *b, int n);
void        k_memset (void *p, int v, int n);
const char *ltrim    (const char *s);

/* ── Helpers del shell (definidos en shell.c) ────────────────────────────── */
int         starts_with_cmd(const char *cmd, const char *prefix);
const char *arg1           (const char *cmd);
int         resolve_path   (const char *path);

/* ── I/O de puertos (definidos en shell.c) ───────────────────────────────── */
uint8_t inb (uint16_t port);
void    outb(uint16_t port, uint8_t val);

/* ── Helpers RTC (definidos en shell.c) ──────────────────────────────────── */
uint8_t cmos_read(uint8_t reg);
uint8_t bcd2bin  (uint8_t v);
void    print2d  (uint8_t n);

/* ── Helpers de impresión numérica (definidos en shell.c) ────────────────── */
void vga_print_long(long v);
void vga_print_hex (long v);
void vga_print_bin (long v);

#endif /* SHELL_DEFS_H */

#ifndef VGA_H
#define VGA_H

/*
 * En modo 64-bit no tenemos stdint.h (compilamos con -nostdinc),
 * así que definimos los tipos manualmente.
 * Usamos __UINT8_TYPE__ etc. si el compilador los provee,
 * o caemos en los tamaños garantizados para x86_64.
 */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/* Colores VGA */
#define VGA_BLACK         0
#define VGA_BLUE          1
#define VGA_GREEN         2
#define VGA_CYAN          3
#define VGA_RED           4
#define VGA_MAGENTA       5
#define VGA_BROWN         6
#define VGA_LIGHT_GRAY    7
#define VGA_DARK_GRAY     8
#define VGA_LIGHT_BLUE    9
#define VGA_LIGHT_GREEN   10
#define VGA_LIGHT_CYAN    11
#define VGA_LIGHT_RED     12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW        14
#define VGA_WHITE         15

/* Dimensiones de la pantalla */
#define VGA_COLS 80
#define VGA_ROWS 25

void vga_init(void);
void vga_clear(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_putchar(char c);
void vga_print(const char *str);
void vga_println(const char *str);
void vga_print_color(const char *str, uint8_t fg, uint8_t bg);
void vga_print_int(int n);

#endif

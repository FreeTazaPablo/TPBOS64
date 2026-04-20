#ifndef CMD_MATH_H
#define CMD_MATH_H

/* calc, hex, bin */

void cmd_calc(const char *line);
void cmd_hex (const char *line);
void cmd_bin (const char *line);

/* Helpers de impresión numérica (usadas también por otros módulos) */
void vga_print_long(long v);
void vga_print_hex (long v);
void vga_print_bin (long v);

#endif /* CMD_MATH_H */

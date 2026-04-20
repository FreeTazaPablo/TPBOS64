#ifndef CMD_BF_H
#define CMD_BF_H

/*
 * cmd_bf  — Intérprete Brainfuck para TP-BOS
 *
 * Uso:
 *   bf <programa>        ejecuta código BF inline
 *   bfrun <archivo>      ejecuta un archivo .bf del FS
 *
 * Operadores soportados: + - < > [ ] . ,
 *   .  →  vga_putchar (salida)
 *   ,  →  kb_getchar  (entrada)
 *
 * Límites (ajustables con los #defines de abajo):
 *   BF_TAPE_SIZE   celdas de la cinta  (512 bytes)
 *   BF_MAX_STEPS   pasos máximos       (2 000 000) — evita bucles infinitos
 *   BF_MAX_DEPTH   profundidad máx de  [ ]  anidados (64)
 */

#define BF_TAPE_SIZE  512
#define BF_MAX_STEPS  2000000
#define BF_MAX_DEPTH  64

void cmd_bf   (const char *line);   /* bf <código inline>  */
void cmd_bfrun(const char *line);   /* bfrun <archivo.bf>  */

#endif /* CMD_BF_H */

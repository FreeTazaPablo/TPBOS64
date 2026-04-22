#ifndef CMD_BOSSC_H
#define CMD_BOSSC_H

/*
 * cmd_bossc — Interprete BOSSC (BOS Script) para TP-BOS
 *
 * Lenguaje de scripting nativo del sistema.
 * Extension de archivo: .tpbi
 *
 * Caracteristicas:
 *   echo MENSAJE          — imprime texto
 *   $$ comentario         — linea ignorada
 *   declaratev VAR = VAL  — declara/asigna variable
 *   declaratef F(a,b)=EXP — declara funcion
 *   if COND [ ... ]       — condicional con elsif/else
 *   casecon [ case ... ]  — selector de casos
 *   repeat N CMD/[BLOQUE] — repite N veces
 *   infinity CMD/[BLOQUE] — bucle infinito (Ctrl+C para salir)
 *   while COND CMD/[BLQ]  — bucle condicional
 *   run ARCHIVO.tpbi      — ejecuta un script desde el FS
 *
 * Limites:
 *   BOSSC_MAX_VARS   variables simultaneas   (32)
 *   BOSSC_MAX_FUNCS  funciones definidas      (16)
 *   BOSSC_MAX_ARGS   argumentos por funcion   (8)
 *   BOSSC_MAX_DEPTH  profundidad de bloques   (16)
 *   BOSSC_MAX_STEPS  pasos maximos por script (100000) — evita bucles infinitos
 */

#define BOSSC_MAX_VARS    32
#define BOSSC_MAX_FUNCS   16
#define BOSSC_MAX_ARGS     8
#define BOSSC_MAX_DEPTH   16
#define BOSSC_MAX_STEPS   100000
#define BOSSC_VAR_NAME    24
#define BOSSC_VAR_VAL     64
#define BOSSC_FUNC_BODY  512

/* Ejecuta un script desde el contenido de texto (llamado por run y desde shell) */
void bossc_exec(const char *source);

/* Comando: run archivo.tpbi */
void cmd_run(const char *line);

#endif /* CMD_BOSSC_H */

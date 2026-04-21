#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "fs_persist.h"
#include "shell.h"

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void pit_disable(void) {
    outb(0x43, 0x30);
    outb(0x40, 0x00);
    outb(0x40, 0x00);
}

static void pic_disable(void) {
    outb(0xA1, 0xFF);
    outb(0x21, 0xFF);
}

/*
 * En x86_64 SysV ABI los argumentos llegan en registros (rdi, rsi),
 * no en el stack como en 32 bits. boot.asm ya coloca mb_magic en rdi
 * y mb_info en rsi antes de llamar a kernel_main.
 *
 * Usamos unsigned int (32 bits) porque Multiboot devuelve valores de 32 bits.
 */
void kernel_main(unsigned int mb_magic, unsigned int mb_info) {
    (void)mb_magic;
    (void)mb_info;

    __asm__ volatile ("cli");
    pic_disable();
    pit_disable();

    vga_init();
    kb_init();
    fs_load_from_disk();

    /* ── Banner de arranque ──────────────────────────────────────────────── */
    vga_print_color(
        "TP-BOS v0.1 [Taza Pablo Boot Operating System] (64-bit)\n",
        VGA_WHITE, VGA_BLACK
    );
    vga_print_color(
        "Copyright (C) 2026. All rights reserved.\n",
        VGA_LIGHT_GRAY, VGA_BLACK
    );
    vga_putchar('\n');
    vga_print_color(
        "Escribe 'help' para ver los comandos disponibles.\n",
        VGA_DARK_GRAY, VGA_BLACK
    );
    vga_putchar('\n');

    /* ── Shell — nunca retorna ───────────────────────────────────────────── */
    shell_run();

    /* Por si acaso */
    while (1) __asm__ volatile ("pause");
}

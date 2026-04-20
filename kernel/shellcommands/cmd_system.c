#include "shell_defs.h"
#include "cmd_system.h"

/* ── echo ─────────────────────────────────────────────────────────────────── */
void cmd_echo(const char *line) {
    const char *msg = arg1(line);
    vga_println(msg);
}

/* ── ver ──────────────────────────────────────────────────────────────────── */
void cmd_ver(void) {
    vga_print_color("TP-BOS ", VGA_WHITE, VGA_BLACK);
    vga_print_color("v0.1", VGA_YELLOW, VGA_BLACK);
    vga_print_color(" [Taza Pablo Boot Operating System]\n", VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print_color("Build: ", VGA_DARK_GRAY, VGA_BLACK);
    vga_print_color(__DATE__ " " __TIME__ "\n", VGA_DARK_GRAY, VGA_BLACK);
}

/* ── about ────────────────────────────────────────────────────────────────── */
void cmd_about(void) {
    vga_print_color("TP-BOS v0.1\n", VGA_YELLOW, VGA_BLACK);
    vga_println("Taza Pablo Boot Operating System");
    vga_println("Un OS minimalista de 32 bits escrito en C y Assembly.");
    vga_println("Arquitectura: x86  |  Boot: Multiboot2  |  VGA: 80x25");
}

/* ── memory ───────────────────────────────────────────────────────────────── */
void cmd_memory(void) {
    vga_print_color("Memoria del sistema\n", VGA_YELLOW, VGA_BLACK);
    vga_println("  RAM detectada : consulta los logs de GRUB al arrancar.");
    vga_println("  Kernel cargado en : 0x00100000 (1 MB)");
    vga_println("  Buffer VGA   : 0x000B8000");
    vga_println("  Stack        : 16 KB reservados en .bss");
    vga_println("  FS en RAM    : estatico en .bss del kernel");
}

/* ── time ─────────────────────────────────────────────────────────────────── */
void cmd_time(void) {
    uint8_t h = bcd2bin(cmos_read(0x04));
    uint8_t m = bcd2bin(cmos_read(0x02));
    uint8_t s = bcd2bin(cmos_read(0x00));
    vga_print_color("Hora: ", VGA_YELLOW, VGA_BLACK);
    print2d(h); vga_putchar(':'); print2d(m); vga_putchar(':'); print2d(s);
    vga_putchar('\n');
}

/* ── date ─────────────────────────────────────────────────────────────────── */
void cmd_date(void) {
    uint8_t day  = bcd2bin(cmos_read(0x07));
    uint8_t mon  = bcd2bin(cmos_read(0x08));
    uint8_t year = bcd2bin(cmos_read(0x09));
    vga_print_color("Fecha: ", VGA_YELLOW, VGA_BLACK);
    print2d(day); vga_putchar('/'); print2d(mon);
    vga_print("/20"); print2d(year);
    vga_putchar('\n');
}

/* ── aboutcpu ─────────────────────────────────────────────────────────────── */
void cmd_aboutcpu(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    char vendor[13];
    for (int i = 0; i < 4; i++) vendor[i]   = (char)((ebx >> (i*8)) & 0xFF);
    for (int i = 0; i < 4; i++) vendor[4+i] = (char)((edx >> (i*8)) & 0xFF);
    for (int i = 0; i < 4; i++) vendor[8+i] = (char)((ecx >> (i*8)) & 0xFF);
    vendor[12] = '\0';

    uint32_t info;
    __asm__ volatile ("cpuid" : "=a"(info) : "a"(1) : "ebx","ecx","edx");
    uint8_t stepping = (uint8_t)(info & 0xF);
    uint8_t model    = (uint8_t)((info >> 4) & 0xF);
    uint8_t family   = (uint8_t)((info >> 8) & 0xF);

    vga_print_color("CPU Info\n", VGA_YELLOW, VGA_BLACK);
    vga_print("  Vendor   : "); vga_println(vendor);
    vga_print("  Familia  : "); vga_print_int(family);   vga_putchar('\n');
    vga_print("  Modelo   : "); vga_print_int(model);    vga_putchar('\n');
    vga_print("  Stepping : "); vga_print_int(stepping); vga_putchar('\n');
    vga_print("  Max CPUID: "); vga_print_int(eax);      vga_putchar('\n');
}

/* ── fsinfo ───────────────────────────────────────────────────────────────── */
void cmd_fsinfo(void) {
    int used = 0, dirs = 0, files = 0, total_bytes = 0;
    for (int i = 0; i < FS_MAX_NODES; i++) {
        FSNode *n = fs_get(i);
        if (n && n->used) {
            used++;
            if (n->type == NODE_DIR) dirs++;
            else { files++; total_bytes += n->content_len; }
        }
    }
    vga_print_color("Sistema de archivos (RAM)\n", VGA_YELLOW, VGA_BLACK);
    vga_print("  Nodos totales : "); vga_print_int(FS_MAX_NODES);       vga_putchar('\n');
    vga_print("  Nodos usados  : "); vga_print_int(used);                vga_putchar('\n');
    vga_print("  Nodos libres  : "); vga_print_int(FS_MAX_NODES - used); vga_putchar('\n');
    vga_print("  Directorios   : "); vga_print_int(dirs);                vga_putchar('\n');
    vga_print("  Archivos      : "); vga_print_int(files);               vga_putchar('\n');
    vga_print("  Bytes en uso  : "); vga_print_int(total_bytes);         vga_putchar('\n');
    vga_print("  Max por archivo: "); vga_print_int(FS_MAX_CONTENT);     vga_println(" bytes");
}

/* ── history ──────────────────────────────────────────────────────────────── */
void cmd_history(void) {
    if (hist_count == 0) { vga_println("  (sin historial)"); return; }
    for (int i = hist_count - 1; i >= 0; i--) {
        const char *e = hist_get(i);
        if (!e) continue;
        vga_print_color("  ", VGA_DARK_GRAY, VGA_BLACK);
        vga_print_int(hist_count - i);
        vga_print("  ");
        vga_println(e);
    }
}

/* ── clear / clean ────────────────────────────────────────────────────────── */
void cmd_clear(void) {
    vga_clear();
}

/* ── reboot ───────────────────────────────────────────────────────────────── */
void cmd_reboot(void) {
    vga_println("Reiniciando...");
    uint8_t tmp;
    do { tmp = inb(0x64); } while (tmp & 0x02);
    outb(0x64, 0xFE);
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    volatile uint8_t idt_null[6] = {0,0,0,0,0,0};
    __asm__ volatile ("cli; lidt %0; int $3" : : "m"(idt_null));
    while (1) __asm__ volatile ("pause");
}

/* ── poweroff ─────────────────────────────────────────────────────────────── */
void cmd_poweroff(void) {
    vga_println("Apagando el sistema...");
    outb(0x604, 0x00);
    outb(0x605, 0x20);
    outb(0xB0, 0x04);
    outb(0xB0, 0x00);
    outb(0x604, 0x34);
    while (1) __asm__ volatile ("cli; pause");
}

/* ── sleep ────────────────────────────────────────────────────────────────── */
void cmd_sleep(const char *line) {
    const char *nstr = arg1(line);
    if (k_strlen(nstr) == 0) { vga_println("Uso: sleep N"); return; }

    int secs = 0;
    while (*nstr >= '0' && *nstr <= '9') secs = secs * 10 + (*nstr++ - '0');
    if (secs <= 0) { vga_println("Numero invalido."); return; }
    if (secs > 60) { vga_println("Maximo 60 segundos."); return; }

    vga_print("Esperando ");
    vga_print_int(secs);
    vga_println(" segundo(s)...");

    uint8_t prev = bcd2bin(cmos_read(0x00));
    int elapsed = 0;
    while (elapsed < secs) {
        uint8_t cur_s = bcd2bin(cmos_read(0x00));
        if (cur_s != prev) { prev = cur_s; elapsed++; }
        __asm__ volatile ("pause");
    }
    vga_println("Listo.");
}

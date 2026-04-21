# ── Toolchain ─────────────────────────────────────────────────────────────────
#
# Si tienes un cross-compiler x86_64-elf-gcc úsalo directamente:
#   CC = x86_64-elf-gcc
#   LD = x86_64-elf-ld
#
# Si solo tienes el GCC del sistema (normalmente ya es x86_64), usa:
#   CC = gcc
#   LD = ld
#
CC        = gcc
LD        = ld

CFLAGS    = -m64 -ffreestanding -O2 -Wall -Wextra       \
            -fno-stack-protector -fno-builtin            \
            -nostdlib -nostdinc                          \
            -fno-pic -fno-pie                            \
            -mno-sse -mno-sse2 -mno-mmx                 \
            -mno-red-zone                                \
            -Ikernel

# -mno-red-zone: CRÍTICO en kernels 64-bit.
# Sin esto el compilador usa los 128 bytes bajo el stack pointer,
# lo que causa corrupción de pila cuando llegan interrupciones.

NASM      = nasm
NASMFLAGS = -f elf64

LDFLAGS   = -m elf_x86_64 -T linker.ld -nostdlib -z max-page-size=0x1000

OBJ = kernel/boot.o                          \
      kernel/vga.o                           \
      kernel/keyboard.o                      \
      kernel/fs.o                            \
      kernel/ata.o                           \
      kernel/fs_persist.o                    \
      kernel/shell.o                         \
      kernel/shellcommands/cmd_system.o      \
      kernel/shellcommands/cmd_fs.o          \
      kernel/shellcommands/cmd_math.o        \
      kernel/shellcommands/cmd_misc.o        \
      kernel/shellcommands/cmd_bf.o          \
      kernel/kernel.o

ISO  = tpbos.iso
BIN  = tpbos.bin
DISK = disk.img

# Tamaño del disco virtual: 1 MB (2048 sectores × 512 bytes)
DISK_SECTORS = 2048

.PHONY: all iso clean clean-disk run

all: $(BIN)

# ── Imagen de disco persistente ───────────────────────────────────────────────
$(DISK):
	dd if=/dev/zero of=$(DISK) bs=512 count=$(DISK_SECTORS)
	@echo "  Disco virtual creado: $(DISK)"

kernel/boot.o: kernel/boot.asm
	$(NASM) $(NASMFLAGS) $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/shellcommands/%.o: kernel/shellcommands/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

iso: $(BIN)
	mkdir -p iso/boot/grub
	cp $(BIN)             iso/boot/tpbos.bin
	cp boot/grub/grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso
	@echo ""
	@echo "  ISO lista: $(ISO)"

# ── Run: qemu-system-x86_64 ──────────────────────────────────────────────────
run: iso $(DISK)
	qemu-system-x86_64                                  \
	  -cdrom $(ISO)                                     \
	  -drive file=$(DISK),format=raw,index=1,media=disk \
	  -m 64M -no-reboot -no-shutdown

# ── clean NO borra disk.img ───────────────────────────────────────────────────
clean:
	rm -f kernel/*.o kernel/shellcommands/*.o $(BIN) $(ISO)
	rm -rf iso
	@echo "  Nota: $(DISK) no se borró. Usa 'make clean-disk' para borrarlo también."

# ── Borrar todo incluyendo el disco ──────────────────────────────────────────
clean-disk: clean
	rm -f $(DISK)
	@echo "  Disco virtual eliminado."

# TP-BOS

**Taza Pablo Boot Operating System** - sistema operativo de terminal inspirado en MS-DOS, escrito desde cero en C y Assembly x86.

Corre en bare-metal (o QEMU), con su propio kernel, shell interactivo, sistema de archivos en RAM con **persistencia en disco**, driver ATA PIO, driver VGA de texto, y hasta un intérprete de Brainfuck!!

---

## Características

- **Shell interactivo** con historial, autocompletado con Tab, cursor con flechas, Home/End
- **Sistema de archivos** jerárquico en RAM (directorios, archivos, rutas tipo `#:home:docs`)
- **Persistencia real** — los archivos sobreviven reinicios gracias a un driver ATA PIO propio
- **Editor de texto** integrado (`open archivo`) con Ctrl+S para guardar y Ctrl+F para buscar
- **Calculadora** con soporte para expresiones matemáticas (`calc 2+3*4`)
- **Intérprete Brainfuck** (`bfrun`)
- Sin libc, sin dependencias externas — todo escrito desde cero

---

## Requisitos

```bash
# Debian/Ubuntu
sudo apt install gcc gcc-multilib nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools
```

```bash
# Arch/Manjaro
sudo pacman -S --needed gcc nasm qemu-base grub libisoburn mtools
```
---

## Cómo compilar y correr

```bash
# Compilar y lanzar en QEMU
make run

# Solo compilar
make

# Limpiar objetos e ISO (conserva disk.img con tus archivos)
make clean

# Limpiar todo incluyendo el disco persistente
make clean-disk
```

> La primera vez que corras `make run` se crea automáticamente `disk.img` (disco virtual de 1 MB).
> A partir de ahí, todo lo que guardes persiste entre reinicios.

---

## Comandos del shell que todavi no le pongo nombre propio

| Comando | Descripción |
|---|---|
| `help` | Muestra todos los comandos |
| `echo [texto]` | Imprime texto |
| `clear` / `clean` | Limpia la pantalla |
| `ver` | Versión del OS |
| `about` | Información del sistema |
| `memory` | Info de memoria |
| `time` / `date` | Hora y fecha (RTC) |
| `aboutcpu` | Info del CPU |
| `reboot` / `poweroff` | Reiniciar / apagar |
| `sleep [N]` | Pausa N segundos |
| **Filesystem** | |
| `list` | Listar directorio actual |
| `tree [dir]` | Árbol del FS |
| `go [dir]` | Cambiar directorio |
| `new file\|dir nombre` | Crear archivo o directorio |
| `remove [nombre]` | Eliminar archivo o directorio |
| `cat [archivo]` | Ver contenido de archivo |
| `write [archivo] [texto]` | Escribir en archivo |
| `append [archivo] [texto]` | Añadir texto al final |
| `open [archivo]` | Abrir editor de texto |
| `copy [origen] [destino]` | Copiar archivo |
| `move [archivo] to [dir]` | Mover archivo |
| `rename [nombre] to [nuevo]` | Renombrar |
| `find [nombre]` | Buscar en todo el FS |
| `head [arch] [N]` | Primeras N líneas |
| `tail [arch] [N]` | Últimas N líneas |
| `wordc [arch]` | Contar líneas, palabras y bytes |
| `fsinfo` | Estadísticas del FS |
| **Matemáticas** | |
| `calc [expr]` | Calculadora |
| `hex [num]` | Decimal a hexadecimal |
| `bin [num]` | Decimal a binario |
| **Intérpretes** | |
| `bfrun [archivo]` | Ejecutar archivo `.bf` |
| **Misc** | |
| `repeat [N] [cmd]` | Repetir un comando N veces |
| `history` | Ver historial de comandos |

---

## Estructura de carpetas

```
TP-BOS/
├── boot/
│   └── grub/
│       └── grub.cfg          # Configuración del bootloader
├── kernel/
│   ├── boot.asm              # Entry point (Multiboot)
│   ├── kernel.c              # kernel_main — inicialización
│   ├── vga.c / vga.h         # Driver VGA de texto
│   ├── keyboard.c / keyboard.h  # Driver de teclado (PS/2)
│   ├── fs.c / fs.h           # Sistema de archivos en RAM
│   ├── ata.c / ata.h         # Driver ATA PIO (disco)
│   ├── fs_persist.c / fs_persist.h  # Persistencia FS ↔ disco
│   ├── shell.c / shell.h     # Shell principal y dispatcher
│   └── shellcommands/
│       ├── cmd_fs.c          # Comandos de filesystem
│       ├── cmd_system.c      # Comandos de sistema
│       ├── cmd_math.c        # Calculadora
│       ├── cmd_misc.c        # Comandos varios
│       └── cmd_bf.c          # Intérprete Brainfuck
├── linker.ld                 # Script del linker
└── Makefile
```

---

## Persistencia

TP-BOS usa un driver ATA PIO propio para guardar el sistema de archivos en un disco virtual (`disk.img`). El layout en disco es:

```
Sector 0        → Magic header "TPBF" (4 bytes)
Sectores 1-524  → Array FSNode[128] serializado (~262 KB)
```

Cada vez que modificas un archivo, el FS completo se guarda automáticamente al disco. Al reiniciar, se detecta el magic y se restaura el estado anterior.

---

## Licencia

GPL-3.0 — ver [LICENSE](LICENSE)

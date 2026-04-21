; TP-BOS boot stub — Multiboot1, transición a largo modo (64-bit)
; Compilar con: nasm -f elf64

bits 32

; ── Multiboot header ──────────────────────────────────────────────────────────
section .multiboot
align 4
    dd 0x1BADB002             ; Magic Multiboot 1
    dd 0x03                   ; Flags (memoria + alineación)
    dd -(0x1BADB002 + 0x03)   ; Checksum

; ── Tablas de paginación (identity map primeros 1 GB) ─────────────────────────
section .bss
align 4096
p4_table:                     ; PML4  (nivel 4)
    resb 4096
p3_table:                     ; PDPT  (nivel 3)
    resb 4096
p2_table:                     ; PD    (nivel 2, huge pages de 2 MB)
    resb 4096

stack_bottom:
    resb 16384
stack_top:

; ── GDT de 64 bits ────────────────────────────────────────────────────────────
section .rodata
align 8
gdt64:
    dq 0                                        ; Descriptor null
.code: equ $ - gdt64
    ; Código 64-bit: L=1, P=1, S=1, Type=0xA (exec+read), DPL=0
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.data: equ $ - gdt64
    ; Datos: W=1, P=1, S=1, Type=0x2 (read+write), DPL=0
    dq (1<<41) | (1<<44) | (1<<47)
.pointer:
    dw $ - gdt64 - 1          ; Límite
    dq gdt64                  ; Base

; ── Entry point (arranca en modo protegido 32-bit) ────────────────────────────
section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top

    ; Guardar argumentos Multiboot (magic en eax, info en ebx)
    ; Los usaremos al final para pasarlos a kernel_main
    mov edi, eax              ; mb_magic  → edi  (arg1 en x86_64 SysV ABI)
    mov esi, ebx              ; mb_info   → esi  (arg2)

    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    ; Cargar GDT de 64 bits
    lgdt [gdt64.pointer]

    ; Far jump → selector de código 64-bit → activa largo modo completamente
    jmp gdt64.code:long_mode_start

; ── Verificar soporte de CPUID ────────────────────────────────────────────────
check_cpuid:
    ; Si podemos modificar el bit ID del EFLAGS, CPUID está disponible
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    ; No hay CPUID — colgar
    cli
    hlt

; ── Verificar soporte de largo modo ──────────────────────────────────────────
check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)       ; Bit LM
    jz .no_long_mode
    ret
.no_long_mode:
    cli
    hlt

; ── Construir tablas de paginación ───────────────────────────────────────────
setup_page_tables:
    ; P4[0] → P3  (present + writable)
    mov eax, p3_table
    or  eax, 0b11
    mov [p4_table], eax

    ; P3[0] → P2  (present + writable)
    mov eax, p2_table
    or  eax, 0b11
    mov [p3_table], eax

    ; P2: 512 entradas de 2 MB cada una (huge pages) → cubre 1 GB
    mov ecx, 0
.map_p2_loop:
    mov eax, 0x200000         ; 2 MB
    mul ecx
    or  eax, 0b10000011       ; huge + writable + present
    mov [p2_table + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_loop
    ret

; ── Habilitar paginación y modo largo ────────────────────────────────────────
enable_paging:
    ; Cargar P4 en CR3
    mov eax, p4_table
    mov cr3, eax

    ; Habilitar PAE (Physical Address Extension) — requerido para largo modo
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; Poner EFER.LME (Long Mode Enable)
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    ; Habilitar paginación (PG) y mantener modo protegido (PE)
    mov eax, cr0
    or  eax, (1 << 31) | (1 << 0)
    mov cr0, eax
    ret

; ── Código de 64 bits ─────────────────────────────────────────────────────────
bits 64
long_mode_start:
    ; Recargar selectores de segmento con descriptor de datos 64-bit
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; edi y esi ya contienen mb_magic y mb_info (establecidos arriba en 32-bit)
    ; En x86_64 SysV ABI los primeros argumentos van en rdi, rsi
    ; Los registros de 64 bits (rdi, rsi) ya tienen los valores correctos
    ; porque mov edi/esi hace zero-extend automático a rdi/rsi
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .text

global gdt_load

gdt_load:
    cli
    lgdt [rdi]

    push 0x08
    lea rax, [rel .reload_CS]
    push rax
    retfq ; In order to set the CS segment we need to perform a far jump

.reload_CS:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
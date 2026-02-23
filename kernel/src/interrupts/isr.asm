; Otherwise nasm complains that if the linker relocates
; interrupt_handler too far it may break (won't happen :) )
[warning -reloc-abs-qword] 
[warning -reloc-rel-dword]

default rel
section .text

; We define a stub for the isr's which don't push an error code
%macro no_error_code_interrupt_handler 1
global interrupt_handler_%1
interrupt_handler_%1:
        push    qword 0                     ; push 0 as error code
        push    qword %1                    ; push the interrupt number
        jmp     common_interrupt_handler    ; jump to the common handler
%endmacro

; We define a stub for the isr's which DO push an error code
%macro error_code_interrupt_handler 1
global interrupt_handler_%1
interrupt_handler_%1:
        push    qword %1                    ; push the interrupt number
        jmp     common_interrupt_handler    ; jump to the common handler
%endmacro

extern interrupt_handler

common_interrupt_handler:
    ; We save ALL the registers (pusha and popa available only in pm)
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rsp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; 1st argument
    mov rdi, rsp

    ; Our external C function
    call interrupt_handler

    ; Restore the previous (or changed) cpu context
    mov rsp, rax

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    add rsp, 8 ; To now overwrite rsp
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    ; Remember that we pushed the error code and the interrupt vector
    add rsp, 0x10

    iretq

; Now we declare those isr's based on the x86_64 manual
no_error_code_interrupt_handler 0
no_error_code_interrupt_handler 1
no_error_code_interrupt_handler 2
no_error_code_interrupt_handler 3
no_error_code_interrupt_handler 4
no_error_code_interrupt_handler 5
no_error_code_interrupt_handler 6
no_error_code_interrupt_handler 7
error_code_interrupt_handler    8
no_error_code_interrupt_handler 9
error_code_interrupt_handler    10
error_code_interrupt_handler    11
error_code_interrupt_handler    12
error_code_interrupt_handler    13
error_code_interrupt_handler    14
no_error_code_interrupt_handler 15
no_error_code_interrupt_handler 16
error_code_interrupt_handler    17
no_error_code_interrupt_handler 18
no_error_code_interrupt_handler 19
no_error_code_interrupt_handler 20
no_error_code_interrupt_handler 21
no_error_code_interrupt_handler 22
no_error_code_interrupt_handler 23
no_error_code_interrupt_handler 24
no_error_code_interrupt_handler 25
no_error_code_interrupt_handler 26
no_error_code_interrupt_handler 27
no_error_code_interrupt_handler 28
no_error_code_interrupt_handler 29
error_code_interrupt_handler    30
no_error_code_interrupt_handler 31

; Create the remaining interrupt handlers
%assign i 32
%rep 256-32
    no_error_code_interrupt_handler i
    %assign i i+1
%endrep

section .data

; Define a function pointer array to our handlers
global isr_stub_table
isr_stub_table:
    %assign i 0 
    %rep    256
        dq interrupt_handler_%+i
    %assign i i+1 
%endrep
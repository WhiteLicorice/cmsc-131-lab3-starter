;
; fmt.asm - int_to_dec, integer to decimal text. Callable from C.
;
; This is your starting point: it assembles and links as-is, so the build
; works before you write any code. Right now it writes nothing and returns
; 0, which makes line numbers print empty. Your job is to replace that with
; the conversion described below.
;
; The contract, from driver.c:
;
;       char *out                   [ebp+12]
;       int n                       [ebp+8]
;
;       int int_to_dec(int n, char *out)
;
; Write the decimal text of n into out, return the number of bytes written.
; No leading zeros except for n == 0, which is "0". No sign. No terminator.
; The caller provides a buffer at least 16 bytes.
;
; Requirements from the manual:
;
;   * Divide by ten repeatedly, collecting remainders, which come out
;     backwards.
;   * The stack does the reversal. Push each remainder, then pop them into
;     the output in order.
;   * The loop must be bottom-tested so that zero emits one digit. A
;     top-tested loop emits nothing for n == 0, which is the classic bug.
;   * Clear edx before every div. The div writes its remainder there.
;   * Preserve ebx, esi, edi, and ebp. Return the byte count in eax.
;   * No calls into the C standard library.
;
; The manual's shape:
;
;       mov ebx, 10
;       xor ecx, ecx              ; digit count
;   digit_loop:
;       mov edx, 0                ; clear before div, as always
;       div ebx                   ; eax = quotient, edx = remainder
;       add dl, '0'               ; remainder to ASCII
;       push edx                  ; stack reverses them for us
;       inc ecx
;       cmp eax, 0
;       jne digit_loop
;       ; now pop ecx digits into edi in the right order
;
; (line numbers here never go negative, but int_to_dec must still handle
; n == 0 correctly, and the driver never passes negatives.)
;

; Windows C decorates the names it exports with a leading underscore and
; Linux C does not, so the same source would otherwise need two spellings of
; every entry point. -d ELF_TYPE, which the shared Makefile fragment passes
; on Linux, selects the respelling here. It's the same trick asm_io.inc
; uses for _asm_main in the bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _int_to_dec int_to_dec
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .text
        global  _int_to_dec

_int_to_dec:
        enter   0,0
        pusha

        ;
        ; TODO: convert.
        ;
        ; The shape above is the manual's. Your job is to make it write
        ; into out (which is [ebp+12]) and return the digit count in eax.
        ; The pop loop writes one byte per digit to out[0..count-1].
        ;

        popa
        mov     eax, 0
        leave
        ret

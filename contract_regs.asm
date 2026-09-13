;
; contract_regs.asm - call each routine with sentinel registers.
;
; run_tests.sh compares the tool's output with grep's, which cannot see
; whether a routine leaves ebx, esi, or edi changed. C assumes those three
; survive a call. A routine that clobbers one produces failures far from
; the cause, and on Linux the failure lands in the driver, because gcc's
; position-independent builds keep the GOT base in ebx.
;
; Each function below loads a sentinel into ebx, esi, and edi, takes a copy
; of esp, calls the routine under test, and returns a bitmask:
;
;   bit 0  ebx changed
;   bit 1  esi changed
;   bit 2  edi changed
;   bit 3  esp moved
;
; Zero means every one of them survived. This file is provided. Do not
; modify it.
;

; Windows C puts a leading underscore on every exported name. Linux C does
; not. The Makefile passes -d ELF_TYPE on Linux. This block then respells
; the names below to match. asm_io.inc does the same for _asm_main in the
; bootcamp blocks.
%ifdef ELF_TYPE
  %define _index_lines index_lines
  %define _line_matches line_matches
  %define _int_to_dec int_to_dec
  %define _parse_args parse_args
  %define _check_index_registers check_index_registers
  %define _check_matches_registers check_matches_registers
  %define _check_fmt_registers check_fmt_registers
  %define _check_parse_registers check_parse_registers
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

extern _index_lines
extern _line_matches
extern _int_to_dec
extern _parse_args

%define SENTINEL_EBX 0x11111111
%define SENTINEL_ESI 0x22222222
%define SENTINEL_EDI 0x33333333

;
; verdict - build the register bitmask in eax. edx holds the saved stack
; pointer. The macro expands in place, so it adds nothing to the stack and
; the esp comparison stays honest.
;
%macro verdict 0
        xor     eax, eax
        cmp     ebx, SENTINEL_EBX
        je      %%ebx_ok
        or      eax, 1
%%ebx_ok:
        cmp     esi, SENTINEL_ESI
        je      %%esi_ok
        or      eax, 2
%%esi_ok:
        cmp     edi, SENTINEL_EDI
        je      %%edi_ok
        or      eax, 4
%%edi_ok:
        cmp     esp, edx
        je      %%esp_ok
        or      eax, 8
%%esp_ok:
%endmacro

segment .text

; int check_index_registers(char *buf, int len, struct line *out, int max)
        global  _check_index_registers
_check_index_registers:
        enter   0,0
        push    ebx
        push    esi
        push    edi

        mov     ebx, SENTINEL_EBX
        mov     esi, SENTINEL_ESI
        mov     edi, SENTINEL_EDI

        mov     eax, esp
        push    eax
        push    dword [ebp+20]          ; max
        push    dword [ebp+16]          ; out
        push    dword [ebp+12]          ; len
        push    dword [ebp+8]           ; buf
        call    _index_lines
        add     esp, 16
        pop     edx
        verdict

        pop     edi
        pop     esi
        pop     ebx
        leave
        ret

; int check_matches_registers(char *line, int linelen, char *pat, int patlen,
;                             int fold, int whole)
        global  _check_matches_registers
_check_matches_registers:
        enter   0,0
        push    ebx
        push    esi
        push    edi

        mov     ebx, SENTINEL_EBX
        mov     esi, SENTINEL_ESI
        mov     edi, SENTINEL_EDI

        mov     eax, esp
        push    eax
        push    dword [ebp+28]          ; whole
        push    dword [ebp+24]          ; fold
        push    dword [ebp+20]          ; patlen
        push    dword [ebp+16]          ; pat
        push    dword [ebp+12]          ; linelen
        push    dword [ebp+8]           ; line
        call    _line_matches
        add     esp, 24
        pop     edx
        verdict

        pop     edi
        pop     esi
        pop     ebx
        leave
        ret

; int check_fmt_registers(int n, char *out)
        global  _check_fmt_registers
_check_fmt_registers:
        enter   0,0
        push    ebx
        push    esi
        push    edi

        mov     ebx, SENTINEL_EBX
        mov     esi, SENTINEL_ESI
        mov     edi, SENTINEL_EDI

        mov     eax, esp
        push    eax
        push    dword [ebp+12]          ; out
        push    dword [ebp+8]           ; n
        call    _int_to_dec
        add     esp, 8
        pop     edx
        verdict

        pop     edi
        pop     esi
        pop     ebx
        leave
        ret

; int check_parse_registers(int argc, char **argv, struct args *out)
        global  _check_parse_registers
_check_parse_registers:
        enter   0,0
        push    ebx
        push    esi
        push    edi

        mov     ebx, SENTINEL_EBX
        mov     esi, SENTINEL_ESI
        mov     edi, SENTINEL_EDI

        mov     eax, esp
        push    eax
        push    dword [ebp+16]          ; out
        push    dword [ebp+12]          ; argv
        push    dword [ebp+8]           ; argc
        call    _parse_args
        add     esp, 12
        pop     edx
        verdict

        pop     edi
        pop     esi
        pop     ebx
        leave
        ret

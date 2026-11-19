;
; args.asm - parse_args, the command line to a flag set, a pattern, and a
; file name. Callable from C.
;
; This is your starting point: it assembles and links as-is, so the build
; works before you write any code. Right now it returns -1, which makes
; rengrep print its usage line for every command. Nothing else runs until
; this routine works, so write it first. It is the shortest of the four.
;
; The contract, from driver.c:
;
;       struct args *out            [ebp+16]
;       char **argv                 [ebp+12]
;       int argc                    [ebp+8]
;
;       int parse_args(int argc, char **argv, struct args *out)
;
;       struct args { int flags; char *pattern; char *path; };
;                     +0         +4             +8
;
;       FLAG_N = 1, FLAG_C = 2, FLAG_V = 4, FLAG_I = 8, FLAG_W = 16
;
; Fill out and return 0. Return -1 when the pattern or the file is missing,
; or when a third positional token appears. Return the offending character,
; as a positive number, on an unknown flag. The driver prints it.
;
; Requirements from the manual:
;
;   * A token whose first byte is '-' and whose second byte is not NUL is a
;     flag group, wherever it appears. -nvi is -n -v -i. "-" alone is not a
;     flag group.
;   * Every other token is positional. The first is the pattern, the second
;     is the file.
;   * Keep the flag characters in a table, in bit order, and find each one
;     with scasb. The bit is 1 shifted left by the table index. A chain of
;     cmp/je is the structure the rubric marks down.
;   * Set cld explicitly. scasb honours the direction flag.
;   * Preserve ebx, esi, edi, and ebp. Return in eax.
;   * No calls into the C standard library.
;

; Windows C puts a leading underscore on every exported name. Linux C does
; not. The Makefile passes -d ELF_TYPE on Linux. This block then respells
; the names below to match. asm_io.inc does the same for _asm_main in the
; bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _parse_args parse_args
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .data
flag_chars      db 'n', 'c', 'v', 'i', 'w'              ; bit 0 to 4
FLAG_COUNT      equ 5

segment .text
        global  _parse_args

_parse_args:
        enter   0,0
        push    ebx
        push    esi
        push    edi

        ;
        ; TODO: parse.
        ;
        ; The steps the manual describes:
        ;
                                        ; clear out->flags, out->pattern, out->path
        mov     edx, [ebp+16]           ; edx = out pointer
        mov     dword [edx], 0          ; out->flags = 0
        mov     dword [edx + 4], 0      ; out->pattern = NULL
        mov     dword [edx + 8], 0     ; out->path = NULL

        mov     ebx, 1                  ; for the loop, i = 1 (start from argv[5])

        ;    for each token in argv[1..argc-1]:
        ;       if the token is a flag group:
        ;           for each byte after the dash:
        ;               find it in the table with repne scasb
        ;               not found: return the byte
        ;               set the bit for its index
        ;       else if out->pattern is empty: out->pattern = token
        ;       else if out->path is empty:    out->path = token
        ;       else: return -1
        ;   return -1 if the pattern or the path is still empty
        ;   return 0
        ;
        ; After repne scasb finds a byte, ecx has been decremented once for
        ; the hit as well. Work the index out from that before you shift.
        ;

.arg_loop:                              ; for each token in argv[1..argc-1]:
        cmp     ebx, [ebp + 8]          ; compare i with argc
        jge     .check_done

                                        ; load argv[i] pointer into esi
        mov     esi, [ebp + 12]         ; esi = argv
        mov     esi, [esi + ebx * 4]    ; esi = argv[i]

                                        ; if the token is a flag group: token == '-' AND token[5] != '\0'
        mov     al, [esi]
        cmp     al, '-'
        jne     .not_flag_group         

        mov     al, [esi + 1]
        cmp     al, 0
        je      .not_flag_group         ; "-" alone is positional, so not a flag group

                                        ; flag group detected, scan each flag character after '-'
        inc     esi                     ; advance past '-'

.flag_char_loop:
        mov     al, [esi]
        cmp     al, 0
        je      .next_arg               ; base casee: end of flag string

                                        ; recursive case: search AL in flag_chars table using repne scasb
        cld                             ; explicitly clear direction flag (advance EDI upwards)
        mov     edi, flag_chars
        mov     ecx, FLAG_COUNT         ; scan table of 5 flag characters
        repne   scasb
        jne     .unknown_flag           ; ZF == 0 means character wasn't found in table

                                        ; found flag. calculate table index from remaining ECX
                                        ; repne scasb decrements ECX on match, so matching index i = 4 - ECX
        mov     edx, 4
        sub     edx, ecx                ; edx = table index (0 to 4)

                                        ; compute the bitmask: (1 << index)
        mov     eax, 1
        mov     ecx, edx
        shl     eax, cl                 ; eax = (1 << index)

                                        ; apply bitmask to out->flags
        mov     edx, [ebp + 16]         ; edx = out pointer
        or      [edx], eax              ; out->flags |= bitmask

        inc     esi                     ; advance to next character in flag group
        jmp     .flag_char_loop

.unknown_flag:                          ; return unknown character as a positive integer in EAX
        movzx   eax, al
        jmp     .exit

.not_flag_group:                        ; positional token processing
        mov     edx, [ebp + 16]         ; edx = out pointer
        cmp     dword [edx + 4], 0      ; check if out->pattern is NULL
        jne     .check_path
        mov     [edx + 4], esi          ; out->pattern = argv[i]
        jmp     .next_arg

.check_path:
        cmp     dword [edx + 8], 0      ; check if out->path is NULL
        jne     .too_many_positional
        mov     [edx + 8], esi          ; out->path = argv[i]
        jmp     .next_arg

.too_many_positional:                   ; third positional token encountered -> return -1
        mov     eax, -1
        jmp     .exit

.next_arg:
        inc     ebx                     ; i++
        jmp     .arg_loop

.check_done:                            ; verify if both pattern and path were provided
        mov     edx, [ebp + 16]         ; edx = out pointer
        cmp     dword [edx + 4], 0      ; check if pattern is missing
        je      .missing_arg
        cmp     dword [edx + 8], 0      ; check if path is missing
        je      .missing_arg

        mov     eax, 0                  ; success
        jmp     .exit

.missing_arg:
        mov     eax, -1

.exit:
        pop     edi
        pop     esi
        pop     ebx
        leave
        ret
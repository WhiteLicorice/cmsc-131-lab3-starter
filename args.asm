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

segment .text
        global  _parse_args

_parse_args:
        enter   0,0
        pusha

        ;
        ; TODO: parse.
        ;
        ; The steps the manual describes:
        ;
        ;   clear out->flags, out->pattern, out->path
        ;   for each token in argv[1..argc-1]:
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

        popa
        mov     eax, -1
        leave
        ret

;
; match.asm - line_matches, substring search with case folding and word
; boundaries. Callable from C.
;
; This is your starting point: it assembles and links as-is, so the build
; works before you write any code. Right now it always returns 0 (no
; match), which makes rengrep print nothing. Your job is to replace that
; with the search described below.
;
; The contract, from driver.c:
;
;       int line_matches(char *line, int linelen,
;                        char *pat, int patlen,
;                        int fold, int whole)
;
; so on the stack after a cdecl call:
;       [ebp+8]  = line
;       [ebp+12] = linelen
;       [ebp+16] = pat
;       [ebp+20] = patlen
;       [ebp+24] = fold
;       [ebp+28] = whole
;
; Return 1 in eax if pat occurs in line, 0 otherwise.
;
; Requirements from the manual:
;
;   * Use the x86 string instructions for the inner comparison. repe cmpsb
;     at minimum. A mov/cmp loop does not satisfy the activity.
;   * fold: compare case-insensitively WITHOUT modifying the buffer. Fold
;     in a register at comparison time. This path cannot use repe cmpsb
;     (it compares raw bytes), so a folding loop for that path is expected.
;   * whole: a match counts only when the byte before the match and the
;     byte after it are both non-word characters or absent. A failed
;     word-boundary check does NOT end the search. Keep scanning.
;   * The search bound is linelen - patlen, not linelen. Compare it with a
;     signed jump. When patlen is larger than linelen the difference is
;     negative. An unsigned jump reads it as enormous, and the scan runs
;     off the line into memory the process does not own and faults. This
;     is the Block 5 lesson.
;   * An empty pattern matches at every position, as it does in grep. A
;     rep prefix with ecx = 0 executes nothing and leaves the flags as they
;     were, so set ZF yourself before repe cmpsb. A bottom-tested folding
;     loop needs its own guard.
;   * Set cld explicitly. The direction flag persists across calls and
;     assuming it is clear is how a routine that works alone breaks when
;     called after something else.
;   * Preserve ebx, esi, edi, and ebp. Return in eax.
;   * No calls into the C standard library.
;
; Word characters are A-Z, a-z, 0-9, and _.
;

; Windows C puts a leading underscore on every exported name. Linux C does
; not. The Makefile passes -d ELF_TYPE on Linux. This block then respells
; the names below to match. asm_io.inc does the same for _asm_main in the
; bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _line_matches line_matches
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .text
        global  _line_matches

_line_matches:
        enter   0,0
        pusha

        ;
        ; TODO: search.
        ;
        ; The loop the manual describes:
        ;
        ;   for start = 0 to linelen - patlen:
        ;       compare patlen bytes at line+start against pat
        ;       if equal: check word boundaries if whole
        ;                 return 1 if they pass
        ;   return 0
        ;
        ; The fast path (fold == 0) uses repe cmpsb. The folding path
        ; compares a byte at a time, folding both sides in a register.
        ; Both need cld first.
        ;

        popa
        mov     eax, 0
        leave
        ret

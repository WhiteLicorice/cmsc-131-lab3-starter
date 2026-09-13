;
; lines.asm - index_lines, a file buffer to a list of lines. Callable from C.
;
; This is your starting point: it assembles and links as-is, so the build
; works before you write any code. Right now it returns 0 lines, which makes
; rengrep print nothing. Your job is to replace that with the index
; described below.
;
; The contract, from driver.c:
;
;       int max                     [ebp+20]
;       struct line *out            [ebp+16]
;       int len                     [ebp+12]
;       char *buf                   [ebp+8]
;
;       struct line { int offset; int length; };
;
; Fill out with one entry per line and return the number of entries
; written. The capacity is a hard limit: stop when out is full and return
; what you wrote. max == 0 writes nothing and returns 0.
;
; Requirements from the manual:
;
;   * A line's length EXCLUDES its trailing newline.
;   * A file with no trailing newline still indexes its last line.
;   * An empty file (len == 0) has zero lines.
;   * Preserve ebx, esi, edi, and ebp. Return the count in eax.
;   * No calls into the C standard library.
;
; The manual calls this "buffer to line index". Walk the buffer, find the
; newlines yourself, and record where each line starts and how long it
; is. The driver reads the file into one buffer and uses your index to
; treat it as a list of lines.
;

; Windows C puts a leading underscore on every exported name. Linux C does
; not. The Makefile passes -d ELF_TYPE on Linux. This block then respells
; the names below to match. asm_io.inc does the same for _asm_main in the
; bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _index_lines index_lines
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .text
        global  _index_lines

_index_lines:
        enter   0,0
        pusha

        ;
        ; TODO: build the index.
        ;
        ; Walk buf looking for newline bytes (0x0A). Each newline closes
        ; the line that ended before it. The tricky cases:
        ;
        ;   * If the file does not end with a newline, the bytes after the
        ;     last newline still form a line. The last entry has no
        ;     newline and must still be indexed.
        ;   * An empty file produces zero lines, not one empty line.
        ;
        ; Watch the max capacity too. out holds max entries. A file of len
        ; bytes has at most len lines, when every byte is a newline, so the
        ; caller's max is always big enough. The routine must still stop
        ; writing at max.
        ;

        popa
        mov     eax, 0
        leave
        ret

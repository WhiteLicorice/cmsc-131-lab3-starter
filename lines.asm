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
        enter   4,0     ; one local: the return value 
        pusha           ; saves ebx, esi, edi

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

        ; --------------------------------------------------------------------------
        ; For tracking, registers inside the routine:
        ;
        ;       al      the newline byte, 0x0A (scasb compares against it)
        ;       ebx     number of entries written so far
        ;       ecx     bytes of the buffer still to scan (repne counter)
        ;       edx     offset where the current line starts
        ;       esi     pointer to the next free struct line in out
        ;       edi     scan cursor into buf (scasb advances it)
        ;       [ebp-4] the return value, parked here because popa restores eax
        ;
        ; stack frame:
        ;       The contract, from driver.c:
        ;
        ;       int max                     [ebp+20]
        ;       struct line *out            [ebp+16]
        ;       int len                     [ebp+12]
        ;       char *buf                   [ebp+8]
        ;
        ;       struct line { int offset; int length; };
        ; --------------------------------------------------------------------------------------

        cld 

        mov     edi, [ebp + 8]          ; scan cursor = buf
        mov     ecx, [ebp + 12]         ; bytes left = len (counter for scasb)
        mov     esi, [ebp + 16]         ; next free struct line slot = out 
        xor     ebx, ebx                ; entries written = 0
        xor     edx, edx                ; current line starts at offset = 0

.scan:
        ; guard against empty scan
        test    ecx, ecx                ; repne does nothing with ecx = 0
        jz      .tail                   ; leaves ZF stale, so stop here instead
        
        ; search for the newline
        repne scasb                     ; advance edi past to the next newline, or run out

        ; scasb ->      cmp al, [edi]
        ;               inc edi

        jne     .tail                   ; ZF clear: ecx hit 0 with no newline found

        ; a newline was found. edi now points one past it
        cmp     ebx, [ebp + 20]         ; check capacity
        jge     .done                   ; if no. of entries >= max, done

        mov     [esi], edx              ; write the offset
        mov     eax, edi                ; eax <- edi (scan cursor)
        sub     eax, [ebp + 8]          ; scan cursor - buf (offset just past the newline)
        dec     eax                     ; offset of the newline itself
        sub     eax, edx                ; length = newline offset - line start
        mov     [esi + 4], eax          ; write the length

        ; prepare for the next line
        mov     edx, edi
        sub     edx, [ebp + 8]          ; new line start = offset of the next byte after the newline
        add     esi, 8                  ; sizeof(struct line), forward 8 bytes
        inc     ebx                     ; goes up for the entry just written
        mov     al, 0Ah                 ; reload eax
        jmp     .scan                   ; goes back to scan

.tail
        ; no more newlines. if bytes remain after the last one,
        ; they are a final line with no trailing newline. 
        ; when the file ends with a newline, or is empty, edx == edi - buf (len)
        ; and nothing is added

        mov     eax, [ebp + 12]         ; eax = len
        sub     eax, edx                ; len - last line start offset (bytes remaining)
        jle     .done                   ; if <= 0, no more lines to add

        cmp     ebx, [ebp + 20]         ; check capacity
        jge     .done                   ; if no. of entries >= max (out is full), done        
        mov     [esi], edx              ; write the offset of the last line
        mov     [esi + 4], eax          ; write the length of the last line
        inc     ebx                     ; goes up for the entry just written

.done
        mov     [ebp - 4], ebx          ; return value = number of entries written
                                        ; save the count in that local

        popa
        mov     eax, 0
        
        mov     eax, [ebp - 4]          ; return value = number of entries written
                                        ; now load the count to eax                                        

        leave
        ret

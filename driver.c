/*
 * rengrep - a working subset of grep. Provided to the group. Do not modify.
 *
 * This file reads the file, parses flags, builds the line index, and writes
 * output. Everything that touches a byte of the text is your assembly: the
 * three routines below. Your defense will use this copy, so what it prints
 * and the structs it passes are the contract. Read this file before writing
 * assembly.
 *
 * Correctness is defined by agreement with the installed grep, byte for
 * byte. run_tests.sh runs both and diffs them. There is no expected-output
 * file to maintain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdecl.h"

/* ------------------------------------------------------------------ */
/* The routines you implement.                                        */
/* ------------------------------------------------------------------ */

/*
 * Build an index of line starts and lengths.
 *
 *   buf  - the whole file, including newlines
 *   len  - the file length in bytes
 *   out  - caller's array of struct line
 *   max  - capacity of out
 *
 * Fills out with one struct line per line, in file order, and returns the
 * number of lines. A line's length EXCLUDES its trailing newline. The last
 * line of a file with no trailing newline is still a line. An empty file
 * has zero lines.
 */
struct line {
    int offset;
    int length;
};
int PRE_CDECL index_lines(char *buf, int len, struct line *out, int max) POST_CDECL;

/*
 * Return 1 if pat occurs in the line, 0 otherwise.
 *
 *   line, linelen - the line, with no trailing newline
 *   pat, patlen   - the pattern
 *   fold          - nonzero means compare case-insensitively
 *   whole         - nonzero means require a whole-word match
 *
 * A match is a whole word when the character before it and the character
 * after it are both non-word characters or absent. Word characters are
 * A-Z, a-z, 0-9, and _. A pattern longer than the line cannot match.
 */
int PRE_CDECL line_matches(char *line, int linelen,
                           char *pat, int patlen,
                           int fold, int whole) POST_CDECL;

/*
 * Write the decimal text of n into out. Return the number of bytes written.
 * No leading zeros (except n == 0, which is "0"), no sign, no terminator.
 */
int PRE_CDECL int_to_dec(int n, char *out) POST_CDECL;

/* ------------------------------------------------------------------ */
/* The rest is driver. Read it for the output format, not to change.  */
/* ------------------------------------------------------------------ */

static int is_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static char *read_file(const char *path, int *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "rengrep: cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n > 0 ? (size_t)n : 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "rengrep: out of memory\n");
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    *len_out = (int)got;
    return buf;
}

int main(int argc, char **argv)
{
    int fold = 0, whole = 0, num = 0, count = 0, invert = 0;
    const char *pat = NULL, *path = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] != '\0' && pat == NULL) {
            /* Combined short flags: -nvi is -n -v -i. */
            for (const char *p = a + 1; *p; p++) {
                switch (*p) {
                case 'n': num = 1; break;
                case 'c': count = 1; break;
                case 'v': invert = 1; break;
                case 'i': fold = 1; break;
                case 'w': whole = 1; break;
                default:
                    fprintf(stderr, "rengrep: unknown flag -%c\n", *p);
                    return 2;
                }
            }
        } else if (pat == NULL) {
            pat = a;
        } else {
            path = a;
        }
    }

    if (!pat || !path) {
        fprintf(stderr, "usage: rengrep [flags] PATTERN FILE\n");
        return 2;
    }

    int len;
    char *buf = read_file(path, &len);
    if (!buf)
        return 1;

    int max_lines = len + 1;
    struct line *lines = malloc(sizeof(struct line) * (size_t)max_lines);
    if (!lines) {
        fprintf(stderr, "rengrep: out of memory\n");
        free(buf);
        return 1;
    }
    int nlines = index_lines(buf, len, lines, max_lines);

    int patlen = (int)strlen(pat);

    if (count) {
        int matched = 0;
        for (i = 0; i < nlines; i++) {
            int m = line_matches(buf + lines[i].offset, lines[i].length,
                                 (char *)pat, patlen, fold, whole);
            if (invert ? !m : m)
                matched++;
        }
        char numbuf[16];
        int n = int_to_dec(matched, numbuf);
        fwrite(numbuf, 1, (size_t)n, stdout);
        fputc('\n', stdout);
        free(lines);
        free(buf);
        return 0;
    }

    for (i = 0; i < nlines; i++) {
        int m = line_matches(buf + lines[i].offset, lines[i].length,
                             (char *)pat, patlen, fold, whole);
        if (invert ? !m : m) {
            if (num) {
                char numbuf[16];
                int n = int_to_dec(i + 1, numbuf);
                fwrite(numbuf, 1, (size_t)n, stdout);
                fputc(':', stdout);
            }
            fwrite(buf + lines[i].offset, 1, (size_t)lines[i].length, stdout);
            fputc('\n', stdout);
        }
    }

    free(lines);
    free(buf);
    return 0;
}

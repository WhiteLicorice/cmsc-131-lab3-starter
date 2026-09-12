/*
 * rengrep - a working subset of grep. Provided to the group. Do not modify.
 *
 * This file reads the file, calls your parser, builds the line index, and
 * writes output. Everything that touches a byte of the text or of argv is
 * your assembly: the four routines below. Your defense will use this copy,
 * so what it prints and the structs it passes are the contract. Read this
 * file before writing assembly.
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
 * A-Z, a-z, 0-9, and _. A pattern longer than the line cannot match. An
 * empty pattern matches at every position, as it does in grep.
 */
int PRE_CDECL line_matches(char *line, int linelen,
                           char *pat, int patlen,
                           int fold, int whole) POST_CDECL;

/*
 * Write the decimal text of n into out. Return the number of bytes written.
 * No leading zeros (except n == 0, which is "0"), no sign, no terminator.
 */
int PRE_CDECL int_to_dec(int n, char *out) POST_CDECL;

/*
 * Parse argv into out.
 *
 *   argc, argv - as main received them. argv[0] is the program name.
 *   out        - the parsed result
 *
 * A token whose first byte is '-' and whose second byte is not NUL is a
 * flag group, wherever it appears. -nvi is -n -v -i. Every other token is
 * positional. The first positional is the pattern, the second is the file.
 *
 * out->flags is the OR of the FLAG_ bits below. Return 0 on success.
 * Return -1 when the pattern or the file is missing, or a third positional
 * appears. Return the offending character, as a positive int, on an
 * unknown flag.
 */
#define FLAG_N 1
#define FLAG_C 2
#define FLAG_V 4
#define FLAG_I 8
#define FLAG_W 16
struct args {
    int flags;
    char *pattern;
    char *path;
};
int PRE_CDECL parse_args(int argc, char **argv, struct args *out) POST_CDECL;

/* ------------------------------------------------------------------ */
/* The rest is driver. Read it for the output format, not to change.  */
/* ------------------------------------------------------------------ */

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
    struct args a;
    int i;

    int r = parse_args(argc, argv, &a);
    if (r > 0) {
        fprintf(stderr, "rengrep: unknown flag -%c\n", r);
        return 2;
    }
    if (r < 0) {
        fprintf(stderr, "usage: rengrep [flags] PATTERN FILE\n");
        return 2;
    }

    int num    = (a.flags & FLAG_N) != 0;
    int count  = (a.flags & FLAG_C) != 0;
    int invert = (a.flags & FLAG_V) != 0;
    int fold   = (a.flags & FLAG_I) != 0;
    int whole  = (a.flags & FLAG_W) != 0;
    const char *pat = a.pattern, *path = a.path;

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

/*
 * contract_test.c - the checks the grep comparison cannot make.
 *
 * run_tests.sh compares rengrep's output with the installed grep, byte for
 * byte, across every flag subset, pattern, and test file. That is the
 * correctness oracle, and it only sees what the driver prints. Four things
 * stay invisible to it:
 *
 *   1. The capacity argument of index_lines. The driver always passes
 *      len + 1, so a routine that ignores max never fails in the tool.
 *   2. The word-boundary and folding rules on inputs no test file
 *      contains, such as a pattern longer than the line.
 *   3. int_to_dec on the values no file reaches, and whether it writes
 *      past the digits it counted.
 *   4. The cdecl contract. Nothing in the output shows a clobbered ebx.
 *
 * This program calls the four routines directly and checks each one on its
 * own terms. It exits 0 when every check passes and 1 otherwise. This file
 * is provided, along with contract_regs.asm, which makes check 4 possible.
 * Do not modify either one.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdecl.h"

struct line {
    int offset;
    int length;
};

struct args {
    int flags;
    char *pattern;
    char *path;
};

#define FLAG_N 1
#define FLAG_C 2
#define FLAG_V 4
#define FLAG_I 8
#define FLAG_W 16

int PRE_CDECL index_lines(char *buf, int len, struct line *out, int max) POST_CDECL;
int PRE_CDECL line_matches(char *line, int linelen,
                           char *pat, int patlen,
                           int fold, int whole) POST_CDECL;
int PRE_CDECL int_to_dec(int n, char *out) POST_CDECL;
int PRE_CDECL parse_args(int argc, char **argv, struct args *out) POST_CDECL;

int PRE_CDECL check_index_registers(char *buf, int len, struct line *out, int max) POST_CDECL;
int PRE_CDECL check_matches_registers(char *line, int linelen, char *pat, int patlen, int fold, int whole) POST_CDECL;
int PRE_CDECL check_fmt_registers(int n, char *out) POST_CDECL;
int PRE_CDECL check_parse_registers(int argc, char **argv, struct args *out) POST_CDECL;

static int failures = 0;

static void ok(const char *name)
{
    printf("ok    %s\n", name);
}

static void fail(const char *name, const char *detail)
{
    printf("FAIL  %s: %s\n", name, detail);
    failures++;
}

/* --- index_lines ---------------------------------------------------- */

/* Run one index_lines call and compare. Report a failure and return 0, or
   return 1. out holds n entries. Entries past the wanted count must stay
   at the sentinel. */
static int run_lines(const char *name, const char *how, char *buf, int len, int max,
                     int want_count, const struct line *want, struct line *out, int n)
{
    int i;

    /* A sentinel pattern, so a write past the capacity is visible. */
    for (i = 0; i < n; i++) {
        out[i].offset = 0x7F7F7F7F;
        out[i].length = 0x7F7F7F7F;
    }

    int got = index_lines(buf, len, out, max);

    if (got != want_count) {
        char detail[128];
        snprintf(detail, sizeof detail, "%s: returned %d, wanted %d", how, got, want_count);
        fail(name, detail);
        return 0;
    }
    for (i = 0; i < want_count && i < n; i++) {
        if (out[i].offset != want[i].offset || out[i].length != want[i].length) {
            char detail[160];
            snprintf(detail, sizeof detail,
                     "%s: entry %d is {%d,%d}, wanted {%d,%d}",
                     how, i, out[i].offset, out[i].length, want[i].offset, want[i].length);
            fail(name, detail);
            return 0;
        }
    }
    for (i = want_count; i < n; i++) {
        if (out[i].offset != 0x7F7F7F7F || out[i].length != 0x7F7F7F7F) {
            char detail[160];
            snprintf(detail, sizeof detail,
                     "%s: wrote entry %d past the capacity of %d", how, i, max);
            fail(name, detail);
            return 0;
        }
    }
    return 1;
}

/* Each small case also runs in wide forms. When the capacity is above the
   wanted count, the case runs again with a capacity of 65536, and again
   padded with 'x' to a length of exactly 65536. The case also runs after a
   first line of 65536 bytes. len and max then pass the low word on every
   path. */
static void check_lines(const char *name, char *buf, int len, int max,
                        int want_count, const struct line *want)
{
    static struct line wide_out[65537];
    static char wide_buf[65537 + 4096];
    struct line out[8];
    struct line wide_want[9];
    int i;

    if (!run_lines(name, "as given", buf, len, max, want_count, want, out, 8))
        return;
    if (len <= 4096 && want_count <= 8) {
        if (want_count > 0 && max > want_count
            && !run_lines(name, "capacity 65536", buf, len, 65536, want_count, want, wide_out, 16))
            return;
        if (max > want_count) {
            int pad = 65536 - len;
            int n = want_count;
            memcpy(wide_buf, buf, len);
            memset(wide_buf + len, 'x', pad);
            for (i = 0; i < want_count; i++)
                wide_want[i] = want[i];
            if (len > 0 && buf[len - 1] != '\n') {
                wide_want[n - 1].length += pad;
            } else {
                wide_want[n].offset = len;
                wide_want[n].length = pad;
                n++;
            }
            if (!run_lines(name, "padded to 65536 bytes", wide_buf, 65536, max + 1,
                           n, wide_want, wide_out, 16))
                return;
        }
        memset(wide_buf, 'x', 65536);
        wide_buf[65536] = '\n';
        memcpy(wide_buf + 65537, buf, len);
        wide_want[0].offset = 0;
        wide_want[0].length = 65536;
        for (i = 0; i < want_count; i++) {
            wide_want[i + 1].offset = want[i].offset + 65537;
            wide_want[i + 1].length = want[i].length;
        }
        if (!run_lines(name, "after a 65536-byte line", wide_buf, len + 65537, max + 1,
                       want_count + 1, wide_want, wide_out, 16))
            return;
    }
    ok(name);
}

static void lines_checks(void)
{
    static const struct line two[2] = { {0, 1}, {2, 1} };
    static const struct line trailing[2] = { {0, 1}, {2, 1} };
    static const struct line blank[1] = { {0, 0} };
    static const struct line one[1] = { {0, 3} };
    static const struct line first[1] = { {0, 1} };
    static const struct line zero[1] = { {0, 0} };

    check_lines("index_lines empty buffer", "", 0, 8, 0, NULL);
    check_lines("index_lines two lines", "a\nb\n", 4, 8, 2, two);
    check_lines("index_lines missing final newline", "a\nb", 3, 8, 2, trailing);
    check_lines("index_lines one blank line", "\n", 1, 8, 1, blank);
    check_lines("index_lines no newline at all", "abc", 3, 8, 1, one);
    check_lines("index_lines capacity zero", "a\nb\n", 4, 0, 0, NULL);
    check_lines("index_lines capacity one on a final unterminated line", "a\nb", 3, 1, 1, first);

    /* The canary: capacity two, three lines in the buffer. */
    {
        struct line out[8];
        int i;
        for (i = 0; i < 8; i++) {
            out[i].offset = 0x7F7F7F7F;
            out[i].length = 0x7F7F7F7F;
        }
        int got = index_lines("a\nb\nc\n", 6, out, 2);
        if (got != 2) {
            char detail[96];
            snprintf(detail, sizeof detail, "capacity 2 returned %d, wanted 2", got);
            fail("index_lines canary (capacity two)", detail);
        } else if (out[0].offset != 0 || out[0].length != 1
                   || out[1].offset != 2 || out[1].length != 1) {
            fail("index_lines canary (capacity two)", "the two entries are wrong");
        } else if (out[2].offset != 0x7F7F7F7F || out[2].length != 0x7F7F7F7F) {
            fail("index_lines canary (capacity two)", "wrote past the capacity");
        } else {
            ok("index_lines canary (capacity two)");
        }
    }

    check_lines("index_lines blank line is one entry", "x\n\ny\n", 5, 8, 3,
                (const struct line[]) { {0, 1}, {2, 0}, {3, 1} });

    /* len and max are full ints. The routine reads len bytes and stops there. */
    check_lines("index_lines reads len bytes, not up to a NUL", "abcEXTRA", 3, 8, 1, one);
    {
        static struct line roomy[65536];
        int got = index_lines("abc", 3, roomy, 65536);
        if (got == 1 && roomy[0].offset == 0 && roomy[0].length == 3)
            ok("index_lines capacity past the low word");
        else
            fail("index_lines capacity past the low word", "capacity 65536 did not give one entry");
    }
    {
        static char big[65537];
        static const struct line want[1] = { {0, 65536} };
        memset(big, 'x', 65536);
        check_lines("index_lines length past the low word", big, 65536, 8, 1, want);
    }
}

/* --- line_matches --------------------------------------------------- */

/* Run one line_matches call and compare. Report a failure and return 0, or
   return 1. */
static int match_once(const char *name, const char *how, char *line, int linelen,
                      char *pat, int patlen, int fold, int whole, int want)
{
    int got = line_matches(line, linelen, pat, patlen, fold, whole);
    if (got == want)
        return 1;
    char detail[128];
    snprintf(detail, sizeof detail, "%s: returned %d, wanted %d", how, got, want);
    fail(name, detail);
    return 0;
}

/* Each case also runs in wide forms. A nonzero flag runs again as each
   one-bit value and as -1. When the pattern has no space, the case runs
   again after 65536 leading spaces. A space is not a word character, so the
   result stays the same. */
static void match(const char *name, char *line, int linelen, char *pat, int patlen,
                  int fold, int whole, int want)
{
    static char wide[65536 + 4096];
    char how[48];
    int k;

    if (!match_once(name, "as given", line, linelen, pat, patlen, fold, whole, want))
        return;
    for (k = 0; k <= 32; k++) {
        int v = k < 32 ? (int)(1u << k) : -1;
        snprintf(how, sizeof how, "fold %d", v);
        if (fold && !match_once(name, how, line, linelen, pat, patlen, v, whole, want))
            return;
        snprintf(how, sizeof how, "whole %d", v);
        if (whole && !match_once(name, how, line, linelen, pat, patlen, fold, v, want))
            return;
    }
    if (patlen > 0 && linelen <= 4096 && memchr(pat, ' ', patlen) == NULL) {
        memset(wide, ' ', 65536);
        memcpy(wide + 65536, line, linelen);
        if (!match_once(name, "after 65536 spaces", wide, 65536 + linelen,
                        pat, patlen, fold, whole, want))
            return;
    }
    ok(name);
}

static void matches_checks(void)
{
    char line[] = "the cat sat";

    match("line_matches substring", line, 11, "cat", 3, 0, 0, 1);
    match("line_matches absent", line, 11, "dog", 3, 0, 0, 0);
    match("line_matches pattern longer than the line", "cat", 3, "cats", 4, 0, 0, 0);
    match("line_matches pattern longer, folding path", "cat", 3, "cats", 4, 1, 0, 0);
    match("line_matches empty pattern", "abc", 3, "", 0, 0, 0, 1);
    match("line_matches empty pattern on an empty line", "", 0, "", 0, 0, 0, 1);
    match("line_matches empty pattern with -w on an empty line", "", 0, "", 0, 0, 1, 1);
    match("line_matches empty pattern with -w on a line", "a b", 3, "", 0, 0, 1, 0);
    match("line_matches folding", "The Cat Ran", 11, "cat", 3, 1, 0, 1);
    match("line_matches without folding", "The Cat Ran", 11, "cat", 3, 0, 0, 0);
    match("line_matches nonzero fold other than one", "CAT", 3, "cat", 3, 2, 0, 1);
    /* '[' and '{' differ only in bit 5. Folding changes letters only. */
    match("line_matches folding leaves a bracket in the line", "[", 1, "{", 1, 1, 0, 0);
    match("line_matches folding leaves a bracket in the pattern", "{", 1, "[", 1, 1, 0, 0);
    match("line_matches whole word inside a word", "concatenate", 11, "cat", 3, 0, 1, 0);
    match("line_matches whole word standalone", line, 11, "cat", 3, 0, 1, 1);
    match("line_matches whole line", "cat", 3, "cat", 3, 0, 1, 1);
    match("line_matches word character before", "xcat", 4, "cat", 3, 0, 1, 0);
    match("line_matches nonzero whole other than one", "xcat", 4, "cat", 3, 0, 2, 0);
    /* Any nonzero int sets a flag, also when its low byte is zero. */
    match("line_matches fold flag of 256", "CAT", 3, "cat", 3, 256, 0, 1);
    match("line_matches fold flag of 65536", "CAT", 3, "cat", 3, 65536, 0, 1);
    match("line_matches fold flag of -1", "CAT", 3, "cat", 3, -1, 0, 1);
    match("line_matches whole flag of 256", "xcat", 4, "cat", 3, 0, 256, 0);
    match("line_matches whole flag of 65536", "xcat", 4, "cat", 3, 0, 65536, 0);
    match("line_matches whole flag of -1", "xcat", 4, "cat", 3, 0, -1, 0);
    match("line_matches fold flag of 1 << 24", "CAT", 3, "cat", 3, 1 << 24, 0, 1);
    match("line_matches whole flag of 1 << 24", "xcat", 4, "cat", 3, 0, 1 << 24, 0);
    /* The lengths are full ints. patlen gives the pattern bytes to compare. */
    match("line_matches compares patlen bytes, not up to a NUL", "cat", 3, "cats", 3, 0, 0, 1);
    match("line_matches folding compares patlen bytes, not up to a NUL", "CAT", 3, "cats", 3, 1, 0, 1);
    /* The boundary check uses patlen too, on both sides of the match. */
    match("line_matches whole word before, patlen not up to a NUL", "xcat", 4, "cats", 3, 0, 1, 0);
    match("line_matches folding whole word before, patlen not up to a NUL", "XCAT", 4, "cats", 3, 1, 1, 0);
    match("line_matches whole word after, patlen not up to a NUL", "cat_", 4, "cats", 3, 0, 1, 0);
    match("line_matches whole word match, patlen not up to a NUL", "cat x", 5, "cats", 3, 0, 1, 1);
    match("line_matches folding whole word after, patlen not up to a NUL", "CAT_", 4, "cats", 3, 1, 1, 0);
    match("line_matches folding whole word match, patlen not up to a NUL", "CAT x", 5, "cats", 3, 1, 1, 1);
    /* linelen ends the line, also when its storage holds more bytes. */
    match("line_matches whole word at linelen, not at a NUL", "catX", 3, "cat", 3, 0, 1, 1);
    match("line_matches folding whole word at linelen, not at a NUL", "CATX", 3, "cat", 3, 1, 1, 1);
    {
        static char big[65539];
        memset(big, 'x', sizeof big);
        memcpy(big + 65533, "cat", 3);
        big[65536] = 0;
        match("line_matches line past the low word", big, 65536, "cat", 3, 0, 0, 1);
        match("line_matches folding line past the low word", big, 65536, "CAT", 3, 1, 0, 1);
        big[0] = 'a';
        match("line_matches pattern past the low word", "a", 1, big, 65536, 0, 0, 0);
        match("line_matches folding pattern past the low word", "a", 1, big, 65536, 1, 0, 0);
    }
    /* These cases make a whole match of a 65536-byte pattern. The boundary
       check reads the byte after the pattern, at offset 65536. */
    {
        static char qline[65539];
        static char qpat[65537];
        memset(qline, 'q', 65536);
        qline[65536] = ' ';
        qline[65537] = 'z';
        memset(qpat, 'q', 65536);
        match("line_matches whole pattern past the low word", qline, 65538, qpat, 65536, 0, 1, 1);
        match("line_matches folding whole pattern past the low word", qline, 65538, qpat, 65536, 1, 1, 1);
        /* The storage holds the pattern, but the stated line is one byte. */
        match("line_matches pattern past the low word over a short line", qline, 1, qpat, 65536, 0, 0, 0);
        match("line_matches folding pattern past the low word over a short line", qline, 1, qpat, 65536, 1, 0, 0);
    }
    /* Both lengths are 65536. A count cut to 16 bits is zero, so a routine
       that cuts it compares no bytes and reports a match. */
    {
        static char line_a[65537];
        static char pat_b[65537];
        memset(line_a, 'a', 65536);
        memset(pat_b, 'b', 65536);
        match("line_matches comparison count past the low word", line_a, 65536, pat_b, 65536, 0, 0, 0);
        match("line_matches folding comparison count past the low word", line_a, 65536, pat_b, 65536, 1, 0, 0);
    }
    match("line_matches underscore after", "cat_", 4, "cat", 3, 0, 1, 0);
    match("line_matches underscore before", "_cat", 4, "cat", 3, 0, 1, 0);
    match("line_matches punctuation after", "cat.", 4, "cat", 3, 0, 1, 1);
    match("line_matches folding whole word standalone", "The CAT sat", 11, "cat", 3, 1, 1, 1);
    match("line_matches folding word character before", "XCAT", 4, "cat", 3, 1, 1, 0);
    match("line_matches folding underscore after", "CAT_", 4, "cat", 3, 1, 1, 0);
    match("line_matches folding punctuation after", "CAT.", 4, "cat", 3, 1, 1, 1);

    /* The buffer must return unchanged, whatever the flags. */
    {
        char scratch[] = "The Cat Ran";
        char before[12];
        memcpy(before, scratch, sizeof scratch);
        line_matches(scratch, 11, "cat", 3, 1, 0);
        if (memcmp(before, scratch, sizeof scratch) == 0) {
            ok("line_matches leaves the buffer unchanged");
        } else {
            fail("line_matches leaves the buffer unchanged", "the buffer was modified");
        }
    }
}

/* --- int_to_dec ----------------------------------------------------- */

static void format_one(int n, const char *want, const char *name)
{
    char buf[24];
    int i;
    for (i = 0; i < 24; i++)
        buf[i] = '#';

    int got = int_to_dec(n, buf);
    int wantlen = (int)strlen(want);

    if (got != wantlen) {
        char detail[96];
        snprintf(detail, sizeof detail, "wrote %d bytes, wanted %d", got, wantlen);
        fail(name, detail);
        return;
    }
    if (memcmp(buf, want, (size_t)wantlen) != 0) {
        char detail[96];
        char shown[24];
        memcpy(shown, buf, (size_t)wantlen);
        shown[wantlen] = '\0';
        snprintf(detail, sizeof detail, "wrote \"%s\", wanted \"%s\"", shown, want);
        fail(name, detail);
        return;
    }
    if (buf[wantlen] != '#') {
        fail(name, "wrote a terminator the contract does not ask for");
        return;
    }
    ok(name);
}

static void format_checks(void)
{
    format_one(0, "0", "int_to_dec(0)");
    format_one(7, "7", "int_to_dec(7)");
    format_one(42, "42", "int_to_dec(42)");
    format_one(1000, "1000", "int_to_dec(1000)");
    format_one(INT_MAX, "2147483647", "int_to_dec(INT_MAX)");
}

/* --- parse_args ----------------------------------------------------- */

static void parse_one(const char *name, int argc, char **argv,
                      int want_ret, int want_flags, const char *want_pat, const char *want_path)
{
    struct args a;
    memset(&a, 0x7F, sizeof a);

    int got = parse_args(argc, argv, &a);

    if (got != want_ret) {
        char detail[96];
        snprintf(detail, sizeof detail, "returned %d, wanted %d", got, want_ret);
        fail(name, detail);
        return;
    }
    if (want_ret != 0) {
        ok(name);
        return;
    }
    if (a.flags != want_flags) {
        char detail[96];
        snprintf(detail, sizeof detail, "flags %d, wanted %d", a.flags, want_flags);
        fail(name, detail);
        return;
    }
    if (strcmp(a.pattern, want_pat) != 0 || strcmp(a.path, want_path) != 0) {
        char detail[160];
        snprintf(detail, sizeof detail, "pattern \"%s\", path \"%s\"; wanted \"%s\" and \"%s\"",
                 a.pattern, a.path, want_pat, want_path);
        fail(name, detail);
        return;
    }
    ok(name);
}

static void parse_checks(void)
{
    char *grouped[]   = { "rengrep", "-nvi", "cat", "tests/sample.txt" };
    char *repeated[]  = { "rengrep", "-n", "-n", "cat", "f" };
    char *interleaved[] = { "rengrep", "cat", "-n", "f" };
    char *allfive[]   = { "rengrep", "-ncviw", "p", "f" };
    char *lone_dash[] = { "rengrep", "-", "f" };
    char *no_pattern[] = { "rengrep" };
    char *flags_only[] = { "rengrep", "-n" };
    char *three_positional[] = { "rengrep", "a", "b", "c" };
    char *unknown[]   = { "rengrep", "-x", "cat", "f" };
    char *unknown_grouped[] = { "rengrep", "-nx", "cat", "f" };
    char *extra[]     = { "rengrep", "-n", "cat", "f", "extra" };
    char *no_file[]   = { "rengrep", "cat" };
    char *flag_after_file[] = { "rengrep", "cat", "f", "-n" };

    parse_one("parse_args grouped flags", 4, grouped, 0, FLAG_N | FLAG_V | FLAG_I, "cat", "tests/sample.txt");
    parse_one("parse_args repeated flag", 5, repeated, 0, FLAG_N, "cat", "f");
    parse_one("parse_args flag after the pattern", 4, interleaved, 0, FLAG_N, "cat", "f");
    parse_one("parse_args all five in one token", 4, allfive, 0, 31, "p", "f");
    parse_one("parse_args lone dash is positional", 3, lone_dash, 0, 0, "-", "f");
    parse_one("parse_args missing pattern", 1, no_pattern, -1, 0, NULL, NULL);
    parse_one("parse_args flags with no pattern", 2, flags_only, -1, 0, NULL, NULL);
    parse_one("parse_args three positionals", 4, three_positional, -1, 0, NULL, NULL);
    parse_one("parse_args unknown flag", 4, unknown, 'x', 0, NULL, NULL);
    parse_one("parse_args unknown flag in a group", 4, unknown_grouped, 'x', 0, NULL, NULL);
    parse_one("parse_args extra positional", 5, extra, -1, 0, NULL, NULL);
    parse_one("parse_args missing file", 2, no_file, -1, 0, NULL, NULL);
    parse_one("parse_args flag after the file", 4, flag_after_file, 0, FLAG_N, "cat", "f");

    /* The offending byte returns as a positive int, also above 127. */
    {
        char high_flag[] = { '-', (char)0xFF, 0 };
        char *high[] = { "rengrep", high_flag, "cat", "f" };
        parse_one("parse_args unknown flag byte above 127", 4, high, 255, 0, NULL, NULL);
    }
    /* argc is a full int. */
    {
        char *many[258];
        int i;
        many[0] = "rengrep";
        for (i = 1; i < 255; i++)
            many[i] = "-n";
        many[255] = "cat";
        many[256] = "f";
        many[257] = NULL;
        parse_one("parse_args argc past the low byte", 257, many, 0, FLAG_N, "cat", "f");
    }
    /* argc is a full int past the low word too. */
    {
        static char *wide[65538];
        int i;
        wide[0] = "rengrep";
        for (i = 1; i < 65535; i++)
            wide[i] = "-n";
        wide[65535] = "cat";
        wide[65536] = "f";
        wide[65537] = NULL;
        parse_one("parse_args argc past the low word", 65537, wide, 0, FLAG_N, "cat", "f");
    }
}

/* --- the hostile caller --------------------------------------------- */

static void report_mask(const char *routine, int mask)
{
    if (mask == 0) {
        printf("ok    %s keeps ebx, esi, edi, and esp\n", routine);
        return;
    }
    printf("FAIL  %s changed:", routine);
    if (mask & 1) printf(" ebx");
    if (mask & 2) printf(" esi");
    if (mask & 4) printf(" edi");
    if (mask & 8) printf(" esp");
    printf("\n");
    failures++;
}

static void hostile_checks(void)
{
    char buf[] = "the cat sat\ncat\n";
    struct line lines[4];
    struct args a;
    char *argv[] = { "rengrep", "-n", "cat", "f" };
    char num[24];
    int mask;

    mask = check_index_registers(buf, (int)strlen(buf), lines, 4);
    report_mask("index_lines", mask);

    mask = check_matches_registers(buf, 11, "cat", 3, 0, 0);
    report_mask("line_matches", mask);

    mask = check_fmt_registers(42, num);
    report_mask("int_to_dec", mask);

    mask = check_parse_registers(4, argv, &a);
    report_mask("parse_args", mask);
}

int main(void)
{
    lines_checks();
    matches_checks();
    format_checks();
    parse_checks();
    hostile_checks();

    printf("\n");
    if (failures == 0) {
        printf("The contract checks passed.\n");
        return 0;
    }
    printf("%d contract check(s) failed.\n", failures);
    return 1;
}

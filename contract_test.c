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

static void check_lines(const char *name, char *buf, int len, int max,
                        int want_count, const struct line *want)
{
    struct line out[8];
    int i;

    /* A sentinel pattern, so a write past the capacity is visible. */
    for (i = 0; i < 8; i++) {
        out[i].offset = 0x7F7F7F7F;
        out[i].length = 0x7F7F7F7F;
    }

    int got = index_lines(buf, len, out, max);

    if (got != want_count) {
        char detail[96];
        snprintf(detail, sizeof detail, "returned %d, wanted %d", got, want_count);
        fail(name, detail);
        return;
    }
    for (i = 0; i < want_count && i < 8; i++) {
        if (out[i].offset != want[i].offset || out[i].length != want[i].length) {
            char detail[128];
            snprintf(detail, sizeof detail,
                     "entry %d is {%d,%d}, wanted {%d,%d}",
                     i, out[i].offset, out[i].length, want[i].offset, want[i].length);
            fail(name, detail);
            return;
        }
    }
    for (i = want_count; i < 8; i++) {
        if (out[i].offset != 0x7F7F7F7F || out[i].length != 0x7F7F7F7F) {
            char detail[128];
            snprintf(detail, sizeof detail,
                     "wrote entry %d past the capacity of %d", i, max);
            fail(name, detail);
            return;
        }
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
}

/* --- line_matches --------------------------------------------------- */

static void match(const char *name, char *line, int linelen, char *pat, int patlen,
                  int fold, int whole, int want)
{
    int got = line_matches(line, linelen, pat, patlen, fold, whole);
    if (got == want) {
        ok(name);
    } else {
        char detail[96];
        snprintf(detail, sizeof detail, "returned %d, wanted %d", got, want);
        fail(name, detail);
    }
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
    match("line_matches underscore after", "cat_", 4, "cat", 3, 0, 1, 0);
    match("line_matches underscore before", "_cat", 4, "cat", 3, 0, 1, 0);
    match("line_matches punctuation after", "cat.", 4, "cat", 3, 0, 1, 1);

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

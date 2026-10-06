#!/usr/bin/env bash
#
# rengrep correctness gate. It runs two passes.
#
# Pass 1, the oracle. The installed grep is the reference. Both programs run
# with the same arguments, their stdout is captured to files, and the files
# are compared byte for byte. The oracle is invoked as
#
#       LC_ALL=C grep -F $flags "$pattern" "$file"
#
# The C locale keeps character classes and case folding identical on every
# machine. -F makes the pattern a fixed string, which is the subset rengrep
# implements: a pattern such as "." matches a literal dot and not any
# character.
#
# Every flag subset is tested: no flags, each of the five singles, every
# pair, every triple, every quadruple, and all five together. That is 32
# subsets, against 7 patterns and 6 files, or 1344 comparisons.
#
# Pass 2, the contract. ./contract_test calls the four routines directly and
# checks the capacity argument, the boundary rules, the formatter, the
# parser, and the register discipline. Those are invisible to the oracle.
#
#       ./run_tests.sh
#
# Build both programs first (make). Each run is captured before its text is
# compared, and every status is read, so a crash cannot pass as a match.
# grep exits 0 when it matched and 1 when it did not. Both are accepted.
# grep exits 2 on an error, and that is a failure of the harness. rengrep
# must exit 0 on every valid invocation.

set -uo pipefail

bin="./rengrep"
if [ ! -x "$bin" ] && [ -x "$bin.exe" ]; then
    bin="$bin.exe"
fi

if [ ! -x "$bin" ]; then
    echo "run_tests.sh: $bin not found. Build it first: make" >&2
    exit 1
fi

command -v grep >/dev/null 2>&1 || {
    echo "run_tests.sh: grep not found on PATH" >&2
    exit 1
}

expected="./.grep.out"
actual="./.rengrep.out"
trap 'rm -f "$expected" "$actual"' EXIT

failures=0
total=0

# The 32 flag subsets, built from the five flags. Bit order follows the
# FLAG_ constants in driver.c.
flagsets=()
for mask in $(seq 0 31); do
    f=""
    if [ $((mask & 1)) -ne 0 ]; then f="$f -n"; fi
    if [ $((mask & 2)) -ne 0 ]; then f="$f -c"; fi
    if [ $((mask & 4)) -ne 0 ]; then f="$f -v"; fi
    if [ $((mask & 8)) -ne 0 ]; then f="$f -i"; fi
    if [ $((mask & 16)) -ne 0 ]; then f="$f -w"; fi
    flagsets+=("$f")
done

# Seven patterns. A word, its capitalised form, a two-character substring,
# a single character, a sentence longer than any line, a dot (which -F makes
# literal), and the empty pattern, which matches every line.
patterns=( "cat" "Cat" "he " "e" "The quick brown fox jumps over the lazy dog" "." "" )

files=( tests/*.txt )

for flags in "${flagsets[@]}"; do
    for pat in "${patterns[@]}"; do
        for file in "${files[@]}"; do
            total=$((total + 1))

            LC_ALL=C grep -F $flags "$pat" "$file" > "$expected" 2>/dev/null
            gstatus=$?
            if [ "$gstatus" -gt 1 ]; then
                echo "FAIL  grep itself failed (status $gstatus): $flags '$pat' $file"
                failures=$((failures + 1))
                continue
            fi

            "$bin" $flags "$pat" "$file" > "$actual" 2>/dev/null
            rstatus=$?
            if [ "$rstatus" -ne 0 ]; then
                echo "FAIL  rengrep exited with status $rstatus: $flags '$pat' $file"
                failures=$((failures + 1))
                continue
            fi

            # --strip-trailing-cr matters on Windows: the .exe writes \r\n
            # while grep and the test files use \n. It is harmless
            # everywhere else.
            if ! diff -q --strip-trailing-cr "$expected" "$actual" >/dev/null; then
                echo "FAIL  rengrep $flags '$pat' $file"
                failures=$((failures + 1))
            fi
        done
    done
done

# Pass 2: the contract test.
testbin="./contract_test"
if [ ! -x "$testbin" ] && [ -x "$testbin.exe" ]; then
    testbin="$testbin.exe"
fi

if [ ! -x "$testbin" ]; then
    echo "run_tests.sh: $testbin not found. Build it first: make" >&2
    exit 1
fi

total=$((total + 1))
if "$testbin"; then
    echo "ok    contract"
else
    echo "FAIL  contract"
    failures=$((failures + 1))
fi

echo
if [ "$failures" -eq 0 ]; then
    echo "All $total checks passed."
    exit 0
else
    echo "$failures of $total checks failed."
    exit 1
fi

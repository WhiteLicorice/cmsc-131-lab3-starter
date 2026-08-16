#!/usr/bin/env bash
#
# rengrep correctness gate. Correctness is agreement with the real grep,
# byte for byte, for every flag combination on every test file.
#
#       ./run_tests.sh
#
# Build rengrep first (make). The comparison, per the manual:
#
#   grep $flags "$pattern" "$file" > expected.txt
#   ./rengrep $flags "$pattern" "$file" > actual.txt
#   diff --strip-trailing-cr expected.txt actual.txt
#
# grep exits 1 when nothing matched; run_tests.sh compares output only, so
# match the output and do not worry about exit status.

set -u

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

failures=0
total=0

# Every single flag and every pair, across every test file. The manual says
# "5 files by 6 flag settings and their pairs"; the patterns below cover
# singles and all ten pairs.
flagsets=( "" "-n" "-c" "-v" "-i" "-w"
           "-n -c" "-n -v" "-n -i" "-n -w"
           "-c -v" "-c -i" "-c -w"
           "-v -i" "-v -w" "-i -w" )

# Patterns: a word, a substring, and a single character, so both the fast
# path and the folding path get exercised, plus a pattern longer than any
# line (longlines.txt exists for that).
patterns=( "cat" "he " "e" "The quick brown fox jumps over the lazy dog" )

for flags in "${flagsets[@]}"; do
    for pat in "${patterns[@]}"; do
        for file in tests/*.txt; do
            total=$((total + 1))
            # --strip-trailing-cr matters on Windows: the .exe emits \r\n
            # while grep's output and the test files use \n. It is harmless
            # everywhere else.
            if grep $flags "$pat" "$file" 2>/dev/null | diff -q --strip-trailing-cr - \
                <("$bin" $flags "$pat" "$file" 2>/dev/null) >/dev/null 2>&1; then
                :
            else
                echo "FAIL  rengrep $flags '$pat' $file"
                failures=$((failures + 1))
            fi
        done
    done
done

echo
if [ "$failures" -eq 0 ]; then
    echo "All $total comparisons matched grep."
    exit 0
else
    echo "$failures of $total comparisons differ from grep."
    exit 1
fi

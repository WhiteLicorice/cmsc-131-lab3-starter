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
#
# The first run saves each grep output in .grep-cache, and later runs read
# it from there. Only the first run starts grep 1344 times, so it takes the
# longest. The script makes the cache again when the installed grep, this
# script, a test file, or a saved output changes. When one of them changes
# during a run, the script runs the tests again with grep.
#
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

actual="./.rengrep.out"
trap 'rm -f "$actual"' EXIT

# Read the file $2 into the variable named $1, with no new process. A shell
# variable cannot hold a NUL byte. So each \001 becomes \001\001, and each
# NUL becomes \001\002. Two files then give equal text only when they hold
# equal bytes. A CR before a newline is dropped, as
# diff --strip-trailing-cr drops it. The .exe on Windows writes \r\n, and
# grep and the test files write \n.
slurp() {
    local part="" text="" sep=""
    while IFS= read -r -d '' part; do
        text+="$sep${part//$'\001'/$'\001\001'}"
        sep=$'\001\002'
    done < "$2"
    text+="$sep${part//$'\001'/$'\001\001'}"
    printf -v "$1" '%s' "${text//$'\r\n'/$'\n'}"
}

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

# The cache key is a checksum of the grep program, this script, and each
# test file, with their names. The script takes the key before grep runs.
# After the last grep, the stamp records the key and a checksum of each saved
# output. A later run reads the cache only when both checksums still match.
# An edit to any of these files makes the cache again, also an edit made
# during the first run. A run that reads the cache checks both again at the
# end. GitHub Actions sets CI. There the script always runs
# grep, so a saved output committed to the repository cannot stand in for
# grep.
cache="./.grep-cache"
stamp="$cache/stamp"
cells=$(( ${#flagsets[@]} * ${#patterns[@]} * ${#files[@]} ))
outputs=()
for (( n = 1; n <= cells; n++ )); do
    outputs+=( "$n.out" )
done
key="$(cksum "$(type -P grep)" "$0" "${files[@]}" 2>&1)"
cached=0
if [ -z "${CI:-}" ] && [ -f "$stamp" ]; then
    slurp have "$stamp"
    sums="$(cd "$cache" && cksum "${outputs[@]}" 2>&1)"
    if [ "$have" = "$key"$'\n'"$sums" ]; then
        cached=1
    fi
fi
if [ "$cached" -eq 0 ]; then
    rm -rf "$cache"
    mkdir -p "$cache"
    echo "Saving the output of the installed grep in $cache."
    echo "This first run takes the longest. Later runs read the saved output."
    echo
fi
grep_failed=0

# A run that reads the cache keeps its failure lines until the end. The
# check at the end can then discard them.
held=""
report() {
    if [ "$cached" -eq 1 ]; then
        held+="$1"$'\n'
    else
        echo "$1"
    fi
}

for flags in "${flagsets[@]}"; do
    for pat in "${patterns[@]}"; do
        for file in "${files[@]}"; do
            total=$((total + 1))
            expected="$cache/$total.out"

            if [ "$cached" -eq 0 ]; then
                LC_ALL=C grep -F $flags "$pat" "$file" > "$expected" 2>/dev/null
                gstatus=$?
                if [ "$gstatus" -gt 1 ]; then
                    rm -f "$expected"
                    grep_failed=1
                    echo "FAIL  grep itself failed (status $gstatus): $flags '$pat' $file"
                    failures=$((failures + 1))
                    continue
                fi
            fi

            "$bin" $flags "$pat" "$file" > "$actual" 2>/dev/null
            rstatus=$?
            if [ "$rstatus" -ne 0 ]; then
                report "FAIL  rengrep exited with status $rstatus: $flags '$pat' $file"
                failures=$((failures + 1))
                continue
            fi

            # Compare in the shell. A diff process for each comparison is
            # the slowest step under Git Bash on Windows.
            slurp want "$expected"
            slurp got "$actual"
            if [[ "$got" != "$want" ]]; then
                report "FAIL  rengrep $flags '$pat' $file"
                failures=$((failures + 1))
            fi
        done
    done
done

# Write the stamp only after every grep output is saved.
if [ "$cached" -eq 0 ] && [ "$grep_failed" -eq 0 ]; then
    sums="$(cd "$cache" && cksum "${outputs[@]}" 2>&1)"
    printf '%s\n%s' "$key" "$sums" > "$stamp"
fi

# A change during a cached run makes some saved outputs wrong for the files
# that rengrep read. The script then runs the tests again with grep. That
# run does not read the cache, so it does not come back here.
if [ "$cached" -eq 1 ]; then
    now="$(cksum "$(type -P grep)" "$0" "${files[@]}" 2>&1)"
    sums="$(cd "$cache" && cksum "${outputs[@]}" 2>&1)"
    if [ "$have" != "$now"$'\n'"$sums" ]; then
        echo "grep, $0, a test file, or a saved output changed during the run."
        echo "The script runs the tests again with grep."
        echo
        rm -f "$stamp" "$actual"
        exec "$BASH" "$0"
    fi
    printf '%s' "$held"
fi

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

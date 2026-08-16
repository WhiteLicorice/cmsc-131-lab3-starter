<!--no-pdf-->
# CMSC 131 Lab lab3 Starter

A grep clone whose correctness is agreement with the real grep, byte for byte. The manual is the assignment. This file is the repository's own notes.

## Layout

```text
Makefile       platform preamble and build rules
driver.c        provided: argument parsing, file reading, and output
match.asm       yours
lines.asm       yours
fmt.asm         yours
run_tests.sh   provided: the correctness gate
tests/         provided: the test corpus
```

## What to Run

```bash
make
make check
```

`make` builds `driver.c        provided: argument parsing, file reading, and output
match.asm       yours
lines.asm       yours
fmt.asm         yours`. `make check` builds, then runs `./run_tests.sh`,
which reports each test and exits nonzero when any of them differ.

## Reading a First Run

The assembly files ship as stubs that assemble and link as-is, so the build
works before any code is written. Right now they do nothing useful, which
makes every check fail. That red run is the correct starting state for a
starter, and the badge stays red until the routines are implemented.

The provided files are fixtures. The grader compares your fork against the
starter, so an edited `driver.c`, `Makefile`, `run_tests.sh`, or `tests/`
file shows up as a diff in the open.

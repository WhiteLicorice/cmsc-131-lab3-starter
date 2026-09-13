<!--no-pdf-->
# CMSC 131 Lab 3 Starter

A grep clone whose correctness is agreement with `grep` itself, byte for byte. The manual is the assignment. This file is the repository's own notes.

## Layout

```text
Makefile            platform preamble and build rules
driver.c            provided: file reading, the line loop, and output
args.asm            yours
match.asm           yours
lines.asm           yours
fmt.asm             yours
run_tests.sh        provided: the correctness gate
contract_test.c     provided: the second pass, in C
contract_regs.asm   provided: register checks for contract_test
tests/              provided: the test corpus
```

## What to Run

```bash
make
make check
```

`make` builds `rengrep` and `contract_test`. `make check` builds both, then
runs `./run_tests.sh`, which reports each test and exits nonzero when any
of them differ.

The gate has two passes. The first runs your tool and `grep` with the same
arguments and compares the two outputs byte for byte, across all 32 flag
subsets, 6 patterns, and 5 files: 960 comparisons. The second is
`contract_test`, which calls the four routines directly and checks the
capacity argument, the boundary rules, the formatter, the parser, and the
register discipline. Both passes run every time, because each one catches
what the other cannot.

## Reading a First Run

The assembly files ship as stubs that assemble and link as-is, so the build
works before any code is written. Right now the parser stub exits 2 for
every command. The gate requires exit 0 for a valid invocation, so all
961 checks fail. That red run is the correct starting state for a starter.
Expect the first passing cells once `args.asm` and `lines.asm` are in
place, and the badge turns green when the matching and formatting routines
are done.

The provided files are fixtures. The grader compares your fork against the
starter, so an edited `driver.c`, `Makefile`, `run_tests.sh`, or `tests/`
file shows up as a diff in the open.

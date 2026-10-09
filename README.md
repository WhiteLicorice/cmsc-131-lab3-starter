<!--no-pdf-->
# CMSC 131 Lab 3 Starter

[![lab3-checks](https://github.com/WhiteLicorice/cmsc-131-lab3-starter/actions/workflows/test.yml/badge.svg)](https://github.com/WhiteLicorice/cmsc-131-lab3-starter/actions/workflows/test.yml)

A grep clone whose correctness is agreement with `grep` itself, byte for byte. The manual is the assignment. This file is the repository's own notes.

## Layout

```text
Makefile            platform preamble and build rules
driver.c            provided: file reading, the line loop, and output
cdecl.h             provided: the cdecl macros for the C boundary
args.asm            yours
match.asm           yours
lines.asm           yours
fmt.asm             yours
run_tests.sh        provided: the correctness gate
contract_test.c     provided: the second pass, in C
contract_regs.asm   provided: register checks for contract_test
LICENSE             provided: the repository licence
tests/              provided: the test corpus
```

## What to Run

On Windows, run these commands in Git Bash, the shell from Block 1. In that
shell, `make` is your alias for `mingw32-make`. On Linux, use
your terminal.

```bash
make
make check
```

`make` builds `rengrep` and `contract_test`. `make check` builds both, then
runs `./run_tests.sh`, which reports each test and exits nonzero when any
of them differ.

The first `make check` takes the longest, often more than a minute on
Windows. It saves the output of your installed `grep` in `.grep-cache`, and
later runs read that saved output.

The gate has two passes. The first runs your tool and `grep` with the same
arguments and compares the two outputs byte for byte. It does that across
all 32 flag subsets, 7 patterns, and 6 files: 1344 comparisons. The second
is `contract_test`. It calls the four routines directly. It checks the
capacity argument, the boundary rules, the formatter, the parser, and the
register discipline. Both passes run every time, because each one catches
what the other cannot.

## Reading a First Run

The assembly files ship as stubs that assemble and link as-is, so the build
works before you write any code. Right now the parser stub exits 2 for
every command. The gate requires exit 0 for a valid invocation, so all
1345 checks fail. That red run is the correct starting state for a starter.
Expect the first passing cells once `args.asm` and `lines.asm` are in
place, and the badge turns green when the matching and formatting routines
are done.

The provided files are fixtures. The grader compares your fork against the
starter. An edit to `driver.c`, `Makefile`, `run_tests.sh`, or a `tests/`
file appears as a diff in the open.

## Documentation

The three sections at the end of this file are yours. Complete Design Notes
and Subsystem Ownership before the Week 1 progress report. Complete Quirks
and Issues before the Week 3 progress report. Each section says what it
needs. Leave the rest of this file as it is.

---

## Design Notes

Complete this section before the Week 1 progress report. The syllabus asks
for problem analysis, a solution architecture, and an estimated timeline.
Keep each part short. Update it when the plan changes.

### Problem analysis

The five flags and what each one does. What the oracle is, and what
agreement with it means for a line that matches inside a longer word.

### Solution architecture

How the four routines split the work. How the line index is built from
one buffer, how the flag table dispatches, and which registers each
routine uses.

### Timeline

One line per week. Name the subsystem each week finishes and the member
who owns it.

| Week | Goal | Owner |
|---|---|---|
| 1 | | |
| 2 | | |
| 3 | | |
| 4 | Defense | |

## Subsystem Ownership

Complete this section before the Week 1 progress report. The manual lists
the three subsystems. Each member owns one. In a group of four, two members
share one. The commit history must agree with this table.

| Subsystem | Owner |
|---|---|
| Buffering and lines (`lines.asm`) | |
| Matching (`match.asm`) | |
| Formatting and flags (`fmt.asm`, `args.asm`, the tests you add) | |

## Quirks and Issues

Complete this section before the Week 3 progress report. The syllabus asks
for documentation of quirks and issues with the complete implementation.
One entry per item. State what happens, what causes it, and what the group
did about it.

### Known issues

- 

### Quirks

- 

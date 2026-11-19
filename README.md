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
later runs read that saved output. A change to your `grep`, `run_tests.sh`,
or a test file makes the next run save the output again.

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

The five flags supported by `rengrep` are `-n`, `-c`, `-v`, `-i`, `-w`.<br>
`-n` prefixes each line with a 1-based line number and a colon<br>
`-c` prints only how many lines matched and nothing else<br>
`-v` prints the lines that didn't match by inverting the match<br>
`-i` performs case-insensitive matching by folding cases<br>
`-w` matches only when patterns are whole words<br>

The oracle for this tool is the `grep` command. Agreement with this oracle defines the correctness of the implementation, based on byte-for-byte identical output across all flag combinations. This means finding a substring match inside a longer word (e.g. "cat" in "concatenate") is considered a valid match and must be reported as such, just as in `grep`. Only when the `-w` flag is provided, which restricts matches to whole words, are these matches excluded.

### Solution architecture

### - Routines and Registers

The workload is respectively distributed across the four following assembly routines:

`parse_args` processes command-line arguments to extract the pattern, file path. and flags. It handles flag combinations through a defined flag character table. Using the `scasb` instruction, this table is scanned to identify the provided flags and set the bits in a single flag integer.

| Register Used | Purpose |
|---|---|
| al | holds current flag char being parsed from the argument |
| edi | pointer for flag_chars array |
| ecx | counter initialized to 5 that decrements for table scan (repne scasb) |
| edx | calculate the table index by subtracting remaining ecx value from 4 |
<br><br>

`index_lines` processes raw file buffer into a structured format. It scans the inptit buffer to find newlines, and records the starting offset and length of each line to build an array of line structures in memory.

| Register Used | Purpose |
|---|---|
| edi | scanning pointer that traverses the file buffer, like a moving cursor |
| al | holds the newline character |
| ecx | remaining bytes in the file buffer |
| esi | pointer to struct line entry in the `out` array |
| edx | holds offset of the current line being measured |
| ebx | holds max boundary limit |
| eax | running counter for number of successfully indexed lines |
<br><br>

`line_matches` executes the search logic, using x86 string instructions like `repe cmpsb` to evaluate matches and conditionally apply case-folding and word-boundary logic based on the parsed flags.

| Register Used | Purpose |
|---|---|
| esi | source pointer for pattern during string comparisons |
| edi | destination pointer for candidate line buffer during comparisons |
| ecx | holds pattern length |
| al | holds individual bytes for the `-i` flag's in-place case folding |
<br><br>

`int_to_dec` handles text formatting for the `-n` and `-c` flags. It converts integers into their decimal representations by repeatedly dividing by 10 and pushing the remainders into the stack, reversing the digit order before popping them into the output buffer.

| Register Used | Purpose |
|---|---|
| eax | holds the active quotient |
| ebx | holds the divisor, 10 |
| edx | (cleared to 0) holds the remainder |
| ecx | counter for each remainder pushed to the stack |
| edi | destination pointer for output buffer (popped remainders) |

By cdecl convention, the callee-saved registers ebx, esi, edi, and ebp are preserved, and eax is used for return values for all routines.

### - Flag Table Dispatching
The flag parsing mechanism uses a defined character table, which maps directly to bit positions 0-4. When `parse_args` receives a flag character, it uses the `repne scasb` instruction to scan the table, reflecting found matches in the remaining bytes counter (ecx). By subtracting the counter value from 4, the routine calculates the exact index of the matched flag. 

Once the index is calculated, a 1 is shifted left by that amount, and is OR-ed into an integer. This condenses any flag combinations into one bitmask. The integer is returned across the C boundary to the driver, which reads the bits in the mask to conditionally branch and trigger proper search, formatting, and output behaviors like a dispatcher.

### Timeline

One line per week. Name the subsystem each week finishes and the member
who owns it.

| Week | Goal | Owner |
|---|---|---|
| 1 | Buffering and lines: build working prototype that builds and runs on tests/sample.txt | Justin B. Lauricio |
| 2 | Matching: complete all match routine implementations, get rengrep with no flags to work correctly | Samantha F. Mok |
| 3 | Formatting and flags: finish int_to_dec routine, flag table and parse_args, additional tests | Percie Louise Y. Samaniego |
| 4 | Defense | All members |

## Subsystem Ownership

Complete this section before the Week 1 progress report. The manual lists
the three subsystems. Each member owns one. In a group of four, two members
share one. The commit history must agree with this table.

| Subsystem | Owner |
|---|---|
| Buffering and lines (`lines.asm`) | Justin B. Lauricio |
| Matching (`match.asm`) | Samantha F. Mok |
| Formatting and flags (`fmt.asm`, `args.asm`, the tests you add) | Percie Louise Y. Samaniego |

## Quirks and Issues

Complete this section before the Week 3 progress report. The syllabus asks
for documentation of quirks and issues with the complete implementation.
One entry per item. State what happens, what causes it, and what the group
did about it.

### Known issues

- 

### Quirks

- 

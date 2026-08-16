# CMSC 131 platform preamble. Shared by every laboratory activity.
#
# This file is not a Makefile on its own. make-lab-bundles.sh concatenates it
# ahead of each lab's own Makefile (the part that names that lab's objects and
# targets), so a student's bundle contains one Makefile that reads top to
# bottom: platform detection first, then the activity's rules. The split is so
# the platform logic — the only part that repeats across activities — is
# maintained once here instead of three times.
#
# The platform check asks twice on purpose. Windows sets OS=Windows_NT in the
# environment and a native make imports it. That alone is not enough: a make
# built for MSYS2 or Cygwin reports OS as empty even on Windows, and such a
# make is easy to end up with by accident, so asking uname as well is what
# stops this file from quietly picking the Linux branch on a Windows machine
# and failing several steps later.

UNAME := $(shell uname -s 2>/dev/null)

ifeq ($(OS),Windows_NT)
  PLATFORM := windows
else ifneq (,$(findstring MINGW,$(UNAME)))
  PLATFORM := windows
else ifneq (,$(findstring MSYS,$(UNAME)))
  PLATFORM := windows
else ifneq (,$(findstring CYGWIN,$(UNAME)))
  PLATFORM := windows
else
  PLATFORM := $(UNAME)
endif

NASM := nasm
CC   := gcc

ifeq ($(PLATFORM),windows)
  # COFF objects, and the linker needs telling this is a console program
  # rather than a windowed one.
  ASFLAGS := -f win32
  LDFLAGS := -Wl,-subsystem,console
  EXE     := .exe
else
  # ELF objects. -d ELF_TYPE reaches every assembly file, where it respells
  # the decorated entry points (same trick asm_io.inc uses for the bootcamp
  # blocks: you write the Windows spelling everywhere, and the flag swaps it
  # for the undecorated one C uses on Linux).
  #
  # -no-pie matters: gcc has defaulted to position-independent executables
  # since Ubuntu 17.10, and the absolute addressing this course's assembly
  # cannot be relocated that way. Without it the link fails with
  # "relocation R_386_32 ... can not be used when making a PIE object".
  ASFLAGS := -f elf32 -d ELF_TYPE
  LDFLAGS := -no-pie
  EXE     :=
endif

CFLAGS := -m32

# The laboratory activities bind assembly to a C driver under the cdecl
# convention, so an object rule for each language and one link rule cover
# every activity's build. Names are decorated with a leading underscore on
# Windows but not on Linux; the assembly sources handle that themselves
# (there is no asm_io.inc remap here), so nothing in this file does.
%.obj: %.asm
	$(NASM) $(ASFLAGS) $< -o $@

%.o: %.c cdecl.h
	$(CC) $(CFLAGS) -c $< -o $@
# rengrep build rules. The platform preamble above this file (detection,
# NASM/CC, flags, and the object rules) is shared across the laboratory
# activities and is maintained in lab-shared/Makefile.platform.
#
# This half names this activity's objects and targets:
#
#   make            build rengrep
#   make check      build, then run ./run_tests.sh
#   make test       alias for check
#   make clean      delete build output
#
# Three separately assembled modules, linked together with the C driver.
# The multi-module split is part of the activity's requirements.

BIN  := rengrep$(EXE)
OBJS := driver.o match.obj lines.obj fmt.obj

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

driver.o: driver.c cdecl.h

match.obj: match.asm
lines.obj: lines.asm
fmt.obj: fmt.asm

all: $(BIN)

check: $(BIN)
	bash ./run_tests.sh

test: check

clean:
	rm -f $(BIN) *.obj *.o

.PHONY: all check test clean

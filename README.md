# mira-compiler
Self-contained compiler for the Mira language (x86-64) — lexer → parser → IR → SSA → self-written linker, no external assembler/linker needed. Windows (COFF/PE) &amp; Linux (ELF) from one source tree; O0–O3 identical semantics; ~8 KB outputs; built-in concurrent runtime.

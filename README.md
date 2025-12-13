# clang-14 AST-based C++ Source Obfuscator

This tool is a minimal C++20 program using Clang's AST tooling (clang-14) to rename non-keyword identifiers in a source file.

Features
- Renames user-defined declarations (functions, variables, types, fields, typedefs) found in the main source file.
- Attempts to rewrite references (decl ref, member accesses, and simple type uses).
- Skips system headers and identifiers in `std::`.
- Accepts a protect file to preserve specific identifiers.

Build (example)

You need Clang/LLVM (clang-14) and CMake. Example on Debian/Ubuntu:

```bash
sudo apt install clang-14 libclang-14-dev llvm-14-dev cmake build-essential
mkdir build && cd build
cmake ..
make -j
```

Run

```bash
# simplest: print obfuscated source to stdout
./obfuscator /path/to/input.cpp

# with protect map (one identifier per line):
./obfuscator -protect=/path/to/protect.map /path/to/input.cpp
```

Notes & limitations
- This is a pragmatic starter implementation: it covers many common cases (decls, decl refs, member exprs, type locs) but may not cover every C++20 corner-case (macros, some elaborated template constructs, operator-function renames, macro-spelled identifiers).
- The tool deliberately avoids renaming entities in system headers and `std::` names.
- If you want stronger guarantees across complicated projects, integrate with a translation-phase-aware pipeline and extend matchers & callbacks.

If you want, I can adapt/extend it to cover macros, templates, namespaces, or output per-file results.

# PTXLab Project Status Ledger

Last Updated: 2026-10-25  
Current Milestone: **M7 — PTX-subset Parser, Validator & Reference Interpreter (Oracle O3)** (Completed) -> **M8 Next**  
Platform: **Native Windows (MinGW-w64 UCRT GCC 16.2 / Clang 23.1 + CMake 4.4 + Ninja 1.13)**

---

## 1. Toolchain & Environment Status

| Tool | Status | Version / Path |
|---|---|---|
| **GPU** | VERIFIED | NVIDIA GeForce RTX 5070 Ti Laptop GPU |
| **Compute Capability** | VERIFIED | sm_120 (CC 12.0) |
| **NVIDIA Driver** | VERIFIED | 591.91 (supports CUDA 13.1 driver API, `nvcuda.dll` dynamically loaded) |
| **C++ Compiler** | VERIFIED | MinGW-w64 GCC 16.2.0 (`g++`) + LLVM Clang++ 23.1.3 |
| **Build Tools** | VERIFIED | CMake 4.4.4, Ninja 1.13.2 |
| **GoogleTest** | VERIFIED (FetchContent) | v1.15.2 pinned |
| **Driver JIT Engine** | VERIFIED | `cuModuleLoadDataEx` JIT compiles PTX directly to sm_120 SASS at runtime |
| **ptxas (PTX Assembler)**| UNVERIFIED | Not in PATH; offline validation replaced by runtime driver JIT & execution |

---

## 2. Milestone Progress

| Milestone | Description | Status | Verification Evidence |
|---|---|---|---|
| **M0** | Environment, toolchain audit, CMake/GTest skeleton | **COMPLETED** | CMake 4.4 + Ninja + GCC 16.2 builds `kcc.exe` & `unit_tests.exe`; CTest 2/2 Passed (100%); Concept questions verified |
| **M1** | Diagnostics & Lexer | **COMPLETED** | SourceManager + DiagnosticEngine + DFA Lexer; 39/39 CTest passed (100%); `kcc --dump-tokens` verified on `saxpy.kc` |
| **M2** | Parser, AST, and gate-lab (LL(1)/FIRST/FOLLOW) | **COMPLETED** | Recursive descent parser + AST printer + panic mode recovery + gate-lab FIRST/FOLLOW/LL(1) tool; 57/57 CTest passed (100%); `kcc --dump-ast` verified |
| **M3** | Sema & AST Reference Interpreter (O1) | **COMPLETED** | Scoped symbol table + strict type checker (no implicit conversions) + UB-trapping AST interpreter; 101/101 CTest passed (100%); `kcc --check` & `kcc --run-ast` verified on `saxpy.kc` |
| **M4** | Typed TAC IR, CFG, Verifier & IR Interpreter (O2) | **COMPLETED** | Typed TAC IR + CFG basic blocks + jumping-code lowering + IR Verifier + CPU Reference Interpreter (O2); 119/119 CTest passed (100%); `kcc --dump-ir` & `kcc --run-ir` verified; bit-exact O1 == O2 parity verified |
| **M5** | PTX Emitter & Target Generation | **COMPLETED** | `PTXEmitter` for PTX 7.0 / sm_75 ISA; typed register banks (`%r`, `%f`, `%rd`, `%p`); parameter ABI + generic-to-global conversion; IEEE 754 `.rn` rounding; 64-bit wide array indexing; 127/127 CTest passed (100%); `kcc --emit-ptx [-o <file.ptx>]` verified |
| **M6** | GPU Runner (O4) via CUDA Driver API | **COMPLETED** | Dynamic `nvcuda.dll` loader; `cuModuleLoadDataEx` driver JIT compiler; VRAM memory management; `cuLaunchKernel` execution on RTX 5070 Ti (`sm_120`); 132/132 CTest passed (100%); 3-way differential parity $O_1 == O_2 == O_4$ verified; `kcc --run-gpu` and 7-stage `--pipeline` verified |
| **M7** | PTX-subset Parser, Validator & Interpreter (O3) | **COMPLETED** | Standalone PTX Lexer & Parser (`PTXModule` AST) + Structural & Type Validator + CPU Reference Interpreter (Oracle O3); 146/146 CTest passed (100%); 4-way differential parity $O_1 == O_2 == O_3 == O_4$ verified; `kcc --run-ptx <file.ptx\|file.kc>` and 8-stage `--pipeline` verified; static linking on Windows MinGW verified |
| **M8** | Data-flow Framework & Optimizations (CP, DCE, CSE)| **NEXT** | Optimization pipeline |
| **M9** | Multi-Oracle Differential Testing Harness (ptxdiff)| Pending | — |
| **M10**| Generator, Reducer & Mutation Testing | Pending | — |
| **M11**| Evaluation & Final Presentation | Pending | — |

---

## 3. Known Limitations & Open Issues
* `ptxas` is not yet installed in PATH; offline PTX assembly verification is substituted by driver JIT (`cuModuleLoadDataEx`) and the M7 PTX Validator & Interpreter.
* Fully static MinGW linking (`-static -static-libgcc -static-libstdc++`) is configured in CMake to ensure zero DLL dependencies and compliance with Windows Code Integrity policy.

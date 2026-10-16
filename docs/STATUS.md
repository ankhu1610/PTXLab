# PTXLab Project Status Ledger

Last Updated: 2026-10-16  
Current Milestone: **M1 — Diagnostics & Lexer** (In Progress)  
Platform: **Native Windows (MinGW-w64 UCRT GCC 16.2 / Clang 23.1 + CMake 4.4 + Ninja 1.13)**

---

## 1. Toolchain & Environment Status

| Tool | Status | Version / Path |
|---|---|---|
| **GPU** | VERIFIED | NVIDIA GeForce RTX 5070 Ti Laptop GPU |
| **Compute Capability** | VERIFIED | sm_120 (CC 12.0) |
| **NVIDIA Driver** | VERIFIED | 591.91 (supports CUDA 13.1 driver API) |
| **C++ Compiler** | VERIFIED | MinGW-w64 GCC 16.2.0 (`g++`) + LLVM Clang++ 23.1.3 |
| **Build Tools** | VERIFIED | CMake 4.4.4, Ninja 1.13.2 |
| **GoogleTest** | VERIFIED (FetchContent) | v1.15.2 pinned |
| **ptxas (PTX Assembler)**| UNVERIFIED | Not yet installed / PATH; deferred until CUDA Toolkit component setup |

---

## 2. Milestone Progress

| Milestone | Description | Status | Verification Evidence |
|---|---|---|---|
| **M0** | Environment, toolchain audit, CMake/GTest skeleton | **COMPLETED** | CMake 4.4 + Ninja + GCC 16.2 builds `kcc.exe` & `unit_tests.exe`; CTest 2/2 Passed (100%); Concept questions verified |
| **M1** | Diagnostics & Lexer | **COMPLETED** | SourceManager + DiagnosticEngine + DFA Lexer; 39/39 CTest passed (100%); `kcc --dump-tokens` verified on `saxpy.kc` |
| **M2** | Parser, AST, and gate-lab (LL(1)/FIRST/FOLLOW) | Next | Recursive descent parser, AST nodes, panic mode recovery, gate-lab FIRST/FOLLOW tool |
| **M3** | Sema & AST Reference Interpreter (O1) | Pending | — |
| **M4** | Typed TAC IR, CFG, Verifier & IR Interpreter (O2) | Pending | — |
| **M5** | PTX Emitter & ptxas validation | Pending | — |
| **M6** | GPU Runner (O4) via CUDA Driver API | Pending | — |
| **M7** | PTX-subset Parser, Validator & Interpreter (O3) | Pending | — |
| **M8** | Data-flow Framework & Optimizations (CP, DCE, CSE)| Pending | — |
| **M9** | Multi-Oracle Differential Testing Harness (ptxdiff)| Pending | — |
| **M10**| Generator, Reducer & Mutation Testing | Pending | — |
| **M11**| Evaluation & Final Presentation | Pending | — |

---

## 3. Known Limitations & Open Issues
* `ptxas` is not yet installed in PATH; offline PTX assembly verification will remain marked UNVERIFIED until the toolkit component is set up.
* MinGW binaries require MinGW `bin/` in PATH or `-static-libstdc++ -static-libgcc` to run outside development terminals.

# PTXLab — A Verified Mini-Compiler Targeting PTX
### Project plan for GATE Compiler Design revision + NVIDIA PTX Compiler internship preparation

**Labels used throughout**
- **[DOC]** — stated in documentation I actually opened during research (sources in Appendix A).
- **[REC]** — my engineering recommendation or judgment (not a fact about NVIDIA's tools).
- **[VERIFY]** — I could not confirm it here; a verification procedure is given. Do not treat as fact until checked.

---

## 1. Executive recommendation

**Do not build MiniC-PTX as proposed. Build a hybrid, "PTXLab", whose centerpiece is verification, not feature count.**

PTXLab = a small C-like kernel language (**KC**) compiled by a hand-written C++ compiler (`kcc`) through a typed three-address-code CFG IR with real data-flow optimizations to PTX text, **plus** an independent PTX-subset parser/validator/reference interpreter (`ptxi`) and a **differential testing harness** (`ptxdiff`) that checks every program through up to five independent execution paths:

| Oracle | What it executes | Needs GPU? | Independence |
|---|---|---|---|
| O1 | KC AST interpreter (defines source semantics) | no | baseline |
| O2 | IR interpreter, before and after each optimization pass | no | checks lowering and passes |
| O3 | `ptxi` executing the **emitted PTX text** via a separate parser | no | checks the PTX back end without a GPU |
| O4 | Real GPU via CUDA Driver API (`cuModuleLoadData` JIT → `ptxas`) | yes | the only fully external oracle |
| O5 (stretch) | KC→C++ transpile, compiled with g++/clang `-O0 -fwrapv -ffp-contract=off` | no | mature external compiler |

**Why this beats the original idea [REC]:**
1. Pure MiniC-PTX is the project a skeptical reviewer has seen many times (lexer → parser → TAC → "PTX"). The differentiator is *how you know it is correct*.
2. The job emphasizes the **PTX ISA, PTX front end, and interaction with optimizers/runtime**. A PTX parser + validator + interpreter is literally a PTX front end; a compiler that emits PTX is the producer side of the same interface. Doing both gives you both sides of the contract.
3. GATE coverage stays high because the compiler half implements lexing, LL(1) parsing, syntax-directed translation, TAC, CFGs, and bit-vector data-flow analyses.
4. Everything except O4 works with **no GPU and no CUDA install**, which de-risks the environment.

**Honest timeline verdict [REC]:** The full hybrid in 4 weeks is aggressive. I assume ~20–25 focused hours/week. The plan therefore has a hard **MVP at end of Week 3** and Week 4 is hardening/evidence/documentation. If you have fewer hours, stretch to 6 weeks rather than cutting verification. Cut order is in §10.

**What this is not:** it is not NVIDIA's compiler, does not implement the full PTX ISA, and must never claim PTX/CUDA compatibility beyond the tested subset.

---

## 2. Weighted comparison of candidate projects

### Weights (sum = 100) and reasoning [REC]

| Criterion | Weight | Why this weight |
|---|---|---|
| Relevance to PTX Compiler internship | 25 | Primary goal |
| Interview discussion value | 15 | The project exists to be discussed |
| Feasibility in 4 weeks | 15 | A half-done project is worth little |
| Learning value / conceptual depth | 10 | Core stated need |
| GATE Compiler Design coverage | 10 | Secondary stated goal |
| Objectively measurable results | 10 | You want defensible claims |
| Environment/dependency risk (5 = low risk) | 10 | Windows + new Blackwell GPU raises risk |
| Likelihood of polished deliverable | 5 | Partly captured by feasibility |

### Scores (1–5; my judgment, not measured)

| Option | Rel. (25) | Interview (15) | Feas. (15) | Depth (10) | GATE (10) | Measur. (10) | Env (10) | Polish (5) | **Total /100** |
|---|---|---|---|---|---|---|---|---|---|
| A MiniC-PTX | 4 | 4 | 3 | 4 | 5 | 4 | 3 | 4 | **77** |
| B Optimizer Lab | 2 | 3 | 5 | 4 | 5 | 4 | 5 | 5 | **75** |
| C PTXRef | 5 | 5 | 3 | 4 | 2 | 5 | 4 | 4 | **83** |
| D MiniLink | 3 | 4 | 3 | 4 | 1 | 4 | 4 | 3 | **65** |
| E MiniDbg | 2 | 3 | 2 | 4 | 1 | 3 | 2 | 3 | **48** |
| **H PTXLab (A+B+C hybrid)** | 5 | 5 | 3 | 5 | 4 | 5 | 4 | 3 | **86** |

Total = Σ(weight × score) / 5.

**Reading the table**
- **C beats A** under these weights: PTX-side validation is more role-relevant than a generic front end. But C alone gives little GATE revision (GATE does not cover PTX), so it fails your first goal.
- **B** is the safest and best GATE revision, but has little PTX relevance; it becomes a *component* of H.
- **D (MiniLink)** matches "object files, linkers, loaders" from the role but is far from GATE and needs Linux ELF test inputs; good *extension* (see §10, stretch S4), not the core.
- **E (MiniDbg)** is Linux-`ptrace`/DWARF heavy, poor Windows fit, and the weakest GATE match. Rejected.
- H's feasibility score is a 3, not a 4–5: that is the main risk, handled by milestone gating and a scope-cut order.
- Scores are subjective. Change the weights and re-total; if you weight GATE ≥ 25 and relevance ≤ 10, B/A overtake. Decide whether that is really your priority.

---

## 3. Selected project and justification

### 3.1 Specification

- **Name:** PTXLab
- **Components:** `kcc` (compiler), `ptxi` (PTX-subset validator + interpreter), `ptxdiff` (differential tester + fuzzer + reducer), `gate-lab` (grammar tools: FIRST/FOLLOW/LL(1) table checker)
- **Objective:** compile a small, fully specified GPU-kernel language to PTX; **demonstrate** correctness with multi-oracle differential testing, optimizer mutation testing, and ptxas/GPU validation.
- **Target user:** a student/engineer who wants a compact, inspectable reference for the PTX-emission contract and a test methodology for it.
- **Technical contribution:** (a) explicit, tested semantics for a KC subset, (b) an independent PTX-subset front end, (c) optimizer verified by semantic-equivalence tests, (d) a mutation-testing result showing the harness catches seeded bugs.

### 3.2 MVP (smallest version worth publishing)

End of Week 3 [REC]:
- KC front end (lexer, recursive-descent parser, sema, diagnostics with line:col).
- AST interpreter (O1), IR + CFG + IR interpreter (O2).
- PTX emitter for the full MVP language subset; output accepted by `ptxas` (if installed).
- `ptxi` parse + validate + interpret emitted PTX (O3).
- GPU runner via Driver API (O4) *if environment verifies*; otherwise documented as not-run.
- Data-flow framework with: constant folding, global constant propagation (lattice), branch folding + unreachable-block removal, liveness-based DCE, block-local CSE (value numbering).
- Differential test across O1/O2/O3(/O4) on hand-written and randomly generated programs.
- README with build instructions, limitations, and measured results.

### 3.3 Feature prioritization

| Tier | Feature |
|---|---|
| **Essential** | Hand-written lexer; recursive-descent parser; types `i32 f32 bool` and pointers; sema with typed AST; diagnostics; AST interpreter; TAC IR + CFG + verifier; IR interpreter; PTX emitter; PTX-subset parser/validator/interpreter; differential harness; liveness; const fold/prop; DCE; local CSE; unit + negative + golden tests |
| **Optional (if on schedule)** | GPU runner (O4) — essential if env works; random program generator; reducer; reaching definitions (analysis + verifier use); mutation testing; benchmarks vs `nvcc -ptx` |
| **Stretch** | Global CSE via available expressions; copy propagation; `.file/.loc` debug line directives checked with `cuobjdump`; KC→C++ oracle (O5); SLR/LR(0) item-set tool in `gate-lab`; regex→NFA→DFA demo in `gate-lab`; `nvPTXCompiler`/`cuobjdump -elf` cubin inspection |
| **Deferred / rejected** | SSA construction, physical register allocation (PTX uses virtual registers; `ptxas` allocates), loop optimizations (LICM), function calls, structs, local arrays, shared memory/barriers/atomics, tensor cores, linking, DWARF, parser generators, LLVM backend |

Rationale for rejecting the heavy items [REC]: each adds either toolchain risk or a semantics surface (memory model, barriers) that would consume the time needed to *verify* what already exists.

### 3.4 Architecture decisions and trade-offs [REC]

| Decision | Chosen | Alternative | Why |
|---|---|---|---|
| Lexer/parser | Hand-written lexer + recursive descent | Flex/Bison, ANTLR | Interview-explainable; no tool dependency; GATE tools (FIRST/FOLLOW) covered separately in `gate-lab` |
| IR | Custom typed TAC over virtual regs, non-SSA, explicit CFG | LLVM IR | Maps 1:1 to PTX virtual registers; GATE data-flow is on non-SSA TAC; avoids LLVM build burden. Trade-off: not industry-standard; SSA noted as future work |
| PTX generation | Direct from IR | Via LLVM NVPTX | Direct emission teaches the PTX contract |
| Reference | Three CPU oracles + GPU | Single interpreter | One interpreter written by the same author shares misconceptions; the GPU (and optional C++ oracle) is the independent check |
| Test mode | CPU-only by default; `gpu` CTest label | GPU required | Environment risk |
| Language | C++20, CMake, GoogleTest | other | You chose C++; matches role |

### 3.5 Simulated technical review → revisions made

| Reviewer question | Finding | Revision |
|---|---|---|
| Too broad for 4 weeks? | Yes, if all five options are built | Scope = KC compiler + *subset-only* PTX parser (only what emitter emits + a hand-written test corpus), not general PTX |
| Is validation stronger than generation? | Validation is more role-relevant, generation is more GATE-relevant | Hybrid; PTX parser is the independent oracle |
| Educational vs relevant vs impressive? | Global CSE, SSA, regalloc are "impressive" but add little role value | Moved to stretch/deferred |
| Distinct from a tutorial? | Only if verification is real | Added mutation testing, 3–5 oracles, UB-avoiding generator, semantic spec doc |
| Skeptic's criticisms | "Your interpreter and compiler share bugs"; "float tests are sloppy"; "`add.f32` may be fused" | O3 parser shares no code with emitter; GPU + optional C++ oracle; emit `.rn` modifiers; bit-exact float comparison for `+ - * /` |
| First three interviewer questions | See §11 | Each has a code artifact backing it |
| Claims defensible with evidence | Only what experiments E1–E7 show | Resume wording template leaves numbers blank until measured |
| What to remove for polish | Loops optimizations, CSE-global, SSA | Deferred |

---

## 4. GATE topic → implementation mapping

I describe the current GATE CS Compiler Design syllabus from memory as: lexical analysis, parsing, syntax-directed translation, runtime environments, intermediate code generation, local optimization, data-flow analyses (constant propagation, liveness analysis, common subexpression elimination). **[VERIFY]** against the current official GATE brochure for your exam year. Anything marked "beyond GATE" is engineering depth, not syllabus.

| GATE topic | In syllabus? | Implementation task | File(s) | Test |
|---|---|---|---|---|
| Lexical analysis, regular expressions, DFA | Yes | Hand-written DFA-style lexer; document the token DFA in `docs/lexer-dfa.md`. Stretch: regex→NFA→DFA subset construction demo | `kc/lexer.cpp`, `tools/gate-lab/regex_dfa.cpp` | token streams, invalid chars, unterminated comments |
| CFG, derivations, ambiguity | Yes | Write KC grammar; demonstrate dangling-else ambiguity and its resolution | `docs/grammar.md` | gate-lab reports the conflict |
| FIRST/FOLLOW, LL(1) | Yes | `gate-lab` computes FIRST/FOLLOW/LL(1) table for the KC grammar (after left-recursion removal and left factoring) and reports conflicts | `tools/gate-lab/` | table equals hand-computed expected; conflict test on a known non-LL(1) grammar |
| Recursive descent / predictive parsing | Yes | Parser; expressions via one function per precedence level (equivalent to the left-recursion-eliminated grammar) | `kc/parser.cpp` | golden AST dumps, error recovery tests |
| LR/SLR/LALR parsing | Yes | Stretch only: SLR(1) item sets/table for a small expression grammar. Honest note: KC parser is **not** LR | `tools/gate-lab/slr.cpp` | known textbook grammar |
| Syntax-directed translation, attributes | Yes | Typed AST with synthesized types; short-circuit boolean lowering using true/false jump lists with backpatching | `kc/sema.cpp`, `ir/lower.cpp` | golden IR for `&&`, `\|\|` |
| Symbol tables, scopes, type checking | Yes | Scoped symbol table, no implicit conversions, definite initialization via required initializers | `kc/sema.cpp` | negative tests: undeclared, redeclared, type mismatch |
| Runtime environments / activation records | Yes (syllabus) | **Only partially covered.** KC has no calls. Map: PTX `.param` space (kernel args), `.reg` virtual regs, state spaces. Optional extra: write a short doc + tiny stack-frame simulator separate from the project | `docs/runtime-env.md` | n/a — do not claim full coverage |
| Intermediate code, three-address code | Yes | Typed TAC, quadruple-style struct, textual printer/parser | `ir/` | golden IR |
| Basic blocks, CFG, leaders | Yes | Leader algorithm in IR builder verifier path; CFG edges, dominators optional | `ir/cfg.cpp` | CFG golden dumps |
| Local optimization | Yes | Constant folding, algebraic identities, local value-numbering CSE | `opt/` | semantic-equivalence tests |
| Data-flow analysis framework | Yes | Generic bit-vector solver: direction, meet, gen/kill, worklist | `analysis/dataflow.h` | textbook examples with hand-computed IN/OUT |
| Reaching definitions | Yes | Forward ∪ | `analysis/reaching_defs.cpp` | textbook CFGs; verifier "use before def" |
| Liveness | Yes | Backward ∪; used by DCE | `analysis/liveness.cpp` | textbook CFGs |
| Constant propagation | Yes | Flat-lattice (⊤/const/⊥) global propagation + branch folding | `opt/constprop.cpp` | equivalence + known-result CFGs |
| Common subexpression elimination | Yes | Local value numbering (MVP); available expressions (stretch) | `opt/cse.cpp` | |
| Code generation, instruction selection | Partially in GATE | IR → PTX mapping table | `ptx/emit.cpp` | ptxas + O3 |
| Register allocation | Light in GATE | **Not implemented**: PTX registers are virtual; `ptxas` allocates. Say this explicitly in interviews | — | — |
| SSA, dominators, advanced opts | Beyond GATE | Deferred | — | — |
| PTX ISA, state spaces, JIT, driver API, ELF/cubin | Beyond GATE | Core role content | `ptx/`, `runner/` | — |

---

## 5. Architecture and repository structure

### 5.1 Data flow

```
 KC source ──► Lexer ──► Parser ──► AST ──► Sema ──► Typed AST ──┬──► AST Interpreter (O1)
                                                                  │
                                                                  ▼
                                              IR Lowering ──► IR (TAC + CFG) ──► Verifier
                                                                  │                │
                                                                  │                ├──► IR Interpreter (O2)
                                                                  ▼
                                               Optimizer pipeline (analysis + passes)
                                                                  │
                                                                  ▼
                                                           PTX Emitter ──► kernel.ptx
                                                                              │
                          ┌───────────────────────────────────────────────────┼─────────────────────┐
                          ▼                                                   ▼                     ▼
                  ptxas (offline validation)                    ptxi: parse+validate+interp (O3)   GPU runner (O4)
                                                                                                    cuModuleLoadData
     ptxdiff: generates programs + inputs ──► runs O1..O4(..O5) ──► compares ──► reports / reduces failures
```

### 5.2 Components, responsibilities, interfaces

| Component | Responsibility | Public interface (sketch) |
|---|---|---|
| `support` | `SourceManager`, `Diagnostic` (file:line:col, caret), `Arena`, error-collecting sink | `DiagEngine::error(loc, msg)` |
| `kc` | Lexer, parser, AST, sema | `Lexer(src)→tokens`, `Parser→Program`, `Sema(Program)→TypedProgram` |
| `interp` | KC AST interpreter, IR interpreter, shared `Memory`/`LaunchConfig` model | `run(kernel, args, grid, block, Memory&)→Result{ok|UB(msg)}` |
| `ir` | Types, instructions, basic blocks, CFG, builder, text printer/parser, verifier | `lower(TypedProgram)→IRModule` |
| `analysis` | Dataflow solver + RD, liveness, (avail-exprs) | `solve<Problem>(Function&)` |
| `opt` | Passes + pipeline `-O0/-O1`, each pass returns `changed` | `Pass::run(Function&)→bool` |
| `ptx` | Emitter (`ptx/emit`), **separate** PTX-subset lexer/parser/validator/interpreter (`ptx/ptxi`) | emitter outputs `std::string`; ptxi consumes only text |
| `runner` | Driver-API host harness (optional CMake target) | `launch(ptx_text, name, args...)→buffers` |
| `fuzz` | Random KC generator (UB-free by construction), reducer, differential driver | `ptxdiff --seed N --count M` |
| `tools/gate-lab` | FIRST/FOLLOW/LL(1), optional SLR/regex tools | CLI |

**Independence rule [REC]:** `ptx/ptxi` must not `#include` anything from `ptx/emit` or `ir/`. If code is shared, it lives in `support/` and is trivial (string utils, bit casts).

### 5.3 Repository tree

```
ptxlab/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md                 # results, limitations, quick start
├── LICENSE
├── docs/
│   ├── kc-language-spec.md   # grammar + normative semantics (UB list)
│   ├── ir-spec.md
│   ├── ptx-subset.md         # exactly which PTX the emitter produces/ptxi accepts, with ISA section refs
│   ├── lexer-dfa.md  grammar.md  runtime-env.md
│   ├── gate-map.md           # topic → file/test traceability
│   ├── design/               # one design note per milestone (written BEFORE code)
│   ├── learning-log.md       # your explain-back answers
│   ├── STATUS.md             # implemented / limitations / open issues / next
│   └── evaluation.md         # experiments, commands, raw results
├── include/ptxlab/           # public headers
├── src/
│   ├── support/  kc/  ir/  analysis/  opt/  ptx/  interp/  runner/  fuzz/
│   └── tools/                # kcc.cpp  ptxi.cpp  ptxdiff.cpp
├── tools/gate-lab/
├── tests/
│   ├── unit/                 # GoogleTest per component
│   ├── golden/               # *.kc + expected .tokens/.ast/.ir/.ptx
│   ├── negative/             # *.kc + expected diagnostics
│   ├── programs/             # corpus: vec_add, saxpy, clamp, abs, gcd-like loops, etc.
│   ├── regress/              # one file per fixed bug
│   └── ptx_corpus/           # hand-written PTX for ptxi (valid + invalid)
├── scripts/
│   ├── check_env.sh  check_env.ps1
│   ├── run_all_tests.sh  run_eval.sh
└── .github/workflows/ci.yml  # CPU-only CI (gpu label excluded)
```

---

## 6. Milestone-by-milestone implementation plan

**Rules for every milestone** (enforced in the Gemini prompt): write `docs/design/Mx.md` first → implement one bounded feature at a time → build + test after each change → record exact commands/results → keep `main` green → explain-back questions answered by you before the milestone is closed.

### M0 — Environment and skeleton
- **Inputs:** none. **Outputs:** repo tree, CMake builds an empty `kcc`, GoogleTest runs 1 trivial test, `scripts/check_env.*`.
- **Files:** `CMakeLists.txt`, `CMakePresets.json`, `scripts/check_env.*`, `docs/STATUS.md`.
- **Tests:** `ctest` passes; `check_env` prints a table of tool → found/version/not-found.
- **Done when:** clean clone builds with documented commands; env report saved to `docs/env-report.txt`; decision recorded: WSL2 vs native Windows.

### M1 — Diagnostics + lexer
- **Implement:** `SourceManager`, `Diagnostic`, token kinds, lexer (identifiers, i32/f32 literals, operators, `//` and `/* */`, line:col).
- **Tests:** token golden files; invalid char; unterminated comment; integer overflow literal; float forms (`1.0`, `.5`, `1e3`, `1.0f` decision documented).
- **Done when:** ≥ 25 lexer tests pass; every error has line:col.

### M2 — Parser + AST + gate-lab
- **Implement:** KC grammar (see Gemini prompt §C), recursive-descent parser with panic-mode recovery to `;`/`}`, AST printer; `gate-lab` computing FIRST/FOLLOW/LL(1) conflicts.
- **Tests:** golden ASTs; precedence/associativity; dangling else; syntax-error recovery (multiple errors reported); gate-lab on textbook grammars with hand-computed expected sets.
- **Done when:** parse tests + gate-lab tests pass; `docs/grammar.md` lists the grammar and states which rules were transformed.

### M3 — Sema + AST interpreter (O1)
- **Implement:** scoped symbol table, type rules (no implicit conversion; explicit `i32(x)`, `f32(x)`), definite initialization via mandatory initializer, builtin calls, return rules; AST interpreter with a *trap on UB* (div by 0, `INT_MIN/-1`, shift count ∉ [0,31], OOB access, float→int out of range/NaN, step limit exceeded).
- **Tests:** ≥ 20 negative sema tests with exact messages; interpreter tests for every operator; UB-trap tests.
- **Done when:** `kcc --run-ast prog.kc --grid .. --block .. --buf ..` produces expected output on the hand-written corpus.

### M4 — IR, lowering, CFG, verifier, IR interpreter (O2)
- **Implement:** IR as specified in the Gemini prompt §D; lowering with jumping code/backpatching for `&&`, `||`, `!`, comparisons in conditions; `elemaddr` explicit; CFG construction; IR verifier (types, one terminator per block, defined-before-use on all paths using reaching definitions or a simple dataflow check); IR text printer; IR interpreter.
- **Tests:** golden IR; verifier negative tests (hand-built bad IR); **differential O1 vs O2** on the corpus.
- **Done when:** O1 == O2 on all corpus programs and inputs.

### M5 — PTX emitter + ptxas validation
- **Implement:** the mapping table in §D of the prompt. First straight-line kernels (vec_add), then conditionals, then loops. Virtual-register naming by type: `%p` (pred), `%r` (b32/s32), `%f` (f32), `%rd` (b64). `.reg` declarations counted from actual use.
- **Tests:** golden PTX; if `ptxas` available: `ptxas -arch=sm_75 file.ptx -o /dev/null` (or `NUL` on Windows) **must succeed for every corpus program** — test labeled `ptxas`.
- **Done when:** corpus PTX accepted by ptxas (or documented as unverifiable, with reason).

### M6 — GPU runner (O4) — *skip to M7 if env fails; revisit later*
- **Implement:** `runner` using Driver API: `cuInit`, `cuDeviceGet`, `cuCtxCreate`, `cuModuleLoadDataEx` (with info/error log buffers), `cuModuleGetFunction`, `cuMemAlloc`, `cuMemcpyHtoD/DtoH`, `cuLaunchKernel`, `cuCtxSynchronize`, all return codes checked. Links `CUDA::cuda_driver`; **no nvcc required**.
- **Tests (label `gpu`):** vec_add, saxpy, clamp on GPU equal O1 results.
- **Done when:** GPU outputs == O1 outputs on ≥ 5 corpus programs; JIT log captured.

### M7 — PTX-subset parser, validator, interpreter (O3)
- **Implement:** independent lexer/parser for the subset in `docs/ptx-subset.md`; validator (version/target/address_size present; every register declared and used with a compatible type; labels defined once and referenced labels exist; instruction type suffix matches operand register types; entry params consistent); interpreter with predicated execution, special registers, byte-addressed global memory, params.
- **Tests:** `ptx_corpus/valid/*.ptx` accepted; `invalid/*.ptx` rejected with specific messages; **differential O1 == O2 == O3**; hand-written PTX cross-checked against ISA doc.
- **Done when:** O3 matches O1 on the corpus and on 1,000 random programs (after M10; before that on the corpus).

### M8 — Data-flow framework and optimizations
- **Implement (in order, each its own commit + tests):** (1) bit-vector solver; (2) liveness; (3) reaching definitions; (4) constant folding + algebraic simplification (strict 32-bit wraparound using `uint32_t`; float folding in strict binary32); (5) lattice constant propagation + branch folding + unreachable-block removal; (6) DCE using liveness (never remove stores or special-register reads that are needed for semantics; document purity table); (7) local value numbering CSE for pure ops including `elemaddr` (**loads/stores never CSE'd in MVP**).
- **Tests:** textbook CFGs with hand-computed IN/OUT; per-pass before/after golden IR; **every pass validated by O2-before == O2-after on corpus + random inputs**; negative: pass must not fold `x/0`, must not move/remove stores.
- **Done when:** `-O1` pipeline passes all corpus programs; `--dump-ir-after=<pass>` works.

### M9 — Differential harness
- **Implement:** `ptxdiff` runs a program through selected oracles, compares outputs (ints exact; floats **bit-exact** for `+ - * /` and comparisons; NaN/Inf runs excluded and counted), reports first mismatching element, writes a repro bundle (`.kc`, inputs, outputs per oracle).
- **Tests:** harness catches a deliberately wrong oracle (inject bug via test hook).
- **Done when:** one command runs the corpus across all available oracles at -O0 and -O1.

### M10 — Random generator, reducer, mutation testing
- **Implement:** UB-free generator by construction (see §8.3), bounded loops only, guarded memory access, divisor constants nonzero & ≠ −1, shift counts masked `& 31`, no float→int casts of unbounded values (clamp first) and floats restricted to ranges avoiding overflow/NaN; reducer: statement deletion, expression replacement by operands/constants, delta-debugging loop, re-check "still mismatches". Mutation framework: compile-time flag `PTXLAB_MUTANT=<id>` toggles seeded bugs (e.g., liveness forgets a use, constprop ignores a loop back-edge, `shr` emitted as `shr.u32`, signed compare emitted unsigned, CSE across a redefinition).
- **Tests:** generator output always passes sema; reducer shrinks a mutant failure to a small program.
- **Done when:** ≥ 10,000 generated programs × ≥ 4 inputs show 0 mismatches on the unmutated build; each mutant is killed or documented as equivalent/unkilled.

### M11 — Evaluation, documentation, presentation
- **Implement:** `scripts/run_eval.sh` reproducing E1–E7 (§8.4), `docs/evaluation.md` with raw numbers, README, diagrams, demo script, `docs/gate-map.md`, interview notes.
- **Done when:** a fresh clone reproduces numbers with documented commands; README states limitations plainly.

### Stretch milestones (only after M11)
- **S1** global CSE via available expressions. **S2** copy propagation. **S3** `.file/.loc` emission + check with `ptxas -lineinfo` and `cuobjdump`. **S4** inspect cubin ELF (`cuobjdump -elf`) and write a minimal ELF section/symbol dumper (bridges to MiniLink). **S5** KC→C++ oracle (O5). **S6** SLR table tool.

---

## 7. Environment and dependency verification

**Principle [REC]:** Only `ptxas` and the CUDA driver are needed for GPU validation; `nvcc` and a host-compiler/MSVC pairing are **not** required by the plan. This sidesteps the most common Windows CUDA pain.

### 7.1 Platform recommendation [REC]
Primary: **WSL2 + Ubuntu 24.04** (Linux toolchain, same environment style as NVIDIA's tooling docs; GPU access works through the Windows driver). Fallback: native Windows (MSVC + CMake + CUDA Toolkit). Decide in M0 and record it.

### 7.2 Probe commands

Windows PowerShell:
```powershell
nvidia-smi
nvidia-smi --query-gpu=name,driver_version,compute_cap --format=csv   # compute_cap may be unsupported on old drivers
wsl --status
wsl -l -v
```
Inside WSL (or native Linux):
```bash
nvidia-smi                      # must show the RTX 5070 Ti; do NOT install a Linux NVIDIA display driver inside WSL
g++ --version; clang++ --version; cmake --version; ninja --version; git --version; python3 --version
ptxas --version; nvcc --version; cuobjdump --version; nvdisasm --version
ls /usr/local/cuda*/bin 2>/dev/null
```

### 7.3 Facts and expectations
- **[DOC]** PTX ISA 8.7 adds support for the `sm_120` target; NVIDIA's CUDA platform guide uses CC 12.0 ↔ `sm_120` as an example. **[VERIFY]** that your RTX 5070 Ti Laptop reports compute capability 12.0 (`nvidia-smi` query or `cuDeviceGetAttribute`). I expect it to be Blackwell-class but did not confirm the exact reported value.
- **[DOC]** PTX for a lower compute capability can be JIT-compiled at runtime for equal-or-higher capability (the guide's example: compute_80 PTX JIT-compiled to sm_120). **[REC]** therefore emit `.target sm_75` by default (portable) and add a `--target sm_120` option. **[VERIFY]** that your installed `ptxas` accepts `sm_75` (CUDA 13 era builds still list it as lowest per one build script I saw, not NVIDIA documentation).
- **[VERIFY]** which toolkit you need for `ptxas -arch=sm_120`: PTX 8.7 introduced `sm_120`, which implies CUDA 12.8 or later. Run `ptxas --version` and `ptxas -arch=sm_120 test.ptx` on a trivial PTX file.
- **[DOC]** `cuobjdump` and `nvdisasm` inspect cubin/ELF; `cuobjdump -ptx/-sass/-elf` options exist; `nvdisasm` accepts only cubins.
- **Installation:** install the CUDA Toolkit from NVIDIA's download page choosing the WSL-Ubuntu variant for WSL (do not copy commands from memory; use the page's current repo instructions). **[VERIFY]** version after install.

### 7.4 Minimal PTX smoke test (create in M0)
```ptx
.version 7.0
.target sm_75
.address_size 64
.visible .entry smoke() { ret; }
```
`ptxas -arch=sm_75 smoke.ptx -o smoke.cubin` should succeed. If `.version 7.0` is rejected, find the minimum accepted version by trial and record it. **[VERIFY]**

### 7.5 Fallback matrix

| Missing | Consequence | Fallback |
|---|---|---|
| GPU/driver unusable in WSL | no O4 | run O1–O3; keep GPU tests labeled `gpu`; document; try native Windows runner build |
| `ptxas` | no offline validation | use JIT error log from `cuModuleLoadDataEx` (needs driver) or rely on `ptxi` validator; mark claims "validated by ptxi only" |
| CUDA Toolkit headers (`cuda.h`) | runner won't build | CMake option `PTXLAB_ENABLE_GPU=OFF`; or load `libcuda`/`nvcuda.dll` dynamically (stretch) |
| sm_120 unsupported by installed toolkit | cannot target sm_120 offline | stay on sm_75 PTX; let driver JIT upward |
| No network for GoogleTest | build fails | vendor a pinned copy in `third_party/` |

---

## 8. Testing and evaluation methodology

### 8.1 Test layers

| Layer | What | Pass criterion |
|---|---|---|
| Unit | lexer, parser, sema, IR builder, verifier, each analysis, each pass, PTX parser/validator | GoogleTest green |
| Golden | `.kc` → tokens/AST/IR/PTX text | exact match; `--update-golden` flag reviewed by hand |
| Negative | malformed KC, bad IR, bad PTX | exact diagnostic code/message and line:col |
| Integration | KC → PTX → ptxas | ptxas exit 0 |
| Differential | O1/O2/O3/(O4/O5), -O0 vs -O1 | outputs equal under comparison rules below |
| Regression | every fixed bug gets a file in `tests/regress/` | green forever |
| Data-flow | textbook CFGs with hand-computed fixed points | exact IN/OUT sets |

### 8.2 Semantics that make differential testing valid (normative; put in `docs/kc-language-spec.md`)

- `i32`: two's-complement wraparound for `+ - *` and unary `-` (implement in C++ via `uint32_t` to avoid host UB).
- `/`, `%`: truncation toward zero. **UB** if divisor is 0 or `INT_MIN / -1`. [VERIFY PTX behavior for these; the plan avoids relying on it].
- `<<`, `>>`: **UB** if shift count ∉ [0, 31]. `>>` on `i32` is arithmetic. (PTX clamps large shift counts **[VERIFY in ISA]**; avoiding the case makes both sides agree.)
- `f32`: IEEE-754 binary32, round-to-nearest-even; no fast-math; **no FMA contraction** — emit `add.rn.f32`, `mul.rn.f32`, `sub.rn.f32`, `div.rn.f32`. (PTX documents that un-suffixed add/mul may be contracted; **[VERIFY]** wording in the "Floating Point Instructions" section.) Host reference built with strict FP (`-ffp-contract=off`, SSE2, no `-ffast-math`).
- Subnormals: default non-FTZ **[VERIFY]**; generator keeps magnitudes away from subnormals.
- NaN/Inf: programs that produce them are excluded from comparison and counted.
- Float comparison for `+ - * /`: **bit-exact** (correctly rounded operations are deterministic). Any transcendental support (not planned) would need a ULP tolerance with a documented bound.
- Memory: OOB is UB (interpreters trap). Threads are independent in MVP (no shared memory/barriers/atomics) and each thread writes only to indices derived from its own `gid`, so **sequential interpreter execution of threads is equivalent to parallel GPU execution**. State this assumption in the spec; the generator enforces it.
- Uninitialized reads cannot occur (initializer required).
- Loops: interpreter has a step limit; generated loops are bounded by constants.

### 8.3 Generator rules (UB-free by construction)
- Every memory access is guarded: `if (gid < n) { out[gid] = expr; }`.
- Only reads from `in*[gid]` or `in*[(gid + c) % n]` where `n > 0` is a launch constant.
- Divisors: positive literals ≥ 2, or `((e & 7) + 1)`.
- Shift counts: literals 0–31 or `(e & 31)`.
- Float values: inputs drawn from `[-100, 100]`; avoid division by expressions that can be 0 (use `(abs(e)+1.0)` pattern or literal).
- Float→int casts only of values clamped into a safe range first.
- Loops: `for (i32 i = 0; i < K; i = i + 1)` with literal K ≤ 16, `i` not assigned in the body.

### 8.4 Experiments (report only what you measure)

| ID | Question | Method | Reported metric |
|---|---|---|---|
| E1 | Does the compiler agree across oracles? | `ptxdiff --count 10000 --inputs 4` at -O0 and -O1 | mismatches (target 0), programs excluded for NaN/Inf |
| E2 | Does the harness detect bugs? | Seed ≥ 10 mutants (§M10); run ptxdiff | kill rate; list of unkilled with reasons |
| E3 | What do passes change? | Static IR instruction count -O0 vs -O1 over corpus | table; plus `ptxas -v` register counts (note: ptxas re-optimizes, so differences may shrink — report honestly) |
| E4 | Is emitted PTX accepted by the toolchain? | `ptxas` on all corpus + sample of generated programs, sm_75 (and sm_120 if supported) | acceptance rate (target 100%) |
| E5 | Structural comparison with nvcc | Hand-port 3–5 corpus kernels to CUDA C++, `nvcc -ptx`, compare instruction mix | qualitative table; no performance claims |
| E6 | Compiler scalability | Time `kcc` on synthetic programs of growing size | compile time vs statements |
| E7 | Reducer quality | Reduce failures produced by mutants | original vs reduced size |

Do **not** report GPU kernel speedups from tiny kernels as evidence; they measure launch overhead.

---

## 9. Four-week schedule

Assumes ~20–25 h/week and that you also write the explain-back answers. If behind at a gate, apply the cut order in §10.

| Week | Days | Deliverables | Gate |
|---|---|---|---|
| **1 — Front end + semantics** | 1–2 | M0 env, skeleton, smoke PTX/ptxas test | `ctest` green; env report |
| | 3 | M1 lexer + diagnostics | 25+ lexer tests |
| | 4–5 | M2 parser/AST + gate-lab FIRST/FOLLOW | parse goldens; gate-lab tests |
| | 6–7 | M3 sema + AST interpreter | O1 runs corpus |
| **2 — IR + PTX out** | 8–9 | M4 IR, lowering, verifier, IR interp | O1 == O2 |
| | 10–11 | M5 PTX emitter + ptxas | corpus accepted by ptxas |
| | 12–13 | M6 GPU runner (or defer) | GPU == O1 on ≥ 5 programs |
| | 14 | Buffer + STATUS/doc catch-up | — |
| **3 — Independent oracle + optimizer (MVP)** | 15–17 | M7 PTX parser/validator/interpreter | O1 == O2 == O3 |
| | 18–20 | M8 dataflow + opts (liveness → CP → DCE → CSE) | each pass equivalence-tested |
| | 21 | M9 harness | one-command differential run — **MVP complete** |
| **4 — Evidence + polish** | 22–24 | M10 generator, reducer, mutants | 10k programs, mutation table |
| | 25–26 | M11 experiments E1–E7, README, diagrams | reproducible numbers |
| | 27–28 | Interview prep, demo script, stretch S1 or S3 only if all green | — |

Realism check [REC]: M7 and M8 are the highest-risk blocks (≈ 3 days each). If either slips past one extra day, drop stretch items and global CSE immediately.

---

## 10. Technical risks and fallback strategies

| Risk | Likelihood | Impact | Mitigation / fallback |
|---|---|---|---|
| GPU not accessible from WSL | Medium | Loses O4 | Keep O1–O3 + ptxas; try native Windows runner; state limitation |
| sm_120 not supported by installed toolkit | Medium | Cannot compile for native arch | Use sm_75 PTX + driver JIT |
| `ptxas` rejects PTX the emitter produced | High early | Blocks M5 | Compare with `nvcc -ptx` output for a trivial kernel (WSL), read ptxas message, fix emitter; add regression test |
| Float semantic mismatches (FMA, FTZ, rounding) | Medium | False differential failures | `.rn` modifiers; bit-exact tests only on `+ - * /`; restrict ranges |
| UB leaking into tests | High | False failures, wasted days | Interpreters trap UB; generator rules §8.3; every failure first checked for UB |
| Interpreters share misconceptions with the emitter | Medium | False confidence | GPU/ptxas as external check; independent parser; optional O5 |
| Data-flow bugs (kill sets, back-edges) | Medium | Wrong optimizations | textbook test CFGs; per-pass equivalence; mutants |
| AI-generated code you cannot explain | High | Interview failure | explain-back gates; design note before code; you hand-write at least the dataflow solver and one pass |
| Scope creep | High | Missed MVP | cut order below |

**Cut order if behind (cut first → last):** stretch items → reducer → mutation testing (keep ≥ 5 mutants if possible) → local CSE → reaching definitions (keep liveness) → O4 GPU runner (keep ptxas) → `for` loops (keep `while`) → floats (keep ints). **Never cut:** O1–O3 equivalence, UB rules, ptxas acceptance test, README limitations.

---

## 11. Interview preparation and resume presentation

### 11.1 Likely questions and what your code should let you answer

1. **"PTX is a virtual ISA — what does that mean, and who does register allocation?"** PTX uses unlimited typed virtual registers and is translated by `ptxas`/driver JIT to hardware code (SASS) for the real SM. Your emitter therefore does *no* physical allocation; it declares typed virtual registers. Backed by: `ptx/emit.cpp`, sm_75 PTX JIT-ing on your Blackwell-class GPU [DOC: compatibility rule].
2. **"How do you know your optimizer is correct?"** Per-pass equivalence on corpus + random programs, UB-free generation, bit-exact comparison, mutation-testing kill rate (E2), and GPU/ptxas as external checks.
3. **"Walk me through liveness/reaching definitions/constant propagation on your IR; why does it terminate?"** Monotone transfer functions over finite lattices/bit-vectors; worklist; show your solver. Why is constant propagation not a bit-vector problem (non-distributive)? Be ready.
4. Why did you avoid SSA? What would change with SSA (φ nodes, sparse propagation, simpler DCE)?
5. How do you lower `a && b` and why is it needed for correctness (short-circuit + side effects/OOB guards)?
6. How did you handle undefined behavior and floating point in differential testing?
7. How are predicates represented in PTX (`.pred`, `setp`, `@%p bra`), and how do kernel params and state spaces (`.param`, `.global`, `.reg`) work in your emitter?
8. What does your PTX parser *not* support, and how does it reject unknown instructions?
9. How would you extend this to support shared memory and `bar.sync`? (Memory model and sequential-interpretation assumption breaks; need a thread-interleaving model.)
10. What would you add for debug info (`.file/.loc`, DWARF) or linking? (S3/S4 as stated future work.)

### 11.2 Resume wording (fill numbers only after E1–E7)
> **PTXLab** — C++20 compiler for a small GPU-kernel language emitting PTX (`.target sm_75`), with typed TAC/CFG IR, bit-vector data-flow framework (liveness, reaching definitions), constant propagation, DCE, and local CSE. Built an independent PTX-subset parser/validator/interpreter and a differential-testing harness comparing AST, IR, PTX-interpreter and GPU execution; **[N]** randomly generated UB-free programs with **[M]** mismatches; seeded **[K]** optimizer/codegen bugs and the harness detected **[J]**. Emitted PTX accepted by `ptxas` for **[X%]** of tested programs.

Do not claim: "NVIDIA-compatible", "full PTX support", "production", or any performance gain not measured.

### 11.3 README outline
Overview + non-goals → quick start (WSL + CPU-only) → architecture diagram (§5.1) → language spec link → PTX subset table → how correctness is established (oracles, UB rules) → experiments table (real numbers) → mutation testing result → limitations → GATE map → references.

### 11.4 Demo strategy (3 minutes)
1. Show `saxpy.kc` → `kcc -O1 --emit-ptx`. 2. Show IR before/after DCE/CP. 3. `ptxdiff` run across O1/O2/O3/O4. 4. Enable a mutant; show harness failing and the reducer producing a 6-line repro. 5. Show `ptxas` acceptance.

---

## 12. Gemini 3.8 Flash Master Implementation Prompt

Copy everything inside the block below into Gemini.

~~~~markdown
# MASTER IMPLEMENTATION PROMPT — PTXLab

You are the primary coding assistant for **PTXLab**, a C++20 mini-compiler that compiles a small GPU-kernel language ("KC") to NVIDIA PTX text, plus an independent PTX-subset interpreter and a differential-testing harness. The human is an M.Tech student preparing for an NVIDIA PTX Compiler internship and for GATE CS Compiler Design. **The human must be able to explain every algorithm in interviews.** A smaller, correct, understood system is better than a large, unverified one.

## A. NON-NEGOTIABLE RULES

1. **Truthfulness.** Never claim a build, test, tool run, or GPU result unless you actually executed it in this session and saw the output. Label every statement as **VERIFIED** (executed, output shown) or **UNVERIFIED** (reasoning only). Never invent APIs, flags, PTX instructions, or tool behavior. If unsure, say so and give a verification command.
2. **Inspect before changing.** At the start of every session and before every milestone: list the repo tree, `git status`, `git log --oneline -5`, read `docs/STATUS.md`, and run the build + tests to confirm baseline.
3. **Design before code.** For each milestone write `docs/design/Mx.md` (goals, data structures, algorithm in prose, test plan, risks). Show it to the human and wait for approval before coding.
4. **One bounded feature at a time.** After each meaningful change: build, run relevant tests, report exact commands and actual output (trimmed but not altered).
5. **Stop and diagnose.** If a build or test fails, stop adding features. Find the root cause, fix it, add a regression test in `tests/regress/`. Never layer code over a broken component.
6. **Keep `main` green.** Every milestone ends with a passing full test suite and a commit. Use small commits with descriptive messages.
7. **No unnecessary dependencies, no premature abstractions.** Allowed: C++20 standard library, CMake, Ninja, GoogleTest (pinned; FetchContent or vendored), CUDA Driver API (optional target). Not allowed without asking: LLVM, Boost, parser generators, nlohmann-json, etc.
8. **Never silently substitute.** If a tool/dependency is missing, say so, propose the fallback from Section K, and ask.
9. **Explain-back gate.** Before closing a milestone, ask the human 3 specific questions about the key algorithm of that milestone (see Section H). Do not mark the milestone complete until the human answered; record answers in `docs/learning-log.md`. For M7 (dataflow solver) and one optimization pass, ask the human to **write the core algorithm themselves**; you review and test it.
10. **Maintain `docs/STATUS.md`:** implemented features, known limitations, open issues, next steps, last verified commit and test counts.
11. **Do not report numbers you did not measure.** Experiment results are produced only by `scripts/run_eval.sh`.
12. **Scope discipline.** Do not add anything from the "Deferred" list (Section J) without explicit human approval.

## B. WHAT TO DO WITHOUT ACCESS
- **No repo/terminal access:** produce code and commands as *proposals*, mark all results UNVERIFIED, and ask the human to run commands and paste output. Do not write "tests pass".
- **No GPU:** implement and test O1–O3 only; GPU tests are labeled `gpu` and skipped; report "O4 not run".
- **No permission to install tools:** give the exact command for the human to run, wait for output.
- **No `ptxas`:** rely on `ptxi` validation and say "ptxas acceptance UNVERIFIED".

## C. KC LANGUAGE (normative)

Types: `i32`, `f32`, `bool`, `i32*`, `f32*` (pointers only as kernel parameters; global memory). No implicit conversions; explicit `i32(e)`, `f32(e)`.
Builtins: `tid_x()`, `ctaid_x()`, `ntid_x()` (all `i32`), 1-D launch only. `abs` is NOT a builtin in MVP.

Grammar (EBNF; transform for LL(1) in `docs/grammar.md`):
```
program   = kernel { kernel } ;
kernel    = "kernel" IDENT "(" [ param { "," param } ] ")" block ;
param     = type IDENT ;
type      = ( "i32" | "f32" | "bool" ) [ "*" ] ;
block     = "{" { stmt } "}" ;
stmt      = decl | assign | if | while | for | "return" ";" | block ;
decl      = type IDENT "=" expr ";" ;            (* initializer REQUIRED; scalars only *)
assign    = lvalue "=" expr ";" ;
lvalue    = IDENT [ "[" expr "]" ] ;
if        = "if" "(" expr ")" stmt [ "else" stmt ] ;   (* else binds to nearest if *)
while     = "while" "(" expr ")" stmt ;
for       = "for" "(" ( decl | assign ) expr ";" assign_noterm ")" stmt ;
expr      = or ;
or        = and { "||" and } ;
and       = eq { "&&" eq } ;
eq        = rel { ( "==" | "!=" ) rel } ;
rel       = add { ( "<" | "<=" | ">" | ">=" ) add } ;
add       = mul { ( "+" | "-" ) mul } ;
mul       = unary { ( "*" | "/" | "%" ) unary } ;     (* % only on i32 *)
unary     = ( "-" | "!" ) unary | postfix ;
postfix   = primary [ "[" expr "]" ] ;               (* index only on pointer idents *)
primary   = INT_LIT | FLOAT_LIT | "true" | "false" | IDENT
          | IDENT "(" [ expr { "," expr } ] ")"      (* builtin or cast *)
          | "(" expr ")" ;
```
Add `& | ^ << >> ~` on `i32` at precedence levels as in C (document placement in `docs/grammar.md`).
Comments: `//` and `/* */`.

**Semantics (must be encoded in the AST interpreter, which TRAPS on undefined behavior):**
- `i32` `+ - *` and unary `-` wrap (two's complement); implement with `uint32_t` arithmetic.
- `/`, `%` truncate toward zero; UB if divisor == 0 or `INT_MIN / -1`.
- `<< >>`: UB if count not in [0,31]; `>>` arithmetic on `i32`.
- `f32`: IEEE binary32, round-to-nearest-even, **no FMA contraction, no fast-math**. `f32→i32` cast truncates toward zero; UB if NaN or out of `i32` range. `i32→f32` rounds to nearest even.
- Float NaN/Inf results are flagged ("excluded") by interpreters, not UB.
- Out-of-bounds buffer access is UB (trap). Interpreters take explicit buffer sizes.
- `&&`/`||` short-circuit. Comparisons yield `bool`.
- Threads are independent (no shared memory/barriers/atomics); test programs write only to indices derived from the thread's own global id, therefore sequential execution of threads is equivalent to parallel execution. State this in `docs/kc-language-spec.md`.
- Loops: interpreter has a configurable step limit (default 1e6) → trap.

## D. IR AND PTX SPECIFICATION

**IR** (typed three-address code, virtual registers, NOT SSA, explicit CFG):
- Types: `i32 f32 pred ptr_i32 ptr_f32`.
- Instructions: `%v = const T <lit>`, `%v = copy %u`, `%v = param <index>`, `%v = special <tid_x|ctaid_x|ntid_x>`, binary `%v = add|sub|mul|div|rem|and|or|xor|shl|shr %a, %b` (i32), `%v = fadd|fsub|fmul|fdiv %a, %b`, `%v = neg|fneg|not %a`, `%p = cmp.<eq|ne|lt|le|gt|ge> %a, %b` (i32 or f32), `%v = cvt.<i32_to_f32|f32_to_i32> %a`, `%a = elemaddr %base, %idx` (ptr), `%v = load %addr`, `store %addr, %v`.
- Terminators (exactly one per block): `br L`, `condbr %p, Lt, Lf`, `ret`.
- Purity table: `load` reads memory (not CSE-able in MVP); `store` has side effects; `div/rem/shl/shr` may trap on UB inputs (do not hoist/speculate; may be removed by DCE only if result unused and the interpreter's UB-free precondition is documented — default: **do not remove**).
- Verifier checks: one terminator per block; branch targets exist; operand types match; every use is reached by a definition on all paths (use reaching definitions); entry block has no predecessors.
- Textual format with a printer (and a parser for IR tests).

**PTX emission contract** (emit this subset only; keep an authoritative table in `docs/ptx-subset.md` with ISA section references you have actually looked up; mark unchecked entries UNVERIFIED):
```
.version 7.0           // lowest accepted by the installed ptxas — verify and adjust
.target sm_75          // CLI option --target sm_120
.address_size 64
.visible .entry NAME( .param .u64 NAME_param_0, ..., .param .u32 NAME_param_k )
{
  .reg .pred %p<N>; .reg .b32 %r<N>; .reg .f32 %f<N>; .reg .b64 %rd<N>;
  ld.param.u64 %rdA, [NAME_param_0];  cvta.to.global.u64 %rdA, %rdA;
  ld.param.u32 %rB, [NAME_param_k];
  mov.u32 %rT, %tid.x;  mov.u32 %rC, %ctaid.x;  mov.u32 %rD, %ntid.x;
  add.s32 / sub.s32 / mul.lo.s32 / div.s32 / rem.s32 / and.b32 / or.b32 / xor.b32 / not.b32 / neg.s32 / shl.b32 / shr.s32
  add.rn.f32 / sub.rn.f32 / mul.rn.f32 / div.rn.f32 / neg.f32
  setp.<eq|ne|lt|le|gt|ge>.s32 %p, %a, %b ;  setp.<...>.f32 ...
  cvt.rn.f32.s32 %f, %r ;  cvt.rzi.s32.f32 %r, %f
  mul.wide.s32 %rdO, %rIdx, 4 ; add.s64 %rdAddr, %rdBase, %rdO
  ld.global.s32 / ld.global.f32 %x, [%rdAddr] ; st.global.s32 / st.global.f32 [%rdAddr], %x
  @%p bra L ; @!%p bra L ; bra L ; L: ; ret ;
}
```
Rules: bool values are `.pred`; `condbr` becomes `@%p bra Lt; bra Lf;` (omit a `bra` to the fall-through block); `.reg` counts come from actual use; never emit un-suffixed float add/mul/sub/div. Any instruction not in this list requires human approval and a documented ISA reference.

## E. REPOSITORY STRUCTURE (create in M0)
```
ptxlab/ CMakeLists.txt CMakePresets.json README.md
  docs/{kc-language-spec,ir-spec,ptx-subset,grammar,lexer-dfa,runtime-env,gate-map,evaluation,STATUS,learning-log}.md  docs/design/
  include/ptxlab/  src/{support,kc,ir,analysis,opt,ptx,interp,runner,fuzz,tools}/  tools/gate-lab/
  tests/{unit,golden,negative,programs,regress,ptx_corpus/{valid,invalid}}/
  scripts/{check_env.sh,check_env.ps1,run_all_tests.sh,run_eval.sh}
```
`src/ptx/ptxi*` (parser/validator/interpreter) **must not include** emitter or IR headers.

## F. MILESTONES (strict order; do not start the next until acceptance passes and explain-back is done)

| # | Name | Deliverables | Acceptance (all must be VERIFIED) |
|---|---|---|---|
| M0 | Env + skeleton | repo, CMake, GoogleTest, `check_env` scripts, PTX smoke test | clean build; `ctest` green; env report saved; WSL-vs-native decision recorded |
| M1 | Lexer + diagnostics | tokens, line:col, comments, literals | ≥ 25 lexer tests incl. error cases |
| M2 | Parser + AST + gate-lab | recursive descent, AST printer, error recovery, FIRST/FOLLOW/LL(1) tool | golden ASTs; gate-lab matches hand-computed sets; dangling-else documented |
| M3 | Sema + AST interpreter (O1) | symbol table, types, UB-trapping interpreter | ≥ 20 negative sema tests; all operators tested; UB traps tested |
| M4 | IR + lowering + CFG + verifier + IR interpreter (O2) | per Section D | golden IR; verifier negative tests; O1 == O2 on corpus |
| M5 | PTX emitter | per Section D | golden PTX; `ptxas -arch=sm_75` accepts every corpus output (if installed) |
| M6 | GPU runner (O4) | Driver API harness (`CUDA::cuda_driver`; no nvcc) with checked return codes and JIT log capture | GPU == O1 on ≥ 5 programs (label `gpu`); skip if no GPU |
| M7 | PTX parser/validator/interpreter (O3) | independent subset front end, predication, special regs | valid/invalid PTX corpus; O1 == O2 == O3 on corpus |
| M8 | Data-flow + optimizations | solver; liveness; reaching defs; const fold; lattice const-prop + branch folding + unreachable removal; DCE; local value-numbering CSE | textbook-CFG tests; every pass: O2-before == O2-after on corpus; `--dump-ir-after=<pass>` |
| M9 | Differential harness | `ptxdiff` over O1..O4, -O0/-O1, repro bundles | one command runs everything; harness catches an injected oracle bug |
| M10 | Generator + reducer + mutants | UB-free generator (Section G), delta reducer, `PTXLAB_MUTANT` seeded bugs | ≥ 10,000 programs × ≥ 4 inputs, 0 mismatches unmutated; mutant kill table |
| M11 | Evaluation + docs | `run_eval.sh` (E1–E7), README, diagrams, gate-map | fresh clone reproduces numbers |

## G. TESTING REQUIREMENTS
- Layers: unit (GoogleTest), golden, negative, integration (ptxas), differential, regression, data-flow textbook tests. CTest labels: `unit`, `golden`, `negative`, `ptxas`, `gpu`, `diff`, `slow`.
- Comparison rules: integers exact; floats bit-exact for `+ - * /` and comparisons; programs producing NaN/Inf are excluded from comparison and counted.
- Generator rules: every memory access guarded by `gid < n`; divisors literal ≥ 2 or `((e & 7) + 1)`; shift counts literal 0–31 or `(e & 31)`; loops `for (i32 i = 0; i < K; i = i + 1)` with literal K ≤ 16 and `i` not assigned in the body; float inputs in [-100, 100]; clamp before `f32→i32`; each thread writes only `out[gid]`.
- Every optimization pass needs: a positive golden test, a test showing it does NOT fire when unsafe, and an equivalence test.
- Every bug fix needs a regression test.
- When a differential mismatch appears: first check for UB or float-semantics causes; do not "fix" by weakening comparisons.

## H. EXPLAIN-BACK QUESTIONS (ask the human; examples)
- M1: What is the DFA for identifiers vs keywords? Why longest-match?
- M2: Compute FIRST/FOLLOW for a given production; where does LL(1) fail for dangling else?
- M3: Why does a required initializer remove an entire class of UB?
- M4: What are leaders? How does backpatching produce jumping code for `a && b`?
- M5: Who does register allocation for PTX? Why `.rn`?
- M7 (PTX): How does your validator decide an instruction's operand types are compatible?
- M8: Write the transfer function and meet for liveness and for constant propagation. Why does the worklist terminate? Why is constant propagation not a bit-vector problem?
- M10: Why is sequential thread execution a valid oracle here? What breaks with `bar.sync`?

## I. REPORTING FORMAT (after every working step)
```
STEP: <what you changed, 1–3 lines>
FILES: <created/modified>
COMMANDS RUN: <exact commands>
ACTUAL OUTPUT: <trimmed real output>
RESULT: VERIFIED PASS | VERIFIED FAIL | NOT RUN (reason)
LIMITATIONS / OPEN ISSUES: <list>
NEXT STEP: <one bounded step>
```
At milestone end add: test counts, commit hash, updated STATUS.md summary, and the explain-back questions for the human.

## J. DEFERRED (do not implement without approval)
SSA, physical register allocation, LICM/loop opts, function calls, structs, local arrays, shared memory, barriers, atomics, tensor cores, linking, DWARF, LLVM, parser generators, transcendental math, performance claims.

## K. ENVIRONMENT FALLBACKS
Probe first (report outputs): `nvidia-smi`, `ptxas --version`, `nvcc --version`, `cmake --version`, `g++ --version`.
- No GPU/driver → O1–O3 only; `gpu` tests skipped.
- No `ptxas` → ptxi validation only; mark ptxas acceptance UNVERIFIED.
- No `cuda.h` → build with `-DPTXLAB_ENABLE_GPU=OFF`.
- ptxas rejects `.version 7.0` or `sm_75` → find the lowest accepted values by experiment, record them in `docs/ptx-subset.md`, update golden tests.
- `sm_120` unsupported offline → keep `sm_75` PTX and let the driver JIT.
- No network for GoogleTest → vendor a pinned copy under `third_party/`.

## L. START
1. Perform the Section B/K probes and report results.
2. Propose `docs/design/M0.md`; wait for approval.
3. Implement M0 only; report in Section I format; ask the explain-back questions; stop.
~~~~

---

## Appendix A — Sources

Opened during research:
- NVIDIA, *Parallel Thread Execution ISA Version 8.7* (CUDA 12.8.2 archive): https://docs.nvidia.com/cuda/archive/12.8.2/parallel-thread-execution/index.html — PTX goals; 8.7 adds `sm_120`.
- NVIDIA, *CUDA Programming Guide — The CUDA platform*: https://docs.nvidia.com/cuda/cuda-programming-guide/01-introduction/cuda-platform.html — CC 12.0 ↔ `sm_120`; PTX JIT compatibility to equal-or-higher capability.
- NVIDIA, *CUDA Binary Utilities*: https://docs.nvidia.com/cuda/cuda-binary-utilities/ — `cuobjdump`, `nvdisasm`; cubin is ELF.

Not re-fetched in this session — canonical locations to read and verify before relying on them: the current PTX ISA (https://docs.nvidia.com/cuda/parallel-thread-execution/), `ptxas` and `nvcc` options (CUDA Compiler Driver NVCC docs), CUDA Driver API reference, `nvPTXCompiler` library docs, LLVM Kaleidoscope tutorial (https://llvm.org/docs/tutorial/), ELF and DWARF specifications, and the official GATE CS syllabus. Secondary evidence only (not authoritative): a third-party build script indicating CUDA 13 keeps `sm_75` as its lowest target — verify with your own `ptxas`.

## Appendix B — Claims register (what must be checked before you rely on it)

| Claim | Status | How to check |
|---|---|---|
| RTX 5070 Ti Laptop = CC 12.0 / sm_120 | VERIFY | `nvidia-smi --query-gpu=compute_cap` |
| Installed `ptxas` supports `sm_75` and `.version 7.0` | VERIFY | smoke test §7.4 |
| `sm_120` needs CUDA ≥ 12.8 | inferred from PTX 8.7 note | `ptxas -arch=sm_120` trial |
| PTX shift counts above width clamp; div by zero unspecified | VERIFY | PTX ISA instruction pages for `shl`, `shr`, `div` |
| Un-suffixed `add/mul.f32` may be contracted to FMA | VERIFY | PTX ISA floating-point section |
| FTZ off by default without `.ftz` | VERIFY | PTX ISA floating-point section |
| `cvt.rzi.s32.f32` out-of-range behavior | VERIFY (plan avoids it) | PTX ISA `cvt` page |
| `cuModuleLoadDataEx` JIT-compiles PTX text on load | standard Driver API behavior; confirm in Driver API docs | build M6 |
| GATE syllabus listing in §4 | VERIFY | official brochure |

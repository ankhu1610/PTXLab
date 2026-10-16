# PTXLab Learning Log & Concept Checkpoint

## Milestone 0: Environment, Toolchain, and Compilation Contract

### Concept Questions & Justifications

#### Question 1 (IR vs. Target Virtual ISA):
> In a compiler pipeline, why is Intermediate Representation (IR) typically target-independent, whereas PTX is treated as a target back end, even though PTX is also a virtual ISA?

*Candidate Explanation & Justification:*
* **Core Distinction:** General Intermediate Representation (IR, such as our typed TAC or LLVM IR) is strictly target-independent—it abstracts away the underlying execution hardware and memory hierarchy, operating on abstract values or unbounded virtual registers without committing to machine-specific topologies.
* **PTX as a Target Virtual ISA:** Even though PTX is virtualized (using unlimited virtual registers `%r`, `%f`, `%p` and requiring a downstream assembler to map them to physical registers), it is **target-specific to NVIDIA GPU architectures**:
  1. **Hardware Execution Model:** PTX directly bakes in GPU execution concepts—threads, warps, cooperative thread blocks (CTAs), grids, and special hardware registers (`%tid.x`, `%ctaid.x`, `%ntid.x`).
  2. **Explicit Memory State Spaces:** PTX enforces GPU-specific address spaces (`.global`, `.shared`, `.local`, `.param`, `.const`), memory barriers (`bar.sync`), and predication.
  3. **Role in Pipeline:** To our compiler (`kcc`), PTX is the target text output. The NVIDIA CUDA Driver / `ptxas` then acts as the final backend compiler, consuming this target-specific virtual ISA to produce physical machine instructions (SASS / cubin).

---

#### Question 2 (Forward Compatibility & JIT):
> Why can PTX code compiled for `.target sm_75` execute on a future architecture like your Blackwell `sm_120` GPU without recompiling the original source code? What software component performs this?

*Candidate Explanation & Justification:*
* **Forward Compatibility Mechanism:** PTX serves as an intermediate, architecture-stable virtual assembly format. PTX targeted for an older architecture (like `.target sm_75` / Turing) defines abstract operations over virtual registers that are forward-compatible with newer architectures (like `sm_120` / Blackwell).
* **Software Component (CUDA Driver JIT Compiler):** The **CUDA Driver's built-in JIT compiler** (embedded in `nvcuda.dll` on Windows or `libcuda.so` on Linux) intercepts the PTX at runtime (e.g., when loaded via `cuModuleLoadData` or the CUDA runtime). It invokes the driver-level PTX compiler/assembler to perform final register allocation, instruction scheduling, and instruction selection to emit native hardware machine instructions (**SASS**) specifically optimized for the host system's GPU (`sm_120`).
* **Interview Takeaway:** Application binaries can distribute PTX text or fatbins containing PTX to guarantee forward execution across unreleased future GPU generations without needing source recompilation.

---

#### Question 3 (Compiler Engineering Rigor):
> Why do professional compiler teams set up an independent environment auditor (`check_env`) and smoke test harness *before* writing the lexer or parser?

*Candidate Explanation & Justification:*
* **De-Risking Development & Early Detection:** Building a compiler is complex. Establishing an automated environment auditor (`check_env`) and smoke test harness catches toolchain, SDK, runtime, and architecture mismatches (e.g., C++ standard compliance, CRT library linkage, missing driver APIs, or PATH misconfigurations) immediately—*before* any compiler logic is authored.
* **Isolating Root Cause:** When tests inevitably fail later in development (e.g., in lexing, parsing, or register allocation), having a verified, audited baseline guarantees the failure originates from compiler logic bugs rather than hidden host environment or compiler driver flaws ("it worked on my machine" syndrome).
* **Reproducibility & CI Foundation:** The baseline sets up the continuous integration (CI) contract early, ensuring every contributor and CI runner can reliably reproduce identical build and test results.

---

## Milestone 1: Diagnostics, SourceManager, and Lexer

### Concept Questions & Justifications

#### Question 1 (DFA Design & Keyword Resolution):
> In our lexer, why do identifiers and keywords share the same DFA transition state (`[a-zA-Z_][a-zA-Z0-9_]*`) followed by a keyword table lookup, rather than creating separate DFA branches for every keyword (`k-e-r-n-e-l`, `i-3-2`, etc.)?

*Candidate Explanation & Justification:*
(Awaiting human input)

---

#### Question 2 (Maximal Munch Ambiguity):
> What is the Principle of Maximal Munch (Longest Match)? Why is it required when scanning operators like `<<`, `<=`, and `==`, and how does the lexer resolve `<<<`?

*Candidate Explanation & Justification:*
(Awaiting human input)

---

#### Question 3 (Diagnostic Engine & Line Offset Table):
> Why does `SourceManager` precompute a line-offset table `std::vector<size_t>` to perform binary search ($O(\log N)$) on byte offsets for diagnostics, rather than re-scanning from index 0 or incrementing line/col counters on the fly?

*Candidate Explanation & Justification:*
(Awaiting human input)


# PTXLab Learning Log & Concept Checkpoint

## Milestone 0: Environment, Toolchain, and Compilation Contract

### Concept Questions & Justifications

#### Question 1 (IR vs. Target Virtual ISA):
> In a compiler pipeline, why is Intermediate Representation (IR) typically target-independent, whereas PTX is treated as a target back end, even though PTX is also a virtual ISA?

*Candidate Explanation & Justification:*
(Awaiting human input)

---

#### Question 2 (Forward Compatibility & JIT):
> Why can PTX code compiled for `.target sm_75` execute on a future architecture like your Blackwell `sm_120` GPU without recompiling the original source code? What software component performs this?

*Candidate Explanation & Justification:*
(Awaiting human input)

---

#### Question 3 (Compiler Engineering Rigor):
> Why do professional compiler teams set up an independent environment auditor (`check_env`) and smoke test harness *before* writing the lexer or parser?

*Candidate Explanation & Justification:*
(Awaiting human input)

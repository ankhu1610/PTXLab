# PTXLab PTX Virtual ISA Subset Specification

**Target:** PTX ISA 7.0+ / NVIDIA sm_75+ Architecture  
**Address Size:** 64-bit  
**Standard Directives:** `.version`, `.target`, `.address_size`, `.visible .entry`, `.param`, `.reg`

---

## 1. Overview

This document specifies the concrete syntax and operational semantics of the NVIDIA Parallel Thread Execution (PTX) virtual ISA subset generated and supported by the PTXLab compiler framework (`kcc`).

The subset provides all primitives necessary for data-parallel numeric kernels, including:
- Virtual register banks (`%r`, `%f`, `%rd`, `%p`)
- Typed kernel parameter ABI (`.param`)
- Hardware thread index intrinsics (`%tid.x`, `%ctaid.x`, `%ntid.x`)
- 32-bit two's-complement integer and bitwise arithmetic
- 32-bit IEEE 754 single-precision floating-point arithmetic with round-to-nearest-even (`.rn`)
- Explicit type conversions (`cvt.rn.f32.s32`, `cvt.rzi.s32.f32`)
- 64-bit wide pointer arithmetic and global memory load/store (`ld.global`, `st.global`)
- Predicate registers, comparison operators (`setp`), and predicated branching (`@%p bra`)

---

## 2. Lexical & Syntactic Grammar (EBNF)

### 2.1 Comments and Whitespace
```ebnf
Whitespace  ::= (' ' | '\t' | '\r' | '\n')+
LineComment ::= '//' [^\n]*
BlockComment::= '/*' ( [^*] | '*'+ [^/*] )* '*'+ '/'
```

### 2.2 Directives
```ebnf
ModuleDirective   ::= '.version' FloatLiteral
                    | '.target' Identifier
                    | '.address_size' IntLiteral

KernelHeader      ::= '.visible'? '.entry' Identifier '(' ParamDeclList? ')'
ParamDeclList     ::= ParamDecl (',' ParamDecl)*
ParamDecl         ::= '.param' ParamType Identifier
ParamType         ::= '.u32' | '.f32' | '.u64'

RegDecl           ::= '.reg' RegType RegSpec (',' RegSpec)* ';'
RegType           ::= '.b32' | '.f32' | '.b64' | '.pred'
RegSpec           ::= RegRange | RegName
RegRange          ::= RegPrefix '<' IntLiteral '>'
RegPrefix         ::= '%r' | '%f' | '%rd' | '%p'
RegName           ::= '%' Identifier
```

### 2.3 Statements, Labels, and Instructions
```ebnf
KernelBody        ::= '{' (RegDecl | LabelDef | Statement)* '}'
LabelDef          ::= Identifier ':'
Statement         ::= Predicate? Instruction ';'

Predicate         ::= '@' '!'? RegName

Instruction       ::= MovInst
                    | ParamInst
                    | CvtInst
                    | ArithInst
                    | CmpInst
                    | MemInst
                    | BranchInst
                    | RetInst
```

### 2.4 Operand Grammar
```ebnf
Operand           ::= RegName
                    | SpecialReg
                    | IntLiteral
                    | FloatLiteral
                    | HexFloatLiteral
                    | MemoryRef
                    | LabelRef

SpecialReg        ::= '%tid.x' | '%ctaid.x' | '%ntid.x'
MemoryRef         ::= '[' RegName ']'
                    | '[' RegName '+' IntLiteral ']'
                    | '[' Identifier ']'       (* Parameter memory reference *)

HexFloatLiteral   ::= '0f' [0-9a-fA-F]{8}     (* 32-bit IEEE 754 bit-pattern *)
LabelRef          ::= Identifier
```

---

## 3. Supported Instructions

### 3.1 Data Movement & Conversions
| PTX Mnemonic | Syntax | Semantics |
|---|---|---|
| `mov.u32` | `mov.u32 %r_dst, src;` | Copies 32-bit integer or special register (`%tid.x`, `%ctaid.x`, `%ntid.x`) |
| `mov.f32` | `mov.f32 %f_dst, src;` | Copies 32-bit float register or immediate (`0fXXXXXXXX` / decimal) |
| `mov.pred`| `mov.pred %p_dst, src;`| Copies 1-bit predicate register or immediate `0`/`1` |
| `mov.u64` | `mov.u64 %rd_dst, src;`| Copies 64-bit address register |
| `ld.param.u32` | `ld.param.u32 %r_dst, [param_name];` | Loads 32-bit scalar from parameter space |
| `ld.param.f32` | `ld.param.f32 %f_dst, [param_name];` | Loads 32-bit float from parameter space |
| `ld.param.u64` | `ld.param.u64 %rd_dst, [param_name];`| Loads 64-bit pointer from parameter space |
| `cvta.to.global.u64` | `cvta.to.global.u64 %rd_dst, %rd_src;` | Converts parameter/generic address to global address space |
| `cvt.rn.f32.s32` | `cvt.rn.f32.s32 %f_dst, %r_src;` | Converts signed 32-bit integer to 32-bit float (round nearest even) |
| `cvt.rzi.s32.f32` | `cvt.rzi.s32.f32 %r_dst, %f_src;` | Converts 32-bit float to signed 32-bit integer (truncate toward zero) |

### 3.2 Integer Arithmetic & Bitwise
| PTX Mnemonic | Syntax | Semantics |
|---|---|---|
| `add.s32` | `add.s32 %r_dst, %r_a, %r_b;` | Signed 32-bit two's-complement addition |
| `sub.s32` | `sub.s32 %r_dst, %r_a, %r_b;` | Signed 32-bit two's-complement subtraction |
| `mul.lo.s32` | `mul.lo.s32 %r_dst, %r_a, %r_b;` | Low 32 bits of signed integer multiplication |
| `div.s32` | `div.s32 %r_dst, %r_a, %r_b;` | Signed integer division (truncating toward zero) |
| `rem.s32` | `rem.s32 %r_dst, %r_a, %r_b;` | Signed integer remainder |
| `and.b32` | `and.b32 %r_dst, %r_a, %r_b;` | Bitwise logical AND |
| `or.b32`  | `or.b32 %r_dst, %r_a, %r_b;`  | Bitwise logical OR |
| `xor.b32` | `xor.b32 %r_dst, %r_a, %r_b;` | Bitwise logical XOR |
| `shl.b32` | `shl.b32 %r_dst, %r_a, %r_b;` | Shift left (masked shift count & 31) |
| `shr.s32` | `shr.s32 %r_dst, %r_a, %r_b;` | Arithmetic shift right (sign-extended, count & 31) |
| `neg.s32` | `neg.s32 %r_dst, %r_a;`       | Signed two's-complement negation |
| `not.b32` | `not.b32 %r_dst, %r_a;`       | Bitwise NOT (one's-complement) |
| `not.pred`| `not.pred %p_dst, %p_a;`      | Predicate logical negation |

### 3.3 Floating-Point Arithmetic
| PTX Mnemonic | Syntax | Semantics |
|---|---|---|
| `add.rn.f32` | `add.rn.f32 %f_dst, %f_a, %f_b;` | IEEE 754 binary32 addition (round-to-nearest-even) |
| `sub.rn.f32` | `sub.rn.f32 %f_dst, %f_a, %f_b;` | IEEE 754 binary32 subtraction (round-to-nearest-even) |
| `mul.rn.f32` | `mul.rn.f32 %f_dst, %f_a, %f_b;` | IEEE 754 binary32 multiplication (round-to-nearest-even) |
| `div.rn.f32` | `div.rn.f32 %f_dst, %f_a, %f_b;` | IEEE 754 binary32 division (round-to-nearest-even) |
| `neg.f32`    | `neg.f32 %f_dst, %f_a;`          | IEEE 754 sign-bit flip |

### 3.4 Comparison & Predication
| PTX Mnemonic | Syntax | Semantics |
|---|---|---|
| `setp.<cond>.s32` | `setp.eq.s32 %p_dst, %r_a, %r_b;` | Signed integer comparison setting predicate (`eq, ne, lt, le, gt, ge`) |
| `setp.<cond>.f32` | `setp.lt.f32 %p_dst, %f_a, %f_b;` | Floating-point comparison setting predicate (`eq, ne, lt, le, gt, ge`) |

### 3.5 Memory Addressing, Load, and Store
| PTX Mnemonic | Syntax | Semantics |
|---|---|---|
| `mul.wide.s32` | `mul.wide.s32 %rd_dst, %r_a, imm;` | Sign-extends 32-bit integer and multiplies to 64-bit offset |
| `add.s64` | `add.s64 %rd_dst, %rd_a, %rd_b;` | 64-bit pointer address calculation |
| `ld.global.s32` | `ld.global.s32 %r_dst, [%rd_addr];` | Loads 32-bit integer from global memory |
| `ld.global.f32` | `ld.global.f32 %f_dst, [%rd_addr];` | Loads 32-bit float from global memory |
| `st.global.s32` | `st.global.s32 [%rd_addr], %r_val;` | Stores 32-bit integer to global memory |
| `st.global.f32` | `st.global.f32 [%rd_addr], %f_val;` | Stores 32-bit float to global memory |

### 3.6 Control Flow
| PTX Mnemonic | Syntax | Semantics |
|---|---|---|
| `bra` | `bra target_label;` | Unconditional jump to label |
| `@%p bra` | `@%p0 bra target_label;` | Conditional branch: jump if predicate is true |
| `@!%p bra` | `@!%p0 bra target_label;` | Inverted conditional branch: jump if predicate is false |
| `ret` | `ret;` | Exits the kernel thread |

# Lexer Deterministic Finite Automaton (DFA) Specification

**Component:** KC Lexer (`src/kc/lexer.cpp`)  
**Purpose:** Formal specification of regular expressions, states, and transitions for the KC front end (GATE CS Compiler Design alignment).

---

## 1. Regular Definitions

```text
letter          -> [a-zA-Z_]
digit           -> [0-9]
hex_digit       -> [0-9a-fA-F]
nonzero_digit   -> [1-9]
whitespace      -> [ \t\r\n]

identifier      -> letter ( letter | digit )*
int_literal     -> "0" | nonzero_digit digit* | "0" [xX] hex_digit+
float_literal   -> digit+ "." digit* exponent? [fF]?
                 | "." digit+ exponent? [fF]?
                 | digit+ exponent [fF]?
exponent        -> [eE] [+-]? digit+
```

---

## 2. Token Kinds & Regular Expressions

| Token Kind | Category | Regular Expression / Value | Notes |
|---|---|---|---|
| `KwKernel` | Keyword | `"kernel"` | Exact string match |
| `KwI32` | Keyword | `"i32"` | 32-bit signed int |
| `KwF32` | Keyword | `"f32"` | 32-bit IEEE float |
| `KwBool` | Keyword | `"bool"` | 1-bit boolean |
| `KwTrue` | Keyword | `"true"` | Boolean literal true |
| `KwFalse` | Keyword | `"false"` | Boolean literal false |
| `KwIf` | Keyword | `"if"` | Control flow |
| `KwElse` | Keyword | `"else"` | Control flow |
| `KwWhile` | Keyword | `"while"` | Loop construct |
| `KwFor` | Keyword | `"for"` | Loop construct |
| `KwReturn` | Keyword | `"return"` | Exit kernel |
| `Identifier` | Identifier | `[a-zA-Z_][a-zA-Z0-9_]*` | Reserved if in keyword table |
| `IntLiteral` | Constant | `[0-9]+ \| 0[xX][0-9a-fA-F]+` | Parsed with 32-bit range check |
| `FloatLiteral` | Constant | `[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?[fF]?` | Parsed via `std::from_chars` |
| `Plus` | Operator | `"+"` | |
| `Minus` | Operator | `"-"` | |
| `Star` | Operator | `"*"` | |
| `Slash` | Operator | `"/"` | (Guarded against `//` and `/*`) |
| `Percent` | Operator | `"%"` | |
| `Amp` | Operator | `"&"` | (Guarded against `&&`) |
| `AmpAmp` | Operator | `"&&"` | |
| `Pipe` | Operator | `"\|"` | (Guarded against `\|\|`) |
| `PipePipe` | Operator | `"\|\|"` | |
| `Caret` | Operator | `"^"` | |
| `Tilde` | Operator | `"~"` | |
| `Exclaim` | Operator | `"!"` | (Guarded against `!=`) |
| `ExclaimEqual` | Operator | `"!="` | |
| `Equal` | Operator | `"="` | (Guarded against `==`) |
| `EqualEqual` | Operator | `"=="` | |
| `Less` | Operator | `"<"` | (Guarded against `<=` and `<<`) |
| `LessEqual` | Operator | `"<="` | |
| `LessLess` | Operator | `"<<"` | |
| `Greater` | Operator | `">"` | (Guarded against `>=` and `>>`) |
| `GreaterEqual`| Operator | `">="` | |
| `GreaterGreater`| Operator | `">>"` | |
| `LParen`, `RParen` | Delimiter | `"("`, `")"` | |
| `LBrace`, `RBrace` | Delimiter | `"{"`, `"}"` | |
| `LBracket`, `RBracket`| Delimiter | `"["`, `"]"` | |
| `Semicolon` | Delimiter | `";"` | |
| `Comma` | Delimiter | `","` | |

---

## 3. Disambiguation Rules

1. **Maximal Munch (Longest Match):**
   * If the input is `<=`, the lexer returns `LessEqual`, not `Less` followed by `Equal`.
   * If the input is `>>`, the lexer returns `GreaterGreater`, not two `Greater` tokens.
   * If the input is `/*`, it begins a block comment rather than a `Slash` and `Star`.
2. **Keyword Priority:**
   * Identifiers and keywords share the initial DFA state transition (`letter (letter | digit)*`).
   * When an identifier is recognized, the lexer consults a keyword dictionary. If matched, the token kind is converted from `Identifier` to the respective `Kw*` kind.
3. **Leading Dot in Numeric Literals:**
   * A dot `.` followed immediately by a digit `[0-9]` begins a `FloatLiteral` (e.g., `.5`).
   * A standalone dot without following digits is an illegal character (KC has no struct member access in MVP).

---

## 4. DFA State Transition Diagram (Conceptual)

```
                 [ \t\r\n]
                 ┌──────┐
                 │      │
                 ▼      │
            ┌───────────────┐
            │  START (S0)   │
            └──┬──┬──┬──┬───┘
   [a-zA-Z_]   │  │  │  │  '/'
 ┌─────────────┘  │  │  └───────────────────┐
 │                │  │ [0-9]                │
 ▼                │  └───────────────┐      ▼
┌───────────────┐ │                  │  ┌───────────────┐
│  IDENT / KW   │ │ '.' + digit      │  │  SLASH SEEN   │
└───────┬───────┘ │                  │  └─┬───┬───────┬─┘
        │         ▼                  ▼    │   │       │ other
[a-zA-Z0-9_] ┌─────────────┐   ┌─────────┐│'/'│'*'    ▼
        └───►│ FLOAT_DOT   │   │ INT_DEC ││   │   [SLASH TOKEN]
             └─────────────┘   └─────────┘▼   ▼
                                        LINE  BLOCK
                                        COMM  COMM
```

---

## 5. GATE Interview Questions & Rationale

* **Q: Why does the compiler use Maximal Munch?**
  * *A:* It ensures deterministic parsing of multi-character tokens like `==`, `!=`, `<<`, and prevents ambiguous sequences (e.g. `i---1` being interpreted inconsistently).
* **Q: Why are keywords resolved via a symbol/hash table after recognizing an identifier instead of creating separate DFA start states?**
  * *A:* Integrating 11+ keywords directly into the DFA character transition table exponentially increases the number of states and transition table size. Collapsing all into a generic identifier DFA and doing an $O(1)$ keyword hash lookup keeps the scanner compact, cache-friendly, and maintainable.

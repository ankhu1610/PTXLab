# KC Grammar & LL(1) Parsing Specification

**Component:** KC Parser (`src/kc/parser.cpp`) & `tools/gate-lab`  
**Purpose:** Formal grammar definitions, left-recursion elimination, dangling-else resolution, and LL(1) transformation for GATE CS Compiler Design.

---

## 1. Normative EBNF Grammar

```ebnf
program        ::= kernel+
kernel         ::= "kernel" IDENT "(" [ param_list ] ")" block
param_list     ::= param ( "," param )*
param          ::= type IDENT
type           ::= primitive_type [ "*" ]
primitive_type ::= "i32" | "f32" | "bool"

block          ::= "{" stmt* "}"
stmt           ::= decl_stmt
                 | assign_stmt
                 | if_stmt
                 | while_stmt
                 | for_stmt
                 | return_stmt
                 | block

decl_stmt      ::= type IDENT "=" expr ";"
assign_stmt    ::= lvalue "=" expr ";"
lvalue         ::= IDENT [ "[" expr "]" ]
if_stmt        ::= "if" "(" expr ")" stmt [ "else" stmt ]
while_stmt     ::= "while" "(" expr ")" stmt
for_stmt       ::= "for" "(" for_init expr ";" assign_expr ")" stmt
for_init       ::= decl_stmt | assign_stmt
assign_expr    ::= lvalue "=" expr
return_stmt    ::= "return" ";"
```

---

## 2. Expression Precedence & Left-Recursion Elimination

Direct left-recursive expression grammars (e.g., $E \to E + T \mid T$) cannot be parsed by top-down LL(1) or recursive-descent parsers because they lead to infinite recursion.

We structure expressions into an explicit precedence hierarchy where each non-terminal corresponds to one precedence level (from lowest to highest):

| Precedence | Level Name | Operators | Associativity | Grammar Rule |
|---|---|---|---|---|
| 1 (Lowest) | `LogicalOr` | `\|\|` | Left-to-right | `or ::= and ( "\|\|" and )*` |
| 2 | `LogicalAnd` | `&&` | Left-to-right | `and ::= bit_or ( "&&" bit_or )*` |
| 3 | `BitwiseOr` | `\|` | Left-to-right | `bit_or ::= bit_xor ( "\|" bit_xor )*` |
| 4 | `BitwiseXor`| `^` | Left-to-right | `bit_xor ::= bit_and ( "^" bit_and )*` |
| 5 | `BitwiseAnd`| `&` | Left-to-right | `bit_and ::= eq ( "&" eq )*` |
| 6 | `Equality` | `==`, `!=` | Left-to-right | `eq ::= rel ( ("==" \| "!=") rel )*` |
| 7 | `Relational`| `<`, `<=`, `>`, `>=` | Left-to-right | `rel ::= shift ( ("<" \| "<=" \| ">" \| ">=") shift )*` |
| 8 | `Shift` | `<<`, `>>` | Left-to-right | `shift ::= add ( ("<<" \| ">>") add )*` |
| 9 | `Additive` | `+`, `-` | Left-to-right | `add ::= mul ( ("+" \| "-") mul )*` |
| 10 | `Multiplicative` | `*`, `/`, `%` | Left-to-right | `mul ::= unary ( ("*" \| "/" \| "%") unary )*` |
| 11 | `Unary` | `-`, `!`, `~` | Right-to-left | `unary ::= ("-" \| "!" \| "~") unary \| postfix` |
| 12 | `Postfix` | `[` index `]` | Left-to-right | `postfix ::= primary [ "[" expr "]" ]` |
| 13 (Highest)| `Primary` | Literals, idents, calls, `(expr)` | N/A | `primary ::= INT \| FLOAT \| BOOL \| IDENT [ "(" args ")" ] \| "(" expr ")"` |

In recursive descent, each rule `A ::= B ( op B )*` is implemented as a simple loop:
```cpp
ExprPtr parseAdditive() {
    auto lhs = parseMultiplicative();
    while (match(Plus, Minus)) {
        auto op = previousToken();
        auto rhs = parseMultiplicative();
        lhs = makeBinary(op, lhs, rhs);
    }
    return lhs;
}
```
This loop naturally builds a left-associative AST without recursion overhead or stack exhaustion.

---

## 3. Ambiguities & Disambiguation Rules

### The Dangling-Else Ambiguity
Consider:
```c
if (c1) if (c2) s1 else s2
```
In an unambiguous context-free grammar, `else s2` can belong to either:
1. The outer `if (c1)`
2. The inner `if (c2)`

**Resolution in KC:**
* The grammar resolves this via **greedy association** (standard C/C++ convention): an `else` binds to the nearest preceding unmatched `if`.
* In recursive-descent parsing: when parsing `if_stmt`, if the current token is `else`, the parser immediately consumes it as the `else` branch of the *current* `if` statement.
* In `gate-lab`, this demonstrates a shift/reduce or LL(1) FIRST/FOLLOW conflict where `else` $\in \text{FOLLOW}(\text{stmt}) \cap \text{FIRST}(\text{else\_part})$.

---

## 4. GATE Compiler Design Concepts

1. **FIRST Sets:** The set of terminal symbols that can appear as the first symbol of a string derived from a non-terminal.
2. **FOLLOW Sets:** The set of terminal symbols that can appear immediately to the right of a non-terminal in some sentential form.
3. **LL(1) Condition:** A grammar is LL(1) if and only if for every production $A \to \alpha \mid \beta$:
   * $\text{FIRST}(\alpha) \cap \text{FIRST}(\beta) = \emptyset$
   * If $\epsilon \in \text{FIRST}(\beta)$, then $\text{FIRST}(\alpha) \cap \text{FOLLOW}(A) = \emptyset$.

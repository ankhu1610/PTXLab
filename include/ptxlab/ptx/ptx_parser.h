#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "ptxlab/ptx/ptx_ast.h"
#include "ptxlab/support/diagnostic.h"

namespace ptxlab::ptx {

enum class PTXTokenKind {
    Eof,
    Directive,      // e.g. .version, .target, .entry, .param, .reg, .b32, etc.
    Identifier,     // e.g. saxpy, _param_x, add.s32, ld.global.f32, bra, ret
    Register,       // e.g. %r0, %f1, %rd2, %p0, %rd_offset, %tid.x
    IntLiteral,     // e.g. 64, 4, -1
    FloatLiteral,   // e.g. 2.5, 0.0, 7.0
    HexFloatLiteral,// e.g. 0f3F800000
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    LBracket,       // [
    RBracket,       // ]
    LAngle,         // <
    RAngle,         // >
    Comma,          // ,
    Semicolon,      // ;
    Colon,          // :
    At,             // @
    Exclamation,    // !
    Plus,           // +
    Minus,          // -
};

struct PTXToken {
    PTXTokenKind kind{PTXTokenKind::Eof};
    std::string text;
    int64_t intVal{0};
    float floatVal{0.0f};
    int line{1};
    int col{1};
};

class PTXParser {
public:
    explicit PTXParser(std::string_view source, DiagnosticEngine* diag = nullptr);

    std::unique_ptr<PTXModule> parse();

    bool hasErrors() const { return !errors_.empty(); }
    const std::vector<std::string>& errors() const { return errors_; }

    static std::unique_ptr<PTXModule> parseString(std::string_view source, std::string* errorOut = nullptr);

private:
    void tokenize();
    const PTXToken& peek() const;
    const PTXToken& advance();
    bool match(PTXTokenKind kind);
    bool expect(PTXTokenKind kind, std::string_view context);
    void error(std::string msg, int line = 0, int col = 0);

    bool parseDirectives(PTXModule& module);
    std::unique_ptr<PTXKernel> parseKernel();
    bool parseParamDecl(PTXKernel& kernel);
    bool parseRegDecl(PTXKernel& kernel);
    bool parseStatement(PTXKernel& kernel);
    bool parseInstruction(PTXInstruction& inst);
    bool parseOperand(PTXOperand& op);

    std::string_view source_;
    DiagnosticEngine* diag_{nullptr};
    std::vector<PTXToken> tokens_;
    size_t cursor_{0};
    std::vector<std::string> errors_;
};

} // namespace ptxlab::ptx

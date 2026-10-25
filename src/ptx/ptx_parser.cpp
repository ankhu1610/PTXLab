#include "ptxlab/ptx/ptx_parser.h"
#include <cctype>
#include <charconv>
#include <cstring>
#include <sstream>

namespace ptxlab::ptx {

PTXParser::PTXParser(std::string_view source, DiagnosticEngine* diag)
    : source_(source), diag_(diag) {
    tokenize();
}

void PTXParser::error(std::string msg, int line, int col) {
    errors_.push_back(msg);
    if (diag_) {
        diag_->error(SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)}, std::move(msg));
    }
}

void PTXParser::tokenize() {
    size_t i = 0;
    int line = 1;
    int col = 1;

    auto advanceChar = [&]() -> char {
        char c = source_[i++];
        if (c == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        return c;
    };

    while (i < source_.size()) {
        char c = source_[i];

        // Whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            advanceChar();
            continue;
        }

        // Line comment //
        if (c == '/' && i + 1 < source_.size() && source_[i + 1] == '/') {
            advanceChar();
            advanceChar();
            while (i < source_.size() && source_[i] != '\n') {
                advanceChar();
            }
            continue;
        }

        // Block comment /* ... */
        if (c == '/' && i + 1 < source_.size() && source_[i + 1] == '*') {
            advanceChar();
            advanceChar();
            while (i + 1 < source_.size() && !(source_[i] == '*' && source_[i + 1] == '/')) {
                advanceChar();
            }
            if (i + 1 < source_.size()) {
                advanceChar(); // *
                advanceChar(); // /
            }
            continue;
        }

        int tokLine = line;
        int tokCol = col;

        // Punctuation
        if (c == '(') { advanceChar(); tokens_.push_back({PTXTokenKind::LParen, "(", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == ')') { advanceChar(); tokens_.push_back({PTXTokenKind::RParen, ")", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '{') { advanceChar(); tokens_.push_back({PTXTokenKind::LBrace, "{", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '}') { advanceChar(); tokens_.push_back({PTXTokenKind::RBrace, "}", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '[') { advanceChar(); tokens_.push_back({PTXTokenKind::LBracket, "[", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == ']') { advanceChar(); tokens_.push_back({PTXTokenKind::RBracket, "]", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '<') { advanceChar(); tokens_.push_back({PTXTokenKind::LAngle, "<", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '>') { advanceChar(); tokens_.push_back({PTXTokenKind::RAngle, ">", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == ',') { advanceChar(); tokens_.push_back({PTXTokenKind::Comma, ",", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == ';') { advanceChar(); tokens_.push_back({PTXTokenKind::Semicolon, ";", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == ':') { advanceChar(); tokens_.push_back({PTXTokenKind::Colon, ":", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '@') { advanceChar(); tokens_.push_back({PTXTokenKind::At, "@", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '!') { advanceChar(); tokens_.push_back({PTXTokenKind::Exclamation, "!", 0, 0.0f, tokLine, tokCol}); continue; }
        if (c == '+') { advanceChar(); tokens_.push_back({PTXTokenKind::Plus, "+", 0, 0.0f, tokLine, tokCol}); continue; }

        // Register: begins with %
        if (c == '%') {
            std::string regStr;
            regStr += advanceChar(); // %
            while (i < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[i])) ||
                                          source_[i] == '_' || source_[i] == '$' || source_[i] == '.')) {
                regStr += advanceChar();
            }
            tokens_.push_back({PTXTokenKind::Register, regStr, 0, 0.0f, tokLine, tokCol});
            continue;
        }

        // Directives: begins with . followed by alpha
        if (c == '.' && i + 1 < source_.size() && (std::isalpha(static_cast<unsigned char>(source_[i + 1])) || source_[i + 1] == '_')) {
            std::string dirStr;
            dirStr += advanceChar(); // .
            while (i < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[i])) || source_[i] == '_')) {
                dirStr += advanceChar();
            }
            tokens_.push_back({PTXTokenKind::Directive, dirStr, 0, 0.0f, tokLine, tokCol});
            continue;
        }

        // Hex float literal: 0f or 0F followed by 8 hex digits
        if (c == '0' && i + 1 < source_.size() && (source_[i + 1] == 'f' || source_[i + 1] == 'F')) {
            // Check if followed by hex digits
            size_t probe = i + 2;
            int hexCount = 0;
            while (probe < source_.size() && std::isxdigit(static_cast<unsigned char>(source_[probe]))) {
                probe++;
                hexCount++;
            }
            if (hexCount == 8) {
                std::string hexLit;
                hexLit += advanceChar(); // 0
                hexLit += advanceChar(); // f
                std::string hexDigits;
                for (int k = 0; k < 8; ++k) {
                    char digit = advanceChar();
                    hexLit += digit;
                    hexDigits += digit;
                }
                uint32_t bits = static_cast<uint32_t>(std::stoul(hexDigits, nullptr, 16));
                float fval = 0.0f;
                std::memcpy(&fval, &bits, sizeof(float));
                tokens_.push_back({PTXTokenKind::HexFloatLiteral, hexLit, static_cast<int64_t>(bits), fval, tokLine, tokCol});
                continue;
            }
        }

        // Hex integer: 0x or 0X
        if (c == '0' && i + 1 < source_.size() && (source_[i + 1] == 'x' || source_[i + 1] == 'X')) {
            std::string hexStr;
            hexStr += advanceChar(); // 0
            hexStr += advanceChar(); // x
            while (i < source_.size() && std::isxdigit(static_cast<unsigned char>(source_[i]))) {
                hexStr += advanceChar();
            }
            int64_t val = static_cast<int64_t>(std::stoull(hexStr, nullptr, 16));
            tokens_.push_back({PTXTokenKind::IntLiteral, hexStr, val, static_cast<float>(val), tokLine, tokCol});
            continue;
        }

        // Negative numbers or Minus
        if (c == '-') {
            if (i + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i + 1]))) {
                advanceChar(); // -
                std::string numStr = "-";
                bool isFloat = false;
                while (i < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i]))) {
                    numStr += advanceChar();
                }
                if (i < source_.size() && source_[i] == '.' &&
                    i + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i + 1]))) {
                    isFloat = true;
                    numStr += advanceChar(); // .
                    while (i < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i]))) {
                        numStr += advanceChar();
                    }
                }
                if (i < source_.size() && (source_[i] == 'f' || source_[i] == 'F')) {
                    isFloat = true;
                    advanceChar(); // suffix f
                }
                if (isFloat) {
                    float fval = std::stof(numStr);
                    tokens_.push_back({PTXTokenKind::FloatLiteral, numStr, static_cast<int64_t>(fval), fval, tokLine, tokCol});
                } else {
                    int64_t ival = std::stoll(numStr);
                    tokens_.push_back({PTXTokenKind::IntLiteral, numStr, ival, static_cast<float>(ival), tokLine, tokCol});
                }
                continue;
            } else {
                advanceChar();
                tokens_.push_back({PTXTokenKind::Minus, "-", 0, 0.0f, tokLine, tokCol});
                continue;
            }
        }

        // Numbers (decimal integers or decimal floats)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string numStr;
            bool isFloat = false;
            while (i < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i]))) {
                numStr += advanceChar();
            }
            if (i < source_.size() && source_[i] == '.' &&
                i + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i + 1]))) {
                isFloat = true;
                numStr += advanceChar(); // .
                while (i < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i]))) {
                    numStr += advanceChar();
                }
            }
            if (i < source_.size() && (source_[i] == 'f' || source_[i] == 'F')) {
                isFloat = true;
                advanceChar(); // suffix f
            }
            if (isFloat) {
                float fval = std::stof(numStr);
                tokens_.push_back({PTXTokenKind::FloatLiteral, numStr, static_cast<int64_t>(fval), fval, tokLine, tokCol});
            } else {
                int64_t ival = std::stoll(numStr);
                tokens_.push_back({PTXTokenKind::IntLiteral, numStr, ival, static_cast<float>(ival), tokLine, tokCol});
            }
            continue;
        }

        // Identifiers and dotted mnemonics: [a-zA-Z_$][a-zA-Z0-9_$.]*
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            std::string identStr;
            identStr += advanceChar();
            while (i < source_.size()) {
                char ch = source_[i];
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$') {
                    identStr += advanceChar();
                } else if (ch == '.' && i + 1 < source_.size() &&
                           (std::isalpha(static_cast<unsigned char>(source_[i + 1])) || source_[i + 1] == '_')) {
                    identStr += advanceChar(); // .
                } else {
                    break;
                }
            }
            tokens_.push_back({PTXTokenKind::Identifier, identStr, 0, 0.0f, tokLine, tokCol});
            continue;
        }

        // Unknown character
        std::string unk(1, advanceChar());
        error("Unexpected character '" + unk + "' in PTX source", tokLine, tokCol);
    }

    tokens_.push_back({PTXTokenKind::Eof, "", 0, 0.0f, line, col});
}

const PTXToken& PTXParser::peek() const {
    if (cursor_ < tokens_.size()) return tokens_[cursor_];
    return tokens_.back();
}

const PTXToken& PTXParser::advance() {
    if (cursor_ < tokens_.size()) {
        return tokens_[cursor_++];
    }
    return tokens_.back();
}

bool PTXParser::match(PTXTokenKind kind) {
    if (peek().kind == kind) {
        advance();
        return true;
    }
    return false;
}

bool PTXParser::expect(PTXTokenKind kind, std::string_view context) {
    if (peek().kind == kind) {
        advance();
        return true;
    }
    error("Expected token in " + std::string(context) + ", got '" + peek().text + "'", peek().line, peek().col);
    return false;
}

std::unique_ptr<PTXModule> PTXParser::parseString(std::string_view source, std::string* errorOut) {
    PTXParser parser(source);
    auto mod = parser.parse();
    if (parser.hasErrors()) {
        if (errorOut) {
            std::ostringstream ss;
            for (const auto& err : parser.errors()) {
                ss << err << "\n";
            }
            *errorOut = ss.str();
        }
        return nullptr;
    }
    return mod;
}

std::unique_ptr<PTXModule> PTXParser::parse() {
    auto module = std::make_unique<PTXModule>();

    // Parse top-level directives: .version, .target, .address_size
    if (!parseDirectives(*module)) {
        return nullptr;
    }

    // Parse kernels
    while (peek().kind != PTXTokenKind::Eof) {
        if (peek().kind == PTXTokenKind::Directive) {
            if (peek().text == ".visible" || peek().text == ".entry") {
                auto kernel = parseKernel();
                if (!kernel) return nullptr;
                module->addKernel(std::move(kernel));
                continue;
            } else if (peek().text == ".version" || peek().text == ".target" || peek().text == ".address_size") {
                // Secondary directive or update
                parseDirectives(*module);
                continue;
            }
        }
        error("Unexpected token at top level: '" + peek().text + "'", peek().line, peek().col);
        advance();
    }

    if (hasErrors()) return nullptr;
    return module;
}

bool PTXParser::parseDirectives(PTXModule& module) {
    while (peek().kind == PTXTokenKind::Directive) {
        const std::string& dir = peek().text;
        if (dir == ".version") {
            advance();
            const auto& valTok = peek();
            if (valTok.kind == PTXTokenKind::FloatLiteral || valTok.kind == PTXTokenKind::IntLiteral || valTok.kind == PTXTokenKind::Identifier) {
                module.setVersion(valTok.text);
                advance();
            } else {
                error("Expected version string after .version", valTok.line, valTok.col);
                return false;
            }
        } else if (dir == ".target") {
            advance();
            const auto& valTok = peek();
            if (valTok.kind == PTXTokenKind::Identifier) {
                module.setTarget(valTok.text);
                advance();
            } else {
                error("Expected target identifier after .target", valTok.line, valTok.col);
                return false;
            }
        } else if (dir == ".address_size") {
            advance();
            const auto& valTok = peek();
            if (valTok.kind == PTXTokenKind::IntLiteral) {
                module.setAddressSize(static_cast<int>(valTok.intVal));
                advance();
            } else {
                error("Expected address size integer after .address_size", valTok.line, valTok.col);
                return false;
            }
        } else {
            // Reached kernel or other section
            break;
        }
    }
    return true;
}

std::unique_ptr<PTXKernel> PTXParser::parseKernel() {
    // Optional .visible
    if (peek().kind == PTXTokenKind::Directive && peek().text == ".visible") {
        advance();
    }

    if (!expect(PTXTokenKind::Directive, "kernel declaration (.entry expected)")) {
        return nullptr;
    }
    // Check .entry
    if (tokens_[cursor_ - 1].text != ".entry") {
        error("Expected .entry directive", peek().line, peek().col);
        return nullptr;
    }

    const auto& nameTok = peek();
    if (nameTok.kind != PTXTokenKind::Identifier) {
        error("Expected kernel entry name identifier", nameTok.line, nameTok.col);
        return nullptr;
    }
    std::string kernelName = nameTok.text;
    advance();

    auto kernel = std::make_unique<PTXKernel>(kernelName);

    // Param list ( ... )
    if (!expect(PTXTokenKind::LParen, "opening '(' for parameter list")) return nullptr;

    if (peek().kind != PTXTokenKind::RParen) {
        while (true) {
            if (!parseParamDecl(*kernel)) return nullptr;
            if (match(PTXTokenKind::Comma)) {
                continue;
            }
            break;
        }
    }

    if (!expect(PTXTokenKind::RParen, "closing ')' for parameter list")) return nullptr;

    // Body { ... }
    if (!expect(PTXTokenKind::LBrace, "opening '{' for kernel body")) return nullptr;

    while (peek().kind != PTXTokenKind::RBrace && peek().kind != PTXTokenKind::Eof) {
        if (peek().kind == PTXTokenKind::Directive && peek().text == ".reg") {
            if (!parseRegDecl(*kernel)) return nullptr;
            continue;
        }

        if (!parseStatement(*kernel)) {
            return nullptr;
        }
    }

    if (!expect(PTXTokenKind::RBrace, "closing '}' for kernel body")) return nullptr;

    return kernel;
}

static PTXType parseTypeFromDirective(std::string_view dir) {
    if (dir == ".u32") return PTXType::U32;
    if (dir == ".s32") return PTXType::S32;
    if (dir == ".b32") return PTXType::B32;
    if (dir == ".f32") return PTXType::F32;
    if (dir == ".u64") return PTXType::U64;
    if (dir == ".s64") return PTXType::S64;
    if (dir == ".b64") return PTXType::B64;
    if (dir == ".pred") return PTXType::Pred;
    return PTXType::Unknown;
}

bool PTXParser::parseParamDecl(PTXKernel& kernel) {
    // .param .<type> <name>
    if (!expect(PTXTokenKind::Directive, ".param directive")) return false;
    if (tokens_[cursor_ - 1].text != ".param") {
        error("Expected .param directive", peek().line, peek().col);
        return false;
    }

    if (peek().kind != PTXTokenKind::Directive) {
        error("Expected param type directive (e.g. .u32, .f32, .u64)", peek().line, peek().col);
        return false;
    }
    PTXType type = parseTypeFromDirective(peek().text);
    if (type == PTXType::Unknown) {
        error("Unknown parameter type: " + peek().text, peek().line, peek().col);
        return false;
    }
    advance();

    if (peek().kind != PTXTokenKind::Identifier) {
        error("Expected parameter name identifier", peek().line, peek().col);
        return false;
    }
    std::string name = peek().text;
    int line = peek().line;
    advance();

    kernel.addParam({name, type, line});
    return true;
}

bool PTXParser::parseRegDecl(PTXKernel& kernel) {
    // .reg .<type> <reg_spec> [, <reg_spec> ...];
    advance(); // consume .reg

    if (peek().kind != PTXTokenKind::Directive) {
        error("Expected register type directive (e.g. .b32, .f32, .b64, .pred)", peek().line, peek().col);
        return false;
    }
    PTXType type = parseTypeFromDirective(peek().text);
    if (type == PTXType::Unknown) {
        error("Unknown register type: " + peek().text, peek().line, peek().col);
        return false;
    }
    advance();

    while (true) {
        int line = peek().line;
        if (peek().kind != PTXTokenKind::Register && peek().kind != PTXTokenKind::Identifier) {
            error("Expected register name or prefix in .reg declaration", peek().line, peek().col);
            return false;
        }

        std::string regName = peek().text;
        advance();

        if (match(PTXTokenKind::LAngle)) {
            // Range syntax: %r<10>
            if (peek().kind != PTXTokenKind::IntLiteral) {
                error("Expected register count integer in angle brackets", peek().line, peek().col);
                return false;
            }
            int count = static_cast<int>(peek().intVal);
            advance();
            if (!expect(PTXTokenKind::RAngle, "closing '>' for register range")) return false;

            kernel.addRegDecl({type, regName, count, true, line});
        } else {
            // Single register: %rd_offset or %r_param_pred
            kernel.addRegDecl({type, regName, 1, false, line});
        }

        if (match(PTXTokenKind::Comma)) {
            continue;
        }
        break;
    }

    if (!expect(PTXTokenKind::Semicolon, "semicolon ';' terminating .reg declaration")) return false;
    return true;
}

bool PTXParser::parseStatement(PTXKernel& kernel) {
    int line = peek().line;

    // Check if label: identifier followed by colon
    if (peek().kind == PTXTokenKind::Identifier && cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].kind == PTXTokenKind::Colon) {
        std::string labelName = peek().text;
        advance(); // identifier
        advance(); // colon :
        kernel.addStatement(PTXStatement::makeLabel(labelName, line));
        return true;
    }

    PTXInstruction inst;
    inst.line = line;

    // Predication: @%p0 or @!%p0
    if (match(PTXTokenKind::At)) {
        bool neg = match(PTXTokenKind::Exclamation);
        if (peek().kind != PTXTokenKind::Register) {
            error("Expected predicate register after '@'", peek().line, peek().col);
            return false;
        }
        inst.predicate = PTXPredicate{peek().text, neg};
        advance();
    }

    if (!parseInstruction(inst)) {
        return false;
    }

    kernel.addStatement(PTXStatement::makeInstruction(std::move(inst), line));
    return true;
}

bool PTXParser::parseInstruction(PTXInstruction& inst) {
    if (peek().kind != PTXTokenKind::Identifier) {
        error("Expected instruction opcode mnemonic, got '" + peek().text + "'", peek().line, peek().col);
        return false;
    }

    std::string mnemonic = peek().text;
    advance();

    // Parse opcode and modifiers from dotted mnemonic
    if (mnemonic == "ret") {
        inst.opcode = PTXOpcode::Ret;
    } else if (mnemonic == "bra") {
        inst.opcode = PTXOpcode::Bra;
    } else if (mnemonic.rfind("mov.", 0) == 0) {
        inst.opcode = PTXOpcode::Mov;
        std::string mod = mnemonic.substr(4);
        if (mod == "u32" || mod == "s32") inst.typeMod = PTXType::U32;
        else if (mod == "f32") inst.typeMod = PTXType::F32;
        else if (mod == "pred") inst.typeMod = PTXType::Pred;
        else if (mod == "u64" || mod == "b64") inst.typeMod = PTXType::U64;
    } else if (mnemonic.rfind("ld.param.", 0) == 0) {
        inst.opcode = PTXOpcode::LdParam;
        std::string mod = mnemonic.substr(9);
        if (mod == "u32" || mod == "s32") inst.typeMod = PTXType::U32;
        else if (mod == "f32") inst.typeMod = PTXType::F32;
        else if (mod == "u64" || mod == "b64") inst.typeMod = PTXType::U64;
    } else if (mnemonic == "cvta.to.global.u64") {
        inst.opcode = PTXOpcode::Cvta;
        inst.typeMod = PTXType::U64;
    } else if (mnemonic.rfind("cvt.", 0) == 0) {
        inst.opcode = PTXOpcode::Cvt;
        if (mnemonic == "cvt.rn.f32.s32") {
            inst.rounding = PTXRoundingMode::Rn;
            inst.typeMod = PTXType::F32;
            inst.srcTypeMod = PTXType::S32;
        } else if (mnemonic == "cvt.rzi.s32.f32") {
            inst.rounding = PTXRoundingMode::Rzi;
            inst.typeMod = PTXType::S32;
            inst.srcTypeMod = PTXType::F32;
        } else {
            error("Unsupported cvt variant: " + mnemonic, peek().line, peek().col);
            return false;
        }
    } else if (mnemonic.rfind("ld.global.", 0) == 0) {
        inst.opcode = PTXOpcode::LdGlobal;
        std::string mod = mnemonic.substr(10);
        if (mod == "s32" || mod == "u32") inst.typeMod = PTXType::S32;
        else if (mod == "f32") inst.typeMod = PTXType::F32;
    } else if (mnemonic.rfind("st.global.", 0) == 0) {
        inst.opcode = PTXOpcode::StGlobal;
        std::string mod = mnemonic.substr(10);
        if (mod == "s32" || mod == "u32") inst.typeMod = PTXType::S32;
        else if (mod == "f32") inst.typeMod = PTXType::F32;
    } else if (mnemonic == "mul.wide.s32") {
        inst.opcode = PTXOpcode::MulWide;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "mul.lo.s32") {
        inst.opcode = PTXOpcode::Mul;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "add.s64") {
        inst.opcode = PTXOpcode::Add;
        inst.typeMod = PTXType::S64;
    } else if (mnemonic == "add.s32") {
        inst.opcode = PTXOpcode::Add;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "sub.s32") {
        inst.opcode = PTXOpcode::Sub;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "div.s32") {
        inst.opcode = PTXOpcode::Div;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "rem.s32") {
        inst.opcode = PTXOpcode::Rem;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "and.b32") {
        inst.opcode = PTXOpcode::And;
        inst.typeMod = PTXType::B32;
    } else if (mnemonic == "or.b32") {
        inst.opcode = PTXOpcode::Or;
        inst.typeMod = PTXType::B32;
    } else if (mnemonic == "xor.b32") {
        inst.opcode = PTXOpcode::Xor;
        inst.typeMod = PTXType::B32;
    } else if (mnemonic == "shl.b32") {
        inst.opcode = PTXOpcode::Shl;
        inst.typeMod = PTXType::B32;
    } else if (mnemonic == "shr.s32") {
        inst.opcode = PTXOpcode::Shr;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "add.rn.f32") {
        inst.opcode = PTXOpcode::Add;
        inst.rounding = PTXRoundingMode::Rn;
        inst.typeMod = PTXType::F32;
    } else if (mnemonic == "sub.rn.f32") {
        inst.opcode = PTXOpcode::Sub;
        inst.rounding = PTXRoundingMode::Rn;
        inst.typeMod = PTXType::F32;
    } else if (mnemonic == "mul.rn.f32") {
        inst.opcode = PTXOpcode::Mul;
        inst.rounding = PTXRoundingMode::Rn;
        inst.typeMod = PTXType::F32;
    } else if (mnemonic == "div.rn.f32") {
        inst.opcode = PTXOpcode::Div;
        inst.rounding = PTXRoundingMode::Rn;
        inst.typeMod = PTXType::F32;
    } else if (mnemonic == "neg.s32") {
        inst.opcode = PTXOpcode::Neg;
        inst.typeMod = PTXType::S32;
    } else if (mnemonic == "neg.f32") {
        inst.opcode = PTXOpcode::Neg;
        inst.typeMod = PTXType::F32;
    } else if (mnemonic == "not.b32") {
        inst.opcode = PTXOpcode::Not;
        inst.typeMod = PTXType::B32;
    } else if (mnemonic == "not.pred") {
        inst.opcode = PTXOpcode::Not;
        inst.typeMod = PTXType::Pred;
    } else if (mnemonic.rfind("setp.", 0) == 0) {
        inst.opcode = PTXOpcode::Setp;
        // Format: setp.<cond>.<type> e.g. setp.eq.s32 or setp.lt.f32
        std::string rest = mnemonic.substr(5);
        size_t dotPos = rest.find('.');
        if (dotPos == std::string::npos) {
            error("Malformed setp instruction: " + mnemonic, peek().line, peek().col);
            return false;
        }
        std::string condStr = rest.substr(0, dotPos);
        std::string typeStr = rest.substr(dotPos + 1);

        if (condStr == "eq") inst.cmpCond = PTXCmpCondition::Eq;
        else if (condStr == "ne") inst.cmpCond = PTXCmpCondition::Ne;
        else if (condStr == "lt") inst.cmpCond = PTXCmpCondition::Lt;
        else if (condStr == "le") inst.cmpCond = PTXCmpCondition::Le;
        else if (condStr == "gt") inst.cmpCond = PTXCmpCondition::Gt;
        else if (condStr == "ge") inst.cmpCond = PTXCmpCondition::Ge;
        else {
            error("Unknown comparison condition in setp: " + condStr, peek().line, peek().col);
            return false;
        }

        if (typeStr == "s32" || typeStr == "u32") inst.typeMod = PTXType::S32;
        else if (typeStr == "f32") inst.typeMod = PTXType::F32;
        else {
            error("Unsupported setp comparison type: " + typeStr, peek().line, peek().col);
            return false;
        }
    } else {
        error("Unknown PTX instruction mnemonic: '" + mnemonic + "'", peek().line, peek().col);
        return false;
    }

    // Operands (if any) until semicolon ';'
    if (peek().kind != PTXTokenKind::Semicolon) {
        while (true) {
            PTXOperand op;
            if (!parseOperand(op)) return false;
            inst.operands.push_back(std::move(op));

            if (match(PTXTokenKind::Comma)) {
                continue;
            }
            break;
        }
    }

    if (!expect(PTXTokenKind::Semicolon, "semicolon ';' terminating instruction")) return false;
    return true;
}

bool PTXParser::parseOperand(PTXOperand& op) {
    // Memory reference: [ ... ]
    if (match(PTXTokenKind::LBracket)) {
        if (peek().kind == PTXTokenKind::Register || peek().kind == PTXTokenKind::Identifier) {
            std::string base = peek().text;
            advance();
            int64_t offset = 0;
            if (match(PTXTokenKind::Plus)) {
                if (peek().kind == PTXTokenKind::IntLiteral) {
                    offset = peek().intVal;
                    advance();
                } else {
                    error("Expected integer offset after '+' in memory operand", peek().line, peek().col);
                    return false;
                }
            } else if (match(PTXTokenKind::Minus)) {
                if (peek().kind == PTXTokenKind::IntLiteral) {
                    offset = -peek().intVal;
                    advance();
                } else {
                    error("Expected integer offset after '-' in memory operand", peek().line, peek().col);
                    return false;
                }
            }
            if (!expect(PTXTokenKind::RBracket, "closing ']' in memory operand")) return false;
            op = PTXOperand::mem(std::move(base), offset);
            return true;
        } else {
            error("Expected register or identifier base in memory operand", peek().line, peek().col);
            return false;
        }
    }

    // Register or special register: %...
    if (peek().kind == PTXTokenKind::Register) {
        std::string regName = peek().text;
        advance();
        if (regName == "%tid.x") {
            op = PTXOperand::special(PTXSpecialReg::TidX);
        } else if (regName == "%ctaid.x") {
            op = PTXOperand::special(PTXSpecialReg::CtaidX);
        } else if (regName == "%ntid.x") {
            op = PTXOperand::special(PTXSpecialReg::NtidX);
        } else {
            op = PTXOperand::reg(std::move(regName));
        }
        return true;
    }

    // Hex float literal
    if (peek().kind == PTXTokenKind::HexFloatLiteral) {
        op = PTXOperand::immFloat(peek().floatVal);
        advance();
        return true;
    }

    // Decimal float literal
    if (peek().kind == PTXTokenKind::FloatLiteral) {
        op = PTXOperand::immFloat(peek().floatVal);
        advance();
        return true;
    }

    // Integer literal
    if (peek().kind == PTXTokenKind::IntLiteral) {
        op = PTXOperand::immInt(peek().intVal);
        advance();
        return true;
    }

    // Label or identifier reference
    if (peek().kind == PTXTokenKind::Identifier) {
        op = PTXOperand::label(peek().text);
        advance();
        return true;
    }

    error("Unexpected operand token: '" + peek().text + "'", peek().line, peek().col);
    return false;
}

} // namespace ptxlab::ptx

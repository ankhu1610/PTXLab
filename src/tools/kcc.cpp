#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ptxlab/kc/ast_printer.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"
#include "ptxlab/version.h"

void printUsage(const char* progName) {
    std::cout << ptxlab::getBanner() << "\n\n"
              << "Usage: " << progName << " [options] <input.kc>\n\n"
              << "Options:\n"
              << "  -h, --help       Show this help message\n"
              << "  -v, --version    Show compiler version\n"
              << "  --dump-tokens    [M1] Dump token stream\n"
              << "  --dump-ast       [M2] Dump abstract syntax tree\n"
              << "  --run-ast        [M3] Execute with AST interpreter (O1)\n"
              << "  --dump-ir        [M4] Dump three-address code IR\n"
              << "  --emit-ptx       [M5] Generate PTX code to stdout or file\n";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        printUsage(argv[0]);
        return 0;
    }

    bool dumpTokens = false;
    bool dumpAst = false;
    std::string inputFile;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == argv[0]) {
            continue;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "PTXLab Compiler (kcc) version " << ptxlab::getVersion() << "\n";
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--dump-tokens") {
            dumpTokens = true;
        } else if (arg == "--dump-ast") {
            dumpAst = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "kcc: error: unrecognized command-line option '" << arg << "'\n";
            return 1;
        } else {
            if (!inputFile.empty()) {
                std::cerr << "kcc: error: multiple input files are not supported in MVP: '"
                          << inputFile << "' and '" << arg << "'\n";
                return 1;
            }
            inputFile = arg;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "kcc: error: no input files\n";
        return 1;
    }

    std::ifstream file(inputFile);
    if (!file.is_open()) {
        std::cerr << "kcc: error: cannot open input file '" << inputFile << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    ptxlab::SourceManager sourceMgr(inputFile, std::move(sourceCode));
    ptxlab::DiagnosticEngine diagEngine;
    ptxlab::Lexer lexer(sourceMgr, diagEngine);

    if (dumpTokens) {
        while (true) {
            ptxlab::Token tok = lexer.nextToken();
            std::cout << ptxlab::toString(tok.kind) << " '" << tok.text << "' ["
                      << tok.location.line << ":" << tok.location.column << "]";
            if (tok.kind == ptxlab::TokenKind::IntLiteral) {
                std::cout << " (val=" << tok.intVal << ")";
            } else if (tok.kind == ptxlab::TokenKind::FloatLiteral) {
                std::cout << " (val=" << tok.floatVal << ")";
            }
            std::cout << "\n";

            if (tok.is(ptxlab::TokenKind::Eof)) {
                break;
            }
        }

        if (diagEngine.hasErrors()) {
            for (const auto& diag : diagEngine.getDiagnostics()) {
                std::cerr << diagEngine.format(diag, sourceMgr);
            }
            return 1;
        }
        return 0;
    }

    if (dumpAst) {
        ptxlab::Parser parser(lexer, sourceMgr, diagEngine);
        auto program = parser.parseProgram();

        if (diagEngine.hasErrors()) {
            for (const auto& diag : diagEngine.getDiagnostics()) {
                std::cerr << diagEngine.format(diag, sourceMgr);
            }
            return 1;
        }

        if (program) {
            std::cout << ptxlab::ASTPrinter::dump(*program);
        }
        return 0;
    }

    std::cerr << "kcc: error: compilation beyond AST will be enabled in Milestone 3\n";
    return 1;
}

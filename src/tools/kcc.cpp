#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/kc/ast_printer.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
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
              << "  --check          [M3] Perform syntax & semantic type checking only\n"
              << "  --run-ast        [M3] Execute with AST interpreter (O1)\n"
              << "  --grid <N>       [M3] Number of thread blocks (default 1)\n"
              << "  --block <M>      [M3] Number of threads per block (default 1)\n"
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
    bool checkOnly = false;
    bool runAst = false;
    int gridDim = 1;
    int blockDim = 1;
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
        } else if (arg == "--check") {
            checkOnly = true;
        } else if (arg == "--run-ast") {
            runAst = true;
        } else if (arg == "--grid") {
            if (i + 1 >= args.size()) {
                std::cerr << "kcc: error: missing argument to '--grid'\n";
                return 1;
            }
            gridDim = std::stoi(args[++i]);
        } else if (arg == "--block") {
            if (i + 1 >= args.size()) {
                std::cerr << "kcc: error: missing argument to '--block'\n";
                return 1;
            }
            blockDim = std::stoi(args[++i]);
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

    ptxlab::Parser parser(lexer, sourceMgr, diagEngine);
    auto program = parser.parseProgram();

    if (diagEngine.hasErrors()) {
        for (const auto& diag : diagEngine.getDiagnostics()) {
            std::cerr << diagEngine.format(diag, sourceMgr);
        }
        return 1;
    }

    if (dumpAst) {
        if (program) {
            std::cout << ptxlab::ASTPrinter::dump(*program);
        }
        return 0;
    }

    // Semantic Analysis (Sema)
    ptxlab::Sema sema(sourceMgr, diagEngine);
    if (program) {
        sema.analyze(*program);
    }

    if (diagEngine.hasErrors()) {
        for (const auto& diag : diagEngine.getDiagnostics()) {
            std::cerr << diagEngine.format(diag, sourceMgr);
        }
        return 1;
    }

    if (checkOnly) {
        std::cout << "kcc: semantic check succeeded for '" << inputFile << "'\n";
        return 0;
    }

    if (runAst) {
        if (!program || program->kernels().empty()) {
            std::cerr << "kcc: error: no kernels found in program to execute\n";
            return 1;
        }

        ptxlab::BufferManager buffers;
        auto& kernel = *program->kernels()[0];

        // Prepare dummy/default inputs for testing launch
        std::unordered_map<std::string, ptxlab::KernelArg> args;
        int totalThreads = gridDim * blockDim;

        for (const auto& param : kernel.params()) {
            if (param.type().isPtr()) {
                if (param.type().isInt()) {
                    buffers.allocateI32(param.name(), std::vector<int32_t>(static_cast<size_t>(totalThreads), 0));
                } else {
                    buffers.allocateF32(param.name(), std::vector<float>(static_cast<size_t>(totalThreads), 0.0f));
                }
                args[param.name()] = ptxlab::KernelArg::fromBuffer(param.name());
            } else if (param.type().isInt()) {
                args[param.name()] = ptxlab::KernelArg::fromI32(totalThreads);
            } else if (param.type().isFloat()) {
                args[param.name()] = ptxlab::KernelArg::fromF32(1.0f);
            } else if (param.type().isBool()) {
                args[param.name()] = ptxlab::KernelArg::fromBool(true);
            }
        }

        try {
            ptxlab::ASTInterpreter interpreter(buffers);
            interpreter.launch1D(kernel, gridDim, blockDim, args);
            std::cout << "kcc: [O1 AST Interpreter] kernel '" << kernel.name()
                      << "' executed successfully (" << gridDim << " blocks x "
                      << blockDim << " threads = " << totalThreads << " total threads)\n";
            return 0;
        } catch (const ptxlab::UBTrap& trap) {
            std::cerr << "kcc: [O1 AST Interpreter TRAP]: " << trap.message() << "\n";
            if (trap.location().isValid()) {
                std::cerr << "  at " << inputFile << ":" << trap.location().line << ":" << trap.location().column << "\n";
            }
            return 1;
        }
    }

    std::cout << "kcc: verified '" << inputFile << "' successfully (use --dump-ast or --run-ast)\n";
    return 0;
}

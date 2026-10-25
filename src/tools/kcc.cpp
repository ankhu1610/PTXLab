#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ptxlab/ir/ir_interpreter.h"
#include "ptxlab/ir/ir_printer.h"
#include "ptxlab/ir/ir_verifier.h"
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/kc/ast_printer.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/ptx/ptx_emitter.h"
#include "ptxlab/ptx/ptx_ast.h"
#include "ptxlab/ptx/ptx_parser.h"
#include "ptxlab/ptx/ptx_validator.h"
#include "ptxlab/ptx/ptx_interpreter.h"
#include "ptxlab/gpu/gpu_runner.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"
#include "ptxlab/version.h"

void printUsage(const char* progName) {
    std::cout << ptxlab::getBanner() << "\n\n"
              << "Usage: " << progName << " [options] <input.kc|file.ptx>\n\n"
              << "Options:\n"
              << "  -h, --help       Show this help message\n"
              << "  -v, --version    Show compiler version\n"
              << "  -p, --pipeline   Trace full compiler flow (Tokens -> AST -> Sema -> IR -> Run -> PTX -> O3 -> GPU)\n"
              << "  --dump-tokens    [M1] Dump token stream\n"
              << "  --dump-ast       [M2] Dump abstract syntax tree\n"
              << "  --check          [M3] Perform syntax & semantic type checking only\n"
              << "  --run-ast        [M3] Execute with AST interpreter (O1)\n"
              << "  --dump-ir        [M4] Dump three-address code IR\n"
              << "  --run-ir         [M4] Execute with IR interpreter (O2)\n"
              << "  --emit-ptx       [M5] Generate PTX code to stdout or file\n"
              << "  --run-gpu        [M6] Execute on GPU via CUDA Driver API (O4)\n"
              << "  --run-ptx        [M7] Execute with PTX reference interpreter (O3)\n"
              << "  -o <file>        Output file for generated PTX (default: stdout)\n"
              << "  --grid <N>       Number of thread blocks (default 1)\n"
              << "  --block <M>      Number of threads per block (default 1)\n";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        printUsage(argv[0]);
        return 0;
    }

    bool pipeline = false;
    bool dumpTokens = false;
    bool dumpAst = false;
    bool checkOnly = false;
    bool runAst = false;
    bool dumpIr = false;
    bool runIr = false;
    bool emitPtx = false;
    bool runGpu = false;
    bool runPtx = false;
    int gridDim = 1;
    int blockDim = 1;
    std::string outputFile;
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
        } else if (arg == "-p" || arg == "--pipeline") {
            pipeline = true;
        } else if (arg == "--dump-tokens") {
            dumpTokens = true;
        } else if (arg == "--dump-ast") {
            dumpAst = true;
        } else if (arg == "--check") {
            checkOnly = true;
        } else if (arg == "--run-ast") {
            runAst = true;
        } else if (arg == "--dump-ir") {
            dumpIr = true;
        } else if (arg == "--run-ir") {
            runIr = true;
        } else if (arg == "--emit-ptx") {
            emitPtx = true;
        } else if (arg == "--run-gpu") {
            runGpu = true;
        } else if (arg == "--run-ptx") {
            runPtx = true;
        } else if (arg == "-o") {
            if (i + 1 >= args.size()) {
                std::cerr << "kcc: error: missing argument to '-o'\n";
                return 1;
            }
            outputFile = args[++i];
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

    // Check if input is direct PTX virtual assembly
    if (inputFile.size() >= 4 && inputFile.substr(inputFile.size() - 4) == ".ptx") {
        std::string parseErr;
        auto ptxMod = ptxlab::ptx::PTXParser::parseString(sourceCode, &parseErr);
        if (!ptxMod) {
            std::cerr << "kcc: error parsing PTX:\n" << parseErr << "\n";
            return 1;
        }
        ptxlab::ptx::PTXValidator validator;
        if (!validator.validate(*ptxMod)) {
            std::cerr << "kcc: PTX validation error(s):\n";
            for (const auto& err : validator.errors()) {
                std::cerr << "  " << err << "\n";
            }
            return 1;
        }
        if (checkOnly) {
            std::cout << "kcc: PTX module '" << inputFile << "' validated successfully.\n";
            return 0;
        }
        if (runPtx || pipeline) {
            if (ptxMod->kernels().empty()) {
                std::cerr << "kcc: error: no kernels found in PTX module\n";
                return 1;
            }
            const auto& kernel = *ptxMod->kernels()[0];
            ptxlab::BufferManager buffers;
            std::unordered_map<std::string, ptxlab::KernelArg> kargs;
            int totalThreads = gridDim * blockDim;

            for (const auto& param : kernel.params()) {
                std::string argKey = param.name;
                if (argKey.rfind("_param_", 0) == 0) argKey = argKey.substr(7);
                if (param.type == ptxlab::ptx::PTXType::U64) {
                    std::vector<float> data(static_cast<size_t>(totalThreads), 1.0f);
                    buffers.allocateF32(argKey, std::move(data));
                    kargs[argKey] = ptxlab::KernelArg::fromBuffer(argKey);
                } else if (param.type == ptxlab::ptx::PTXType::F32) {
                    kargs[argKey] = ptxlab::KernelArg::fromF32(2.5f);
                } else if (param.type == ptxlab::ptx::PTXType::Pred) {
                    kargs[argKey] = ptxlab::KernelArg::fromBool(true);
                } else {
                    kargs[argKey] = ptxlab::KernelArg::fromI32(totalThreads);
                }
            }
            ptxlab::ptx::PTXInterpreter interpreter(buffers);
            auto res = interpreter.launch(kernel, gridDim, blockDim, kargs);
            if (!res.success) {
                std::cerr << "kcc: [O3 PTX Interpreter ERROR]: " << res.error << "\n";
                return 1;
            }
            std::cout << "kcc: [O3 PTX Interpreter] kernel '" << kernel.name()
                      << "' executed successfully (" << gridDim << " blocks x " << blockDim << " threads = "
                      << totalThreads << " total threads) [Cycles: " << res.totalCycles << "]\n";
            return 0;
        }
        std::cout << "kcc: PTX file '" << inputFile << "' parsed and validated successfully (use --run-ptx to execute).\n";
        return 0;
    }

    ptxlab::SourceManager sourceMgr(inputFile, std::move(sourceCode));
    ptxlab::DiagnosticEngine diagEngine;
    ptxlab::Lexer lexer(sourceMgr, diagEngine);

    if (pipeline) {
        std::cout << "\n============================================================\n"
                  << " [STAGE 1] Lexical Analysis (Tokens)\n"
                  << "============================================================\n";
        ptxlab::Lexer tokenLexer(sourceMgr, diagEngine);
        while (true) {
            ptxlab::Token tok = tokenLexer.nextToken();
            std::cout << ptxlab::toString(tok.kind) << " '" << tok.text << "' ["
                      << tok.location.line << ":" << tok.location.column << "]";
            if (tok.kind == ptxlab::TokenKind::IntLiteral) {
                std::cout << " (val=" << tok.intVal << ")";
            } else if (tok.kind == ptxlab::TokenKind::FloatLiteral) {
                std::cout << " (val=" << tok.floatVal << ")";
            }
            std::cout << "\n";
            if (tok.is(ptxlab::TokenKind::Eof)) break;
        }

        if (diagEngine.hasErrors()) {
            std::cerr << "\n[!] Pipeline halted at Stage 1 due to lexical error(s):\n";
            for (const auto& diag : diagEngine.getDiagnostics()) {
                std::cerr << diagEngine.format(diag, sourceMgr);
            }
            return 1;
        }
        std::cout << "[PASS] Lexical Analysis succeeded without errors.\n";

        std::cout << "\n============================================================\n"
                  << " [STAGE 2] Syntax Analysis (Parser & Abstract Syntax Tree)\n"
                  << "============================================================\n";
        ptxlab::Lexer parseLexer(sourceMgr, diagEngine);
        ptxlab::Parser parser(parseLexer, sourceMgr, diagEngine);
        auto program = parser.parseProgram();

        if (diagEngine.hasErrors()) {
            std::cerr << "\n[!] Pipeline halted at Stage 2 due to syntax error(s):\n";
            for (const auto& diag : diagEngine.getDiagnostics()) {
                std::cerr << diagEngine.format(diag, sourceMgr);
            }
            return 1;
        }

        if (program) {
            std::cout << ptxlab::ASTPrinter::dump(*program);
        }
        std::cout << "[PASS] Syntax Analysis (AST construction) succeeded without errors.\n";

        std::cout << "\n============================================================\n"
                  << " [STAGE 3] Semantic Analysis (Types, Scopes & Symbols)\n"
                  << "============================================================\n";
        ptxlab::Sema sema(sourceMgr, diagEngine);
        if (program) {
            sema.analyze(*program);
        }

        if (diagEngine.hasErrors()) {
            std::cerr << "\n[!] Pipeline halted at Stage 3 due to semantic error(s):\n";
            for (const auto& diag : diagEngine.getDiagnostics()) {
                std::cerr << diagEngine.format(diag, sourceMgr);
            }
            return 1;
        }
        std::cout << "[PASS] Semantic Analysis and Type Checking passed (0 errors).\n";

        std::cout << "\n============================================================\n"
                  << " [STAGE 4] Intermediate Representation (Typed TAC CFG & Verifier)\n"
                  << "============================================================\n";
        ptxlab::ir::Lowering lowering;
        auto irMod = lowering.lower(*program);

        ptxlab::ir::IRVerifier verifier;
        if (!verifier.verify(*irMod)) {
            std::cerr << "\n[!] Pipeline halted at Stage 4 due to IR verification error(s):\n";
            for (const auto& err : verifier.errors()) {
                std::cerr << "  " << err.message << "\n";
            }
            return 1;
        }

        if (irMod) {
            std::cout << ptxlab::ir::IRPrinter::dump(*irMod);
        }
        std::cout << "[PASS] IR Lowering and CFG Verification succeeded (0 errors).\n";

        std::cout << "\n============================================================\n"
                  << " [STAGE 5] Reference Execution Simulation (O1 AST & O2 IR)\n"
                  << "============================================================\n";
        if (!program || program->kernels().empty()) {
            std::cerr << "kcc: error: no kernels found in program to execute\n";
            return 1;
        }

        ptxlab::BufferManager buffersO1;
        ptxlab::BufferManager buffersO2;
        auto& kernel = *program->kernels()[0];
        const auto& irFunc = *irMod->functions()[0];

        std::unordered_map<std::string, ptxlab::KernelArg> kArgsO1;
        std::unordered_map<std::string, ptxlab::KernelArg> kArgsO2;
        int totalThreads = gridDim * blockDim;

        for (const auto& param : kernel.params()) {
            if (param.type().isPtr()) {
                if (param.type().primitive == ptxlab::PrimitiveType::I32) {
                    std::vector<int32_t> initData(static_cast<size_t>(totalThreads), 0);
                    buffersO1.allocateI32(param.name(), initData);
                    buffersO2.allocateI32(param.name(), initData);
                } else {
                    std::vector<float> initData(static_cast<size_t>(totalThreads), 0.0f);
                    buffersO1.allocateF32(param.name(), initData);
                    buffersO2.allocateF32(param.name(), initData);
                }
                kArgsO1[param.name()] = ptxlab::KernelArg::fromBuffer(param.name());
                kArgsO2[param.name()] = ptxlab::KernelArg::fromBuffer(param.name());
            } else if (param.type().isInt()) {
                kArgsO1[param.name()] = ptxlab::KernelArg::fromI32(totalThreads);
                kArgsO2[param.name()] = ptxlab::KernelArg::fromI32(totalThreads);
            } else if (param.type().isFloat()) {
                kArgsO1[param.name()] = ptxlab::KernelArg::fromF32(1.0f);
                kArgsO2[param.name()] = ptxlab::KernelArg::fromF32(1.0f);
            } else if (param.type().isBool()) {
                kArgsO1[param.name()] = ptxlab::KernelArg::fromBool(true);
                kArgsO2[param.name()] = ptxlab::KernelArg::fromBool(true);
            }
        }

        try {
            ptxlab::ASTInterpreter interpO1(buffersO1);
            interpO1.launch1D(kernel, gridDim, blockDim, kArgsO1);
            std::cout << "kcc: [O1 AST Interpreter] kernel '" << kernel.name()
                      << "' executed successfully.\n";

            ptxlab::ir::IRInterpreter interpO2(buffersO2);
            interpO2.launch1D(irFunc, gridDim, blockDim, kArgsO2);
            std::cout << "kcc: [O2 IR Interpreter]  kernel '" << irFunc.name()
                      << "' executed successfully.\n";

            bool allMatch = true;
            for (const auto& param : kernel.params()) {
                if (param.type().isPtr()) {
                    if (param.type().primitive == ptxlab::PrimitiveType::I32) {
                        const auto& b1 = buffersO1.getI32(param.name());
                        const auto& b2 = buffersO2.getI32(param.name());
                        if (b1 != b2) {
                            allMatch = false;
                            std::cerr << "kcc: error: buffer parity mismatch in '" << param.name() << "'\n";
                        }
                    } else {
                        const auto& b1 = buffersO1.getF32(param.name());
                        const auto& b2 = buffersO2.getF32(param.name());
                        if (b1 != b2) {
                            allMatch = false;
                            std::cerr << "kcc: error: buffer parity mismatch in '" << param.name() << "'\n";
                        }
                    }
                }
            }
            if (!allMatch) {
                return 1;
            }
            std::cout << "[PASS] Differential Testing O1 == O2 verified bit-for-bit parity.\n";

            std::cout << "\n============================================================\n"
                      << " [STAGE 6] Target PTX Code Generation (sm_75 Virtual ISA)\n"
                      << "============================================================\n";
            ptxlab::ptx::PTXEmitter emitter;
            std::string ptxCode = emitter.emit(*irMod);
            std::cout << ptxCode;
            std::cout << "[PASS] PTX Code Generation succeeded without errors.\n";

            std::cout << "\n============================================================\n"
                      << " [STAGE 7] PTX Parser, Validator & Reference Interpreter (Oracle O3)\n"
                      << "============================================================\n";
            std::string parseErr;
            auto ptxMod = ptxlab::ptx::PTXParser::parseString(ptxCode, &parseErr);
            if (!ptxMod) {
                std::cerr << "kcc: error parsing generated PTX:\n" << parseErr << "\n";
                return 1;
            }
            ptxlab::ptx::PTXValidator validator;
            if (!validator.validate(*ptxMod)) {
                std::cerr << "kcc: error validating generated PTX:\n";
                for (const auto& err : validator.errors()) {
                    std::cerr << "  " << err << "\n";
                }
                return 1;
            }
            std::cout << "[PASS] PTX Module parsed and validated successfully (Version " << ptxMod->version()
                      << ", Target " << ptxMod->target() << ", Address Size " << ptxMod->addressSize() << ").\n";

            ptxlab::BufferManager buffersO3;
            std::unordered_map<std::string, ptxlab::KernelArg> kArgsO3;
            for (const auto& param : kernel.params()) {
                if (param.type().isPtr()) {
                    if (param.type().primitive == ptxlab::PrimitiveType::I32) {
                        std::vector<int32_t> initData(static_cast<size_t>(totalThreads), 0);
                        buffersO3.allocateI32(param.name(), initData);
                    } else {
                        std::vector<float> initData(static_cast<size_t>(totalThreads), 0.0f);
                        buffersO3.allocateF32(param.name(), initData);
                    }
                    kArgsO3[param.name()] = ptxlab::KernelArg::fromBuffer(param.name());
                } else if (param.type().isInt()) {
                    kArgsO3[param.name()] = ptxlab::KernelArg::fromI32(totalThreads);
                } else if (param.type().isFloat()) {
                    kArgsO3[param.name()] = ptxlab::KernelArg::fromF32(1.0f);
                } else if (param.type().isBool()) {
                    kArgsO3[param.name()] = ptxlab::KernelArg::fromBool(true);
                }
            }

            ptxlab::ptx::PTXInterpreter interpO3(buffersO3);
            auto resO3 = interpO3.launch(*ptxMod, irFunc.name(), gridDim, blockDim, kArgsO3);
            if (!resO3.success) {
                std::cerr << "kcc: [O3 PTX Interpreter ERROR]: " << resO3.error << "\n";
                return 1;
            }
            std::cout << "kcc: [O3 PTX Interpreter] kernel '" << irFunc.name()
                      << "' executed successfully [Total Cycles: " << resO3.totalCycles << "]\n";

            bool o3Match = true;
            for (const auto& param : kernel.params()) {
                if (param.type().isPtr()) {
                    if (param.type().primitive == ptxlab::PrimitiveType::I32) {
                        if (buffersO1.getI32(param.name()) != buffersO3.getI32(param.name())) {
                            o3Match = false;
                            std::cerr << "kcc: error: buffer parity mismatch between O1 and O3 in '" << param.name() << "'\n";
                        }
                    } else {
                        if (buffersO1.getF32(param.name()) != buffersO3.getF32(param.name())) {
                            o3Match = false;
                            std::cerr << "kcc: error: buffer parity mismatch between O1 and O3 in '" << param.name() << "'\n";
                        }
                    }
                }
            }
            if (!o3Match) return 1;
            std::cout << "[PASS] 3-Way Differential Testing O1 == O2 == O3 verified bit-for-bit parity!\n";

            std::cout << "\n============================================================\n"
                      << " [STAGE 8] Physical GPU Execution & 4-Way Differential Parity\n"
                      << "============================================================\n";
            ptxlab::gpu::GPURunner gpuRunner;
            std::string gpuErr;
            if (gpuRunner.init(&gpuErr)) {
                ptxlab::BufferManager buffersO4;
                std::unordered_map<std::string, ptxlab::KernelArg> kArgsO4;
                for (const auto& param : kernel.params()) {
                    if (param.type().isPtr()) {
                        if (param.type().primitive == ptxlab::PrimitiveType::I32) {
                            std::vector<int32_t> initData(static_cast<size_t>(totalThreads), 0);
                            buffersO4.allocateI32(param.name(), initData);
                        } else {
                            std::vector<float> initData(static_cast<size_t>(totalThreads), 0.0f);
                            buffersO4.allocateF32(param.name(), initData);
                        }
                        kArgsO4[param.name()] = ptxlab::KernelArg::fromBuffer(param.name());
                    } else if (param.type().isInt()) {
                        kArgsO4[param.name()] = ptxlab::KernelArg::fromI32(totalThreads);
                    } else if (param.type().isFloat()) {
                        kArgsO4[param.name()] = ptxlab::KernelArg::fromF32(1.0f);
                    } else if (param.type().isBool()) {
                        kArgsO4[param.name()] = ptxlab::KernelArg::fromBool(true);
                    }
                }
                auto res = gpuRunner.launch(ptxCode, irFunc.name(), gridDim, blockDim, irFunc.params(), buffersO4, kArgsO4);
                if (res.success) {
                    std::cout << "kcc: [O4 GPU Runner] kernel '" << irFunc.name()
                              << "' executed on " << res.device.name << " (sm_"
                              << res.device.computeMajor << res.device.computeMinor << ")"
                              << " [JIT: " << res.jitTimeMs << " ms, Launch: " << res.launchTimeMs << " ms]\n";
                    bool gpuMatch = true;
                    for (const auto& param : kernel.params()) {
                        if (param.type().isPtr()) {
                            if (param.type().primitive == ptxlab::PrimitiveType::I32) {
                                if (buffersO1.getI32(param.name()) != buffersO4.getI32(param.name())) {
                                    gpuMatch = false;
                                    std::cerr << "kcc: error: buffer parity mismatch between CPU and GPU in '" << param.name() << "'\n";
                                }
                            } else {
                                if (buffersO1.getF32(param.name()) != buffersO4.getF32(param.name())) {
                                    gpuMatch = false;
                                    std::cerr << "kcc: error: buffer parity mismatch between CPU and GPU in '" << param.name() << "'\n";
                                }
                            }
                        }
                    }
                    if (!gpuMatch) return 1;
                    std::cout << "[PASS] 4-Way Differential Testing O1 == O2 == O3 == O4 verified bit-for-bit parity on real hardware!\n";
                } else {
                    std::cerr << "kcc: [O4 GPU Runner warning]: " << res.error << "\n";
                }
            } else {
                std::cout << "kcc: [O4 GPU Runner skipped]: " << gpuErr << "\n";
            }

            std::cout << "\n[SUCCESS] Entire compiler pipeline completed successfully!\n";
            return 0;
        } catch (const ptxlab::UBTrap& trap) {
            std::cerr << "kcc: [Interpreter TRAP]: " << trap.message() << "\n";
            if (trap.location().isValid()) {
                std::cerr << "  at " << inputFile << ":" << trap.location().line << ":" << trap.location().column << "\n";
            }
            return 1;
        }
    }

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

    // Lower to TAC IR (M4)
    ptxlab::ir::Lowering lowering;
    auto irModule = lowering.lower(*program);

    ptxlab::ir::IRVerifier verifier;
    if (!verifier.verify(*irModule)) {
        for (const auto& err : verifier.errors()) {
            std::cerr << "kcc: ir verification error: " << err.message << "\n";
        }
        return 1;
    }

    if (dumpIr) {
        if (irModule) {
            std::cout << ptxlab::ir::IRPrinter::dump(*irModule);
        }
        return 0;
    }

    if (runAst) {
        if (!program || program->kernels().empty()) {
            std::cerr << "kcc: error: no kernels found in program to execute\n";
            return 1;
        }

        ptxlab::BufferManager buffers;
        auto& kernel = *program->kernels()[0];

        std::unordered_map<std::string, ptxlab::KernelArg> args;
        int totalThreads = gridDim * blockDim;

        for (const auto& param : kernel.params()) {
            if (param.type().isPtr()) {
                if (param.type().primitive == ptxlab::PrimitiveType::I32) {
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

    if (runIr) {
        if (!irModule || irModule->functions().empty()) {
            std::cerr << "kcc: error: no functions found in IR module to execute\n";
            return 1;
        }

        ptxlab::BufferManager buffers;
        const auto& func = *irModule->functions()[0];

        std::unordered_map<std::string, ptxlab::KernelArg> args;
        int totalThreads = gridDim * blockDim;

        for (const auto& param : func.params()) {
            if (param.type.isPtr()) {
                if (param.type.isInt()) {
                    buffers.allocateI32(param.name, std::vector<int32_t>(static_cast<size_t>(totalThreads), 0));
                } else {
                    buffers.allocateF32(param.name, std::vector<float>(static_cast<size_t>(totalThreads), 0.0f));
                }
                args[param.name] = ptxlab::KernelArg::fromBuffer(param.name);
            } else if (param.type.isInt()) {
                args[param.name] = ptxlab::KernelArg::fromI32(totalThreads);
            } else if (param.type.isFloat()) {
                args[param.name] = ptxlab::KernelArg::fromF32(1.0f);
            } else if (param.type.isPred()) {
                args[param.name] = ptxlab::KernelArg::fromBool(true);
            }
        }

        try {
            ptxlab::ir::IRInterpreter interpreter(buffers);
            interpreter.launch1D(func, gridDim, blockDim, args);
            std::cout << "kcc: [O2 IR Interpreter] kernel '" << func.name()
                      << "' executed successfully (" << gridDim << " blocks x "
                      << blockDim << " threads = " << totalThreads << " total threads)\n";
            return 0;
        } catch (const ptxlab::UBTrap& trap) {
            std::cerr << "kcc: [O2 IR Interpreter TRAP]: " << trap.message() << "\n";
            if (trap.location().isValid()) {
                std::cerr << "  at " << inputFile << ":" << trap.location().line << ":" << trap.location().column << "\n";
            }
            return 1;
        }
    }

    if (emitPtx) {
        ptxlab::ptx::PTXEmitter emitter;
        std::string ptxCode = emitter.emit(*irModule);
        if (!outputFile.empty()) {
            std::ofstream out(outputFile);
            if (!out.is_open()) {
                std::cerr << "kcc: error: cannot open output file '" << outputFile << "' for writing\n";
                return 1;
            }
            out << ptxCode;
        } else {
            std::cout << ptxCode;
        }
        return 0;
    }

    if (runGpu) {
        if (!irModule || irModule->functions().empty()) {
            std::cerr << "kcc: error: no functions found in IR module to execute\n";
            return 1;
        }

        ptxlab::ptx::PTXEmitter emitter;
        std::string ptxCode = emitter.emit(*irModule);

        ptxlab::gpu::GPURunner gpuRunner;
        std::string gpuErr;
        if (!gpuRunner.init(&gpuErr)) {
            std::cerr << "kcc: error: failed to initialize GPU: " << gpuErr << "\n";
            return 1;
        }

        ptxlab::BufferManager buffers;
        const auto& func = *irModule->functions()[0];
        std::unordered_map<std::string, ptxlab::KernelArg> args;
        int totalThreads = gridDim * blockDim;

        for (const auto& param : func.params()) {
            if (param.type.isPtr()) {
                if (param.type.isInt()) {
                    std::vector<int32_t> data(static_cast<size_t>(totalThreads));
                    for (size_t k = 0; k < data.size(); ++k) data[k] = static_cast<int32_t>(k + 1);
                    buffers.allocateI32(param.name, std::move(data));
                } else {
                    std::vector<float> data(static_cast<size_t>(totalThreads));
                    for (size_t k = 0; k < data.size(); ++k) data[k] = static_cast<float>(k + 1);
                    buffers.allocateF32(param.name, std::move(data));
                }
                args[param.name] = ptxlab::KernelArg::fromBuffer(param.name);
            } else if (param.type.isInt()) {
                args[param.name] = ptxlab::KernelArg::fromI32(totalThreads);
            } else if (param.type.isFloat()) {
                args[param.name] = ptxlab::KernelArg::fromF32(2.5f);
            } else if (param.type.isPred()) {
                args[param.name] = ptxlab::KernelArg::fromBool(true);
            }
        }

        auto result = gpuRunner.launch(ptxCode, func.name(), gridDim, blockDim, func.params(), buffers, args);
        if (!result.success) {
            std::cerr << "kcc: [O4 GPU Runner ERROR]: " << result.error << "\n";
            return 1;
        }

        std::cout << "kcc: [O4 GPU Runner] kernel '" << func.name()
                  << "' executed successfully on " << result.device.name
                  << " (sm_" << result.device.computeMajor << result.device.computeMinor << ")"
                  << " (" << gridDim << " blocks x " << blockDim << " threads = "
                  << totalThreads << " total threads)"
                  << " [JIT: " << result.jitTimeMs << " ms, Launch: " << result.launchTimeMs << " ms]\n";

        for (const auto& param : func.params()) {
            if (param.type.isPtr()) {
                std::cout << "  buffer '" << param.name << "' output sample: [";
                if (param.type.isInt()) {
                    const auto& b = buffers.getI32(param.name);
                    size_t count = std::min<size_t>(b.size(), 8);
                    for (size_t k = 0; k < count; ++k) {
                        std::cout << b[k] << (k + 1 < count ? ", " : "");
                    }
                    if (b.size() > 8) std::cout << ", ...";
                } else {
                    const auto& b = buffers.getF32(param.name);
                    size_t count = std::min<size_t>(b.size(), 8);
                    for (size_t k = 0; k < count; ++k) {
                        std::cout << b[k] << (k + 1 < count ? ", " : "");
                    }
                    if (b.size() > 8) std::cout << ", ...";
                }
                std::cout << "]\n";
            }
        }
        return 0;
    }

    if (runPtx) {
        if (!irModule || irModule->functions().empty()) {
            std::cerr << "kcc: error: no functions found in IR module to execute\n";
            return 1;
        }

        ptxlab::ptx::PTXEmitter emitter;
        std::string ptxCode = emitter.emit(*irModule);

        ptxlab::BufferManager buffers;
        const auto& func = *irModule->functions()[0];
        std::unordered_map<std::string, ptxlab::KernelArg> args;
        int totalThreads = gridDim * blockDim;

        for (const auto& param : func.params()) {
            if (param.type.isPtr()) {
                if (param.type.isInt()) {
                    std::vector<int32_t> data(static_cast<size_t>(totalThreads));
                    for (size_t k = 0; k < data.size(); ++k) data[k] = static_cast<int32_t>(k + 1);
                    buffers.allocateI32(param.name, std::move(data));
                } else {
                    std::vector<float> data(static_cast<size_t>(totalThreads));
                    for (size_t k = 0; k < data.size(); ++k) data[k] = static_cast<float>(k + 1);
                    buffers.allocateF32(param.name, std::move(data));
                }
                args[param.name] = ptxlab::KernelArg::fromBuffer(param.name);
            } else if (param.type.isInt()) {
                args[param.name] = ptxlab::KernelArg::fromI32(totalThreads);
            } else if (param.type.isFloat()) {
                args[param.name] = ptxlab::KernelArg::fromF32(2.5f);
            } else if (param.type.isPred()) {
                args[param.name] = ptxlab::KernelArg::fromBool(true);
            }
        }

        ptxlab::ptx::PTXInterpreter interpreter(buffers);
        auto result = interpreter.launch(ptxCode, func.name(), gridDim, blockDim, args);
        if (!result.success) {
            std::cerr << "kcc: [O3 PTX Interpreter ERROR]: " << result.error << "\n";
            return 1;
        }

        std::cout << "kcc: [O3 PTX Interpreter] kernel '" << func.name()
                  << "' executed successfully (" << gridDim << " blocks x " << blockDim << " threads = "
                  << totalThreads << " total threads) [Cycles: " << result.totalCycles << "]\n";

        for (const auto& param : func.params()) {
            if (param.type.isPtr()) {
                std::cout << "  buffer '" << param.name << "' output sample: [";
                if (param.type.isInt()) {
                    const auto& b = buffers.getI32(param.name);
                    size_t count = std::min<size_t>(b.size(), 8);
                    for (size_t k = 0; k < count; ++k) {
                        std::cout << b[k] << (k + 1 < count ? ", " : "");
                    }
                    if (b.size() > 8) std::cout << ", ...";
                } else {
                    const auto& b = buffers.getF32(param.name);
                    size_t count = std::min<size_t>(b.size(), 8);
                    for (size_t k = 0; k < count; ++k) {
                        std::cout << b[k] << (k + 1 < count ? ", " : "");
                    }
                    if (b.size() > 8) std::cout << ", ...";
                }
                std::cout << "]\n";
            }
        }
        return 0;
    }

    std::cout << "kcc: verified and lowered '" << inputFile << "' successfully (use --dump-ir, --run-ir, --emit-ptx, --run-ptx, or --run-gpu)\n";
    return 0;
}

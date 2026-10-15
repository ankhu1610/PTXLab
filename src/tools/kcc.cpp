#include <iostream>
#include <string>
#include <vector>
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

    for (const auto& arg : args) {
        if (arg == "-v" || arg == "--version") {
            std::cout << "PTXLab Compiler (kcc) version " << ptxlab::getVersion() << "\n";
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::cerr << "kcc: error: compilation features will be enabled starting in Milestone 1\n";
    return 1;
}

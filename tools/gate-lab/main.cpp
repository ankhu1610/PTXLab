#include <iostream>
#include <string>
#include <vector>
#include "first_follow.h"
#include "grammar.h"
#include "ll1_table.h"

using namespace ptxlab::gate;

void runTextbookExpressionDemo() {
    std::cout << "\n=======================================================\n"
              << "DEMO 1: Textbook Arithmetic Expression Grammar\n"
              << "=======================================================\n";
    /*
        E  -> T E'
        E' -> + T E' | ε
        T  -> F T'
        T' -> * F T' | ε
        F  -> ( E ) | id
    */
    Grammar g(Symbol::nonTerminal("E"));

    Symbol E  = Symbol::nonTerminal("E");
    Symbol Ep = Symbol::nonTerminal("E'");
    Symbol T  = Symbol::nonTerminal("T");
    Symbol Tp = Symbol::nonTerminal("T'");
    Symbol F  = Symbol::nonTerminal("F");

    Symbol plus   = Symbol::terminal("+");
    Symbol star   = Symbol::terminal("*");
    Symbol lparen = Symbol::terminal("(");
    Symbol rparen = Symbol::terminal(")");
    Symbol id     = Symbol::terminal("id");

    g.addProduction(E, {T, Ep});
    g.addProduction(Ep, {plus, T, Ep});
    g.addProduction(Ep, {Symbol::epsilon()});
    g.addProduction(T, {F, Tp});
    g.addProduction(Tp, {star, F, Tp});
    g.addProduction(Tp, {Symbol::epsilon()});
    g.addProduction(F, {lparen, E, rparen});
    g.addProduction(F, {id});

    auto firstSets = computeFirstSets(g);
    auto followSets = computeFollowSets(g, firstSets);
    auto table = LL1Table::build(g, firstSets, followSets);

    std::cout << "--- FIRST Sets ---\n";
    for (const auto& [sym, set] : firstSets) {
        if (!sym.isTerminal) {
            std::cout << "FIRST(" << sym.name << ") = { ";
            for (const auto& s : set) std::cout << s.name << " ";
            std::cout << "}\n";
        }
    }

    std::cout << "\n--- FOLLOW Sets ---\n";
    for (const auto& [sym, set] : followSets) {
        std::cout << "FOLLOW(" << sym.name << ") = { ";
        for (const auto& s : set) std::cout << s.name << " ";
        std::cout << "}\n";
    }

    std::cout << "\n";
    table.print(std::cout);
}

void runDanglingElseDemo() {
    std::cout << "\n=======================================================\n"
              << "DEMO 2: Dangling-Else Ambiguity & LL(1) Conflict\n"
              << "=======================================================\n";
    /*
        S  -> i E t S S' | a
        S' -> e S | ε
        E  -> b
    */
    Grammar g(Symbol::nonTerminal("S"));

    Symbol S  = Symbol::nonTerminal("S");
    Symbol Sp = Symbol::nonTerminal("S'");
    Symbol E  = Symbol::nonTerminal("E");

    Symbol i = Symbol::terminal("i"); // if
    Symbol t = Symbol::terminal("t"); // then
    Symbol e = Symbol::terminal("e"); // else
    Symbol a = Symbol::terminal("a"); // other stmt
    Symbol b = Symbol::terminal("b"); // condition

    g.addProduction(S, {i, E, t, S, Sp});
    g.addProduction(S, {a});
    g.addProduction(Sp, {e, S});
    g.addProduction(Sp, {Symbol::epsilon()});
    g.addProduction(E, {b});

    auto firstSets = computeFirstSets(g);
    auto followSets = computeFollowSets(g, firstSets);
    auto table = LL1Table::build(g, firstSets, followSets);

    std::cout << "--- FIRST Sets ---\n";
    for (const auto& [sym, set] : firstSets) {
        if (!sym.isTerminal) {
            std::cout << "FIRST(" << sym.name << ") = { ";
            for (const auto& s : set) std::cout << s.name << " ";
            std::cout << "}\n";
        }
    }

    std::cout << "\n--- FOLLOW Sets ---\n";
    for (const auto& [sym, set] : followSets) {
        std::cout << "FOLLOW(" << sym.name << ") = { ";
        for (const auto& s : set) std::cout << s.name << " ";
        std::cout << "}\n";
    }

    std::cout << "\n";
    table.print(std::cout);
}

int main(int argc, char* argv[]) {
    std::cout << "PTXLab GATE Compiler Design Laboratory Tool (gate-lab)\n";

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            std::cout << "\nUsage: gate-lab [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help       Show this help message\n"
                      << "  --demo           Run textbook grammar analysis and conflict detection\n";
            return 0;
        }
    }

    runTextbookExpressionDemo();
    runDanglingElseDemo();
    return 0;
}

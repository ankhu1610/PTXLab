#pragma once

#include <set>
#include <string>
#include <vector>

namespace ptxlab::gate {

struct Symbol {
    std::string name;
    bool isTerminal = false;

    [[nodiscard]] bool isEpsilon() const noexcept {
        return name == "ε" || name == "EPSILON" || name.empty();
    }

    [[nodiscard]] bool isEnd() const noexcept {
        return name == "$";
    }

    [[nodiscard]] bool operator<(const Symbol& other) const noexcept {
        if (isTerminal != other.isTerminal) {
            return isTerminal < other.isTerminal;
        }
        return name < other.name;
    }

    [[nodiscard]] bool operator==(const Symbol& other) const noexcept = default;

    static Symbol terminal(std::string name) {
        return Symbol{std::move(name), true};
    }

    static Symbol nonTerminal(std::string name) {
        return Symbol{std::move(name), false};
    }

    static Symbol epsilon() {
        return Symbol{"ε", true};
    }

    static Symbol endOfInput() {
        return Symbol{"$", true};
    }
};

struct Production {
    Symbol lhs;
    std::vector<Symbol> rhs;

    [[nodiscard]] bool isEpsilonProduction() const noexcept {
        return rhs.empty() || (rhs.size() == 1 && rhs[0].isEpsilon());
    }

    [[nodiscard]] bool operator==(const Production& other) const noexcept = default;
};

class Grammar {
public:
    Grammar() = default;
    explicit Grammar(Symbol startSymbol) : startSymbol_(std::move(startSymbol)) {
        nonTerminals_.insert(startSymbol_);
    }

    void setStartSymbol(Symbol start) {
        startSymbol_ = std::move(start);
        nonTerminals_.insert(startSymbol_);
    }

    const Symbol& getStartSymbol() const noexcept { return startSymbol_; }
    const std::set<Symbol>& getNonTerminals() const noexcept { return nonTerminals_; }
    const std::set<Symbol>& getTerminals() const noexcept { return terminals_; }
    const std::vector<Production>& getProductions() const noexcept { return productions_; }

    void addProduction(Symbol lhs, std::vector<Symbol> rhs) {
        nonTerminals_.insert(lhs);
        for (const auto& sym : rhs) {
            if (sym.isTerminal) {
                if (!sym.isEpsilon() && !sym.isEnd()) {
                    terminals_.insert(sym);
                }
            } else {
                nonTerminals_.insert(sym);
            }
        }
        productions_.push_back(Production{std::move(lhs), std::move(rhs)});
    }

private:
    Symbol startSymbol_;
    std::set<Symbol> nonTerminals_;
    std::set<Symbol> terminals_;
    std::vector<Production> productions_;
};

} // namespace ptxlab::gate

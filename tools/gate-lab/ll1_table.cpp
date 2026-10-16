#include "ll1_table.h"
#include <sstream>

namespace ptxlab::gate {

std::string LL1Conflict::toString() const {
    std::ostringstream oss;
    oss << "LL(1) conflict at M[" << nonTerminal.name << ", " << terminal.name << "]:\n";
    for (const auto& prod : conflictingProductions) {
        oss << "    " << prod.lhs.name << " -> ";
        if (prod.isEpsilonProduction()) {
            oss << "ε";
        } else {
            for (size_t i = 0; i < prod.rhs.size(); ++i) {
                if (i > 0) oss << " ";
                oss << prod.rhs[i].name;
            }
        }
        oss << "\n";
    }
    return oss.str();
}

LL1Table LL1Table::build(
    const Grammar& grammar,
    const FirstSets& firstSets,
    const FollowSets& followSets) {

    LL1Table result;

    for (const auto& prod : grammar.getProductions()) {
        const Symbol& A = prod.lhs;

        std::set<Symbol> firstAlpha = computeFirstOfSequence(prod.rhs, firstSets);

        // 1. For each terminal a in FIRST(alpha), add A -> alpha to M[A, a]
        for (const auto& a : firstAlpha) {
            if (!a.isEpsilon()) {
                result.table_[{A, a}].push_back(prod);
            }
        }

        // 2. If ε in FIRST(alpha), add A -> alpha to M[A, b] for each b in FOLLOW(A)
        if (firstAlpha.find(Symbol::epsilon()) != firstAlpha.end()) {
            auto it = followSets.find(A);
            if (it != followSets.end()) {
                for (const auto& b : it->second) {
                    result.table_[{A, b}].push_back(prod);
                }
            }
        }
    }

    // 3. Detect conflicts
    for (const auto& [key, prods] : result.table_) {
        if (prods.size() > 1) {
            result.conflicts_.push_back(LL1Conflict{key.first, key.second, prods});
        }
    }

    return result;
}

void LL1Table::print(std::ostream& os) const {
    os << "=== LL(1) Parsing Table ===\n";
    for (const auto& [key, prods] : table_) {
        os << "M[" << key.first.name << ", " << key.second.name << "] = ";
        for (size_t i = 0; i < prods.size(); ++i) {
            if (i > 0) os << "  |  ";
            os << prods[i].lhs.name << " -> ";
            if (prods[i].isEpsilonProduction()) {
                os << "ε";
            } else {
                for (size_t j = 0; j < prods[i].rhs.size(); ++j) {
                    if (j > 0) os << " ";
                    os << prods[i].rhs[j].name;
                }
            }
        }
        os << "\n";
    }

    if (!conflicts_.empty()) {
        os << "\n=== Conflicts Detected (" << conflicts_.size() << ") ===\n";
        for (const auto& conflict : conflicts_) {
            os << conflict.toString();
        }
    } else {
        os << "\nGrammar is strictly LL(1) with 0 conflicts.\n";
    }
}

} // namespace ptxlab::gate

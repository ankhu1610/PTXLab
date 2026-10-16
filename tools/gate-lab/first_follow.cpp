#include "first_follow.h"

namespace ptxlab::gate {

FirstSets computeFirstSets(const Grammar& grammar) {
    FirstSets first;

    // 1. Terminals have FIRST(t) = {t}
    for (const auto& term : grammar.getTerminals()) {
        first[term].insert(term);
    }
    first[Symbol::epsilon()].insert(Symbol::epsilon());
    first[Symbol::endOfInput()].insert(Symbol::endOfInput());

    // 2. Initialize non-terminals to empty set
    for (const auto& nt : grammar.getNonTerminals()) {
        first[nt] = {};
    }

    // 3. Fixed-point iteration
    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& prod : grammar.getProductions()) {
            const Symbol& lhs = prod.lhs;
            auto& lhsFirst = first[lhs];
            size_t oldSize = lhsFirst.size();

            if (prod.isEpsilonProduction()) {
                lhsFirst.insert(Symbol::epsilon());
            } else {
                bool allEpsilon = true;
                for (const auto& sym : prod.rhs) {
                    if (sym.isTerminal) {
                        lhsFirst.insert(sym);
                        allEpsilon = false;
                        break;
                    }

                    // sym is non-terminal
                    const auto& symFirst = first[sym];
                    for (const auto& f : symFirst) {
                        if (!f.isEpsilon()) {
                            lhsFirst.insert(f);
                        }
                    }

                    if (symFirst.find(Symbol::epsilon()) == symFirst.end()) {
                        allEpsilon = false;
                        break;
                    }
                }

                if (allEpsilon) {
                    lhsFirst.insert(Symbol::epsilon());
                }
            }

            if (lhsFirst.size() > oldSize) {
                changed = true;
            }
        }
    }

    return first;
}

std::set<Symbol> computeFirstOfSequence(
    const std::vector<Symbol>& sequence,
    const FirstSets& firstSets) {
    
    std::set<Symbol> result;
    if (sequence.empty()) {
        result.insert(Symbol::epsilon());
        return result;
    }

    bool allEpsilon = true;
    for (const auto& sym : sequence) {
        if (sym.isEpsilon()) {
            continue;
        }
        if (sym.isTerminal) {
            result.insert(sym);
            allEpsilon = false;
            break;
        }

        auto it = firstSets.find(sym);
        if (it != firstSets.end()) {
            for (const auto& f : it->second) {
                if (!f.isEpsilon()) {
                    result.insert(f);
                }
            }
            if (it->second.find(Symbol::epsilon()) == it->second.end()) {
                allEpsilon = false;
                break;
            }
        } else {
            allEpsilon = false;
            break;
        }
    }

    if (allEpsilon) {
        result.insert(Symbol::epsilon());
    }
    return result;
}

FollowSets computeFollowSets(
    const Grammar& grammar,
    const FirstSets& firstSets) {

    FollowSets follow;

    for (const auto& nt : grammar.getNonTerminals()) {
        follow[nt] = {};
    }

    // 1. Place $ in FOLLOW(startSymbol)
    follow[grammar.getStartSymbol()].insert(Symbol::endOfInput());

    // 2. Fixed-point iteration
    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& prod : grammar.getProductions()) {
            const Symbol& lhs = prod.lhs;

            for (size_t i = 0; i < prod.rhs.size(); ++i) {
                const Symbol& B = prod.rhs[i];
                if (B.isTerminal || B.isEpsilon()) {
                    continue;
                }

                auto& followB = follow[B];
                size_t oldSize = followB.size();

                // Construct beta = Y_{i+1} ... Y_k
                std::vector<Symbol> beta;
                for (size_t j = i + 1; j < prod.rhs.size(); ++j) {
                    beta.push_back(prod.rhs[j]);
                }

                auto firstBeta = computeFirstOfSequence(beta, firstSets);

                // Add FIRST(beta) \ {ε} to FOLLOW(B)
                for (const auto& f : firstBeta) {
                    if (!f.isEpsilon()) {
                        followB.insert(f);
                    }
                }

                // If ε in FIRST(beta) (or beta is empty), add FOLLOW(lhs) to FOLLOW(B)
                if (firstBeta.find(Symbol::epsilon()) != firstBeta.end()) {
                    const auto& followLhs = follow[lhs];
                    for (const auto& f : followLhs) {
                        followB.insert(f);
                    }
                }

                if (followB.size() > oldSize) {
                    changed = true;
                }
            }
        }
    }

    return follow;
}

} // namespace ptxlab::gate

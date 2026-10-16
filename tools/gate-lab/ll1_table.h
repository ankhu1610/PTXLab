#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>
#include "first_follow.h"
#include "grammar.h"

namespace ptxlab::gate {

struct LL1Conflict {
    Symbol nonTerminal;
    Symbol terminal;
    std::vector<Production> conflictingProductions;

    [[nodiscard]] std::string toString() const;
};

class LL1Table {
public:
    using TableMap = std::map<std::pair<Symbol, Symbol>, std::vector<Production>>;

    [[nodiscard]] static LL1Table build(
        const Grammar& grammar,
        const FirstSets& firstSets,
        const FollowSets& followSets);

    [[nodiscard]] bool isLL1() const noexcept { return conflicts_.empty(); }
    [[nodiscard]] const std::vector<LL1Conflict>& getConflicts() const noexcept { return conflicts_; }
    [[nodiscard]] const TableMap& getTable() const noexcept { return table_; }

    void print(std::ostream& os) const;

private:
    TableMap table_;
    std::vector<LL1Conflict> conflicts_;
};

} // namespace ptxlab::gate

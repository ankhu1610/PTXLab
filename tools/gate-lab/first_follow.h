#pragma once

#include <map>
#include <set>
#include <vector>
#include "grammar.h"

namespace ptxlab::gate {

using FirstSets = std::map<Symbol, std::set<Symbol>>;
using FollowSets = std::map<Symbol, std::set<Symbol>>;

[[nodiscard]] FirstSets computeFirstSets(const Grammar& grammar);

[[nodiscard]] std::set<Symbol> computeFirstOfSequence(
    const std::vector<Symbol>& sequence,
    const FirstSets& firstSets);

[[nodiscard]] FollowSets computeFollowSets(
    const Grammar& grammar,
    const FirstSets& firstSets);

} // namespace ptxlab::gate

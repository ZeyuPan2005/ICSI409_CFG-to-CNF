#include "ConverterTool.h"

#include <algorithm>
#include <queue>

using namespace std;

namespace {
using RHS = Grammar::RHS;
using SymbolSet = set<string>;

// Starting with terminals finds all generating variables. Starting with
// the empty set finds just the nullable ones. An empty RHS always qualifies.
SymbolSet derivableSymbols(const Grammar& grammar, SymbolSet known) {
    // Repeatedly add variables whose RHS symbols are already known. Starting
    // with Sigma finds generating variables, while an empty seed finds nullable
    // variables because an empty RHS satisfies all_of.
    bool changed;
    do {
        changed = false;
        for (const auto& [left, alternatives] : grammar.rule) {
            if (known.count(left)) continue;
            for (const auto& rhs : alternatives) {
                if (all_of(rhs.begin(), rhs.end(), [&](const auto& s) { return known.count(s); })) {
                    known.insert(left);
                    changed = true;
                    break;
                }
            }
        }
    } while (changed);
    return known;
}

string freshVariable(Grammar& grammar, const string& base) {
    string name = base;
    for (unsigned i = 1; grammar.variable.count(name) || grammar.sigma.count(name); ++i)
        name = base + "_" + to_string(i);
    grammar.variable.insert(name);
    return name;
}

bool isUnit(const Grammar& grammar, const RHS& rhs) {
    return rhs.size() == 1 && grammar.variable.count(rhs.front());
}
} // namespace

namespace ConverterTool {
Grammar introduceNewStartVariable(const Grammar& grammar) {
    // A fresh start prevents later epsilon removal from putting the old start
    // on a RHS and preserves the required start-variable restriction.
    for (const auto& entry : grammar.rule) {
        for (const auto& rhs : entry.second) {
            if (find(rhs.begin(), rhs.end(), grammar.startVariable) != rhs.end()) {
                Grammar result = grammar;
                result.startVariable = freshVariable(result, "S0");
                result.rule[result.startVariable].insert(RHS{grammar.startVariable});
                return result;
            }
        }
    }
    return grammar;
}

bool generatesEpsilon(const Grammar& grammar) {
    return derivableSymbols(grammar, {}).count(grammar.startVariable) != 0;
}

Grammar removeEpsilonProductions(const Grammar& grammar) {
    // Nullable variables can be omitted independently at every RHS position.
    // The set of prefixes removes duplicate alternatives from repeated symbols.
    const auto nullable = derivableSymbols(grammar, {});
    Grammar result = grammar;
    result.rule.clear();
    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            // Extend each prefix by keeping the symbol, or by omitting it if
            // nullable. Sets merge equal outcomes for repeated occurrences.
            set<RHS> prefixes{RHS{}};
            for (const auto& symbol : rhs) {
                set<RHS> next;
                if (nullable.count(symbol)) next = prefixes;
                for (auto prefix : prefixes) {
                    prefix.push_back(symbol);
                    next.insert(prefix);
                }
                prefixes.swap(next);
            }
            prefixes.erase(RHS{});
            result.rule[left].insert(prefixes.begin(), prefixes.end());
        }
    }
    if (nullable.count(grammar.startVariable))
        result.rule[grammar.startVariable].insert(RHS{});
    return result;
}

Grammar removeUnitProductions(const Grammar& grammar) {
    // Traverse the unit-rule graph from each source and copy every reachable
    // non-unit rule. The visited set makes cycles terminate.
    Grammar result = grammar;
    result.rule.clear();
    for (const auto& source : grammar.variable) {
        // Visit each variable once per source, even when unit rules form a cycle.
        SymbolSet visited{source};
        queue<string> pending;
        pending.push(source);
        while (!pending.empty()) {
            const auto current = pending.front();
            pending.pop();
            const auto it = grammar.rule.find(current);
            if (it == grammar.rule.end()) continue;
            for (const auto& rhs : it->second) {
                if (!isUnit(grammar, rhs)) result.rule[source].insert(rhs);
                else if (visited.insert(rhs.front()).second) pending.push(rhs.front());
            }
        }
    }
    return result;
}

Grammar removeUselessVariables(const Grammar& grammar) {
    // First keep only generating rules, then reach them from the start. This
    // combines the standard generating and reachable-variable passes.
    const auto generating = derivableSymbols(grammar, grammar.sigma);
    Grammar result;
    result.sigma = grammar.sigma;
    result.startVariable = grammar.startVariable;
    result.variable.insert(grammar.startVariable);
    // Search only through generating productions. If the start cannot
    // generate a terminal string, it remains as the start with no rules.
    queue<string> pending;
    pending.push(grammar.startVariable);
    while (!pending.empty()) {
        const auto left = pending.front();
        pending.pop();
        const auto it = grammar.rule.find(left);
        if (it == grammar.rule.end()) continue;
        for (const auto& rhs : it->second) {
            if (!all_of(rhs.begin(), rhs.end(), [&](const auto& s) { return generating.count(s); }))
                continue;
            result.rule[left].insert(rhs);
            for (const auto& symbol : rhs)
                if (grammar.variable.count(symbol) && result.variable.insert(symbol).second)
                    pending.push(symbol);
        }
    }
    return result;
}

Grammar replaceTerminals(const Grammar& grammar) {
    // CNF permits terminals only in singleton RHSs. Reuse one proxy variable
    // for each terminal so all long productions share the same replacement.
    Grammar result = grammar;
    result.rule.clear();
    map<string, string> proxy;
    for (const auto& [left, alternatives] : grammar.rule) {
        for (auto rhs : alternatives) {
            if (rhs.size() >= 2) {
                for (auto& symbol : rhs) {
                    if (!grammar.sigma.count(symbol)) continue;
                    if (!proxy.count(symbol)) {
                        const auto name = freshVariable(result, symbol == "#" ? "T_hash" : "T_" + symbol);
                        proxy[symbol] = name;
                        result.rule[name].insert(RHS{symbol});
                    }
                    symbol = proxy.at(symbol);
                }
            }
            result.rule[left].insert(rhs);
        }
    }
    return result;
}

Grammar binarize(const Grammar& grammar) {
    // A long RHS is replaced by a chain of binary rules. Each fresh variable
    // names the unprocessed suffix, preserving the original derivation order.
    Grammar result = grammar;
    result.rule.clear();
    unsigned nextId = 1;
    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            auto head = left;
            size_t i = 0;
            // Each new variable derives exactly the unprocessed suffix.
            while (rhs.size() - i > 2) {
                const auto tail = freshVariable(result, "X" + to_string(nextId++));
                result.rule[head].insert(RHS{rhs[i], tail});
                head = tail;
                ++i;
            }
            result.rule[head].insert(RHS(rhs.begin() + i, rhs.end()));
        }
    }
    return result;
}

string cnfError(const Grammar& grammar, bool originalGeneratesEpsilon) {
    // Validate the actual final representation rather than trusting the stage
    // sequence. The original nullability is the reference for start -> eps.
    const auto structuralError = grammar.validationError();
    if (!structuralError.empty()) return structuralError;
    bool startHasEpsilon = false;
    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            if (find(rhs.begin(), rhs.end(), grammar.startVariable) != rhs.end())
                return "The start variable occurs on a RHS in " + left + ".";
            if (rhs.empty() && left == grammar.startVariable) startHasEpsilon = true;
            else if (rhs.size() == 1 && grammar.sigma.count(rhs[0])) continue;
            else if (rhs.size() == 2 && grammar.variable.count(rhs[0]) && grammar.variable.count(rhs[1])) continue;
            else return "Invalid CNF production for " + left + ".";
        }
    }
    if (startHasEpsilon != originalGeneratesEpsilon)
        return "The start epsilon rule disagrees with nullability of the original grammar.";
    return {};
}
} // namespace ConverterTool

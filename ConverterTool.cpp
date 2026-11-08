#include "ConverterTool.h"

#include <algorithm>
#include <queue>

using namespace std;

namespace {
using RHS = Grammar::RHS;
using SymbolSet = set<string>;

/*
Function: derivableSymbols
Description:
    Expand a set of known symbols until no new variable can be added.
    A variable qualifies when one of its RHSs contains only known symbols.
    Starting with {} finds nullable variables; starting with Sigma finds
    variables that can generate terminal strings (including eps).
Parameters:
    grammar: Grammar to examine.
    known: Initial set of known symbols.
Return:
    The expanded set of known symbols.
*/
SymbolSet derivableSymbols(const Grammar& grammar, SymbolSet known) {
    bool changed;
    do {
        changed = false;
        for (const auto& [left, alternatives] : grammar.rule) {
            if (known.count(left)) continue;

            for (const auto& rhs : alternatives) {
                // An empty RHS also qualifies; it is the base case for eps.
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

/*
Function: freshVariable
Description:
    Create and register an unused variable name. If the requested base
    already exists, append a numeric suffix to avoid name collisions.
Parameters:
    grammar: Grammar that will receive the new variable.
    base: Preferred variable name.
Return:
    The chosen variable name.
*/
string freshVariable(Grammar& grammar, const string& base) {
    string name = base;
    for (unsigned i = 1; grammar.variable.count(name) || grammar.sigma.count(name); ++i)
        name = base + "_" + to_string(i);

    grammar.variable.insert(name);
    return name;
}

// A unit production has exactly one variable on its RHS (A -> B).
bool isUnit(const Grammar& grammar, const RHS& rhs) {
    return rhs.size() == 1 && grammar.variable.count(rhs.front());
}
} // namespace

namespace ConverterTool {

/*
Function: introduceNewStartVariable
Description:
    If the start variable occurs on any RHS, add a fresh start with
    a rule pointing to the old start. This keeps the new start off
    all RHSs, as required by the CNF form used in this project.
Return:
    The updated grammar, or the original if no change is needed.
*/
Grammar introduceNewStartVariable(const Grammar& grammar) {
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

// Check whether the start variable can derive eps, directly or indirectly.
bool generatesEpsilon(const Grammar& grammar) {
    return derivableSymbols(grammar, {}).count(grammar.startVariable) != 0;
}

/*
Function: removeEpsilonProductions
Description:
    Find nullable variables, then generate RHS variants by keeping
    or omitting each nullable occurrence. Remove empty alternatives
    and restore eps only for a nullable start variable.
    A set removes duplicate variants.
Return:
    Grammar without non-start epsilon productions.
*/
Grammar removeEpsilonProductions(const Grammar& grammar) {
    const auto nullable = derivableSymbols(grammar, {});
    Grammar result = grammar;
    result.rule.clear();

    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            set<RHS> prefixes{RHS{}};

            for (const auto& symbol : rhs) {
                set<RHS> next;

                // A nullable occurrence may be omitted; the loop below also keeps it.
                if (nullable.count(symbol)) next = prefixes;

                for (auto prefix : prefixes) {
                    prefix.push_back(symbol);
                    next.insert(prefix);
                }

                prefixes.swap(next);
            }

            // Discard empty variants here; restore eps for the start below if needed.
            prefixes.erase(RHS{});
            result.rule[left].insert(prefixes.begin(), prefixes.end());
        }
    }

    if (nullable.count(grammar.startVariable))
        result.rule[grammar.startVariable].insert(RHS{});

    return result;
}

/*
Function: removeUnitProductions
Description:
    Treat each unit rule A -> B as an edge. Starting from each
    variable, follow unit-rule edges and copy the reachable non-unit
    rules back to that variable. Track visited variables to handle cycles.
Return:
    Grammar without unit productions.
*/
Grammar removeUnitProductions(const Grammar& grammar) {
    Grammar result = grammar;
    result.rule.clear();

    for (const auto& source : grammar.variable) {
        SymbolSet visited{source};
        queue<string> pending;
        pending.push(source);

        while (!pending.empty()) {
            const auto current = pending.front();
            pending.pop();

            const auto it = grammar.rule.find(current);
            if (it == grammar.rule.end()) continue;

            for (const auto& rhs : it->second) {
                if (!isUnit(grammar, rhs)) {
                    result.rule[source].insert(rhs);
                }
                else if (visited.insert(rhs.front()).second) {
                    pending.push(rhs.front());
                }
            }
        }
    }

    return result;
}

/*
Function: removeUselessVariables
Description:
    Find variables that can generate terminal strings, then traverse
    from the start using only generating rules. Remove non-generating
    and unreachable variables and their rules.
    Keep the start variable even if the language is empty.
Return:
    Grammar containing only the retained reachable rules.
*/
Grammar removeUselessVariables(const Grammar& grammar) {
    const auto generating = derivableSymbols(grammar, grammar.sigma);

    Grammar result;
    result.sigma = grammar.sigma;
    result.startVariable = grammar.startVariable;
    // Keep the start even when no generating rule survives.
    result.variable.insert(grammar.startVariable);

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

            for (const auto& symbol : rhs) {
                if (grammar.variable.count(symbol) && result.variable.insert(symbol).second)
                    pending.push(symbol);
            }
        }
    }

    return result;
}

/*
Function: replaceTerminals
Description:
    Replace terminals appearing in RHSs of length two or more
    with helper variables. Each helper derives one terminal, and
    the same helper is reused for repeated occurrences.
Return:
    Grammar with terminals isolated in single-terminal rules.
*/
Grammar replaceTerminals(const Grammar& grammar) {
    Grammar result = grammar;
    result.rule.clear();
    map<string, string> proxy;

    for (const auto& [left, alternatives] : grammar.rule) {
        for (auto rhs : alternatives) {
            if (rhs.size() >= 2) {
                for (auto& symbol : rhs) {
                    if (!grammar.sigma.count(symbol)) continue;

                    if (!proxy.count(symbol)) {
                        const auto name = freshVariable(result, symbol == "#" ? "Thash" : "T" + symbol);
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

/*
Function: binarize
Description:
    Split a RHS longer than two elements into binary rules.
    Each new helper variable represents the unprocessed suffix,
    preserving the order of the original RHS.
    Example: A -> B C D becomes A -> B X1, X1 -> C D.
Return:
    Grammar with RHS lengths no greater than two.
*/
Grammar binarize(const Grammar& grammar) {
    Grammar result = grammar;
    result.rule.clear();
    unsigned nextId = 1;

    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            auto head = left;
            size_t i = 0;

            // Each helper derives the suffix not yet emitted.
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

/*
Function: cnfError
Description:
    Check grammar consistency and the allowed CNF forms:
    A -> B C, A -> a, and start -> eps when applicable.
    The start variable must not occur on any RHS.
    Compare the final start-eps rule with the original grammar's
    nullability; this is a structural check, not an equivalence proof.
Parameters:
    grammar: Final grammar to check.
    originalGeneratesEpsilon: Whether the input grammar derives eps.
Return:
    Empty string when valid; otherwise an error message.
*/
string cnfError(const Grammar& grammar, bool originalGeneratesEpsilon) {
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

    // Epsilon membership must match what the original start could derive.
    if (startHasEpsilon != originalGeneratesEpsilon)
        return "The start epsilon rule disagrees with nullability of the original grammar.";

    return {};
}

} // namespace ConverterTool

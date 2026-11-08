#include "ConverterTool.h"

#include <algorithm>
#include <queue>

using namespace std;

namespace {
using RHS = Grammar::RHS;
using SymbolSet = set<string>;

// derivableSymbols: input a grammar and a known set, and it returns all
// variables that can be derived from that starting knowledge.
//
// Simple idea:
// - If known starts as the terminal set Sigma, the function finds all variables
//   that can eventually generate terminal strings.
// - If known starts empty, the function finds all variables that can generate
//   epsilon (empty string).
//
// We keep adding a variable to the set whenever one of its productions is made
// entirely of symbols that are already known. When no new variable can be
// added, the search stops.
SymbolSet derivableSymbols(const Grammar& grammar, SymbolSet known) {
    // This is a fixed-point computation. We keep expanding the set until no new
    // variable can be added.
    bool changed;
    do {
        changed = false;
        for (const auto& [left, alternatives] : grammar.rule) {
            // Don't revisit a variable already in the known set.
            if (known.count(left)) continue;

            // A variable is derivable if there exists at least one production whose
            // every symbol is already derivable. This matches the standard
            // iterative construction used in CFG analysis.
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

// freshVariable creates a new variable name that is guaranteed not to collide
// with any existing variable or terminal symbol.
//
// Example:
//   base = "X" -> names "X", "X_1", "X_2", ...
// This is necessary because after transformations we may need to invent helper
// variables for terminals or binary decomposition.
string freshVariable(Grammar& grammar, const string& base) {
    string name = base;
    for (unsigned i = 1; grammar.variable.count(name) || grammar.sigma.count(name); ++i)
        name = base + "_" + to_string(i);

    grammar.variable.insert(name);
    return name;
}

// A production is a unit production if its RHS has exactly one symbol and that
// symbol is a variable. Example: A -> B.
bool isUnit(const Grammar& grammar, const RHS& rhs) {
    return rhs.size() == 1 && grammar.variable.count(rhs.front());
}
} // namespace

namespace ConverterTool {

// introduceNewStartVariable
// If the old start symbol appears anywhere on the RHS of some rule, we create a
// fresh start variable S0 and add a rule S0 -> oldStart.
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

// generatesEpsilon checks whether the start variable can derive epsilon.
bool generatesEpsilon(const Grammar& grammar) {
    return derivableSymbols(grammar, {}).count(grammar.startVariable) != 0;
}

// removeEpsilonProductions
// -----------------------
// This eliminates epsilon productions by expanding every RHS according to which
// symbols are nullable.
//
// For example, if B is nullable in A -> B C, the generated alternatives
// include A -> C and A -> B C. The empty RHS is removed here and restored only
// for the start variable when the input grammar can derive epsilon.
//
// The algorithm works by building all possible prefix combinations for each RHS:
// for each symbol in the RHS, either keep it or omit it if the symbol is
// nullable. We collect them in a set to avoid duplicates.
Grammar removeEpsilonProductions(const Grammar& grammar) {
    const auto nullable = derivableSymbols(grammar, {});
    Grammar result = grammar;
    result.rule.clear();

    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            // prefixes stores the set of all possible shortened RHSs produced so
            // far while processing the current production.
            set<RHS> prefixes{RHS{}};

            for (const auto& symbol : rhs) {
                set<RHS> next;

                // If the current symbol may be omitted, then all previous
                // prefixes remain valid without it.
                if (nullable.count(symbol)) next = prefixes;

                // Otherwise, we keep the symbol appended to each current prefix.
                for (auto prefix : prefixes) {
                    prefix.push_back(symbol);
                    next.insert(prefix);
                }

                prefixes.swap(next);
            }

            // Remove the empty RHS from the generated set unless the left side is
            // the start symbol and epsilon must be preserved.
            prefixes.erase(RHS{});
            result.rule[left].insert(prefixes.begin(), prefixes.end());
        }
    }

    // If the start symbol is nullable, the grammar may still generate epsilon.
    // The conversion preserves this fact explicitly.
    if (nullable.count(grammar.startVariable))
        result.rule[grammar.startVariable].insert(RHS{});

    return result;
}

// removeUnitProductions
// Basic idea:
// A unit production is of the form A -> B, where both A and B are variables.
// The algorithm treats the unit relation as a graph. We perform a breadth-first
// search (BFS) from each variable to find all reachable variables in that graph.
// Every non-unit production found during the traversal is copied to the source.
Grammar removeUnitProductions(const Grammar& grammar) {
    Grammar result = grammar;
    result.rule.clear();

    for (const auto& source : grammar.variable) {
        // visited prevents infinite traversal when unit rules form cycles like
        // A -> B, B -> A.
        SymbolSet visited{source};
        queue<string> pending;
        pending.push(source);

        while (!pending.empty()) {
            const auto current = pending.front();
            pending.pop();

            const auto it = grammar.rule.find(current);
            if (it == grammar.rule.end()) continue;

            for (const auto& rhs : it->second) {
                // If this is a non-unit rule, it can be inherited by the source.
                if (!isUnit(grammar, rhs)) {
                    result.rule[source].insert(rhs);
                }
                // If it is a unit rule, continue BFS traversal through that edge.
                else if (visited.insert(rhs.front()).second) {
                    pending.push(rhs.front());
                }
            }
        }
    }

    return result;
}

// removeUselessVariables
// ---------------------
// A variable is useless if it is either:
//   1. non-generating: it cannot derive any terminal string, or
//   2. unreachable: it cannot be reached from the start symbol.
//
// The algorithm uses the standard two-pass approach:
// - first compute all generating symbols using terminals as the seed
// - then BFS from the start through only generating rules
//
// Any symbol not in the reachable generating subgrammar is discarded.
Grammar removeUselessVariables(const Grammar& grammar) {
    // generating = all variables that can derive terminal strings.
    // We seed with Sigma because terminals are the base case.
    const auto generating = derivableSymbols(grammar, grammar.sigma);

    Grammar result;
    result.sigma = grammar.sigma;
    result.startVariable = grammar.startVariable;
    result.variable.insert(grammar.startVariable);

    // BFS through the grammar graph from the start symbol.
    queue<string> pending;
    pending.push(grammar.startVariable);

    while (!pending.empty()) {
        const auto left = pending.front();
        pending.pop();

        const auto it = grammar.rule.find(left);
        if (it == grammar.rule.end()) continue;

        for (const auto& rhs : it->second) {
            // Ignore productions that are not entirely within the generating set.
            if (!all_of(rhs.begin(), rhs.end(), [&](const auto& s) { return generating.count(s); }))
                continue;

            result.rule[left].insert(rhs);

            // If a symbol on the RHS is a variable, and it is new, keep traversing
            // through it.
            for (const auto& symbol : rhs) {
                if (grammar.variable.count(symbol) && result.variable.insert(symbol).second)
                    pending.push(symbol);
            }
        }
    }

    return result;
}

// replaceTerminals
// ----------------
// CNF restricts terminals to appear only in rules of the form A -> a, where a
// is a terminal symbol.
//
// If a production contains terminals inside a longer RHS, we replace each terminal
// with a fresh proxy variable and add a new rule for that proxy.
//
// Example:
//   A -> a B C
// becomes:
//   A -> X B C
//   X -> a
//
// We reuse the same proxy variable for the same terminal to avoid unnecessary
// duplication.
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

// binarize
// --------
// CNF requires each rule to have at most two symbols on the RHS.
//
// For a long production like A -> B C D E, we introduce fresh helper variables:
//   A -> B X1
//   X1 -> C X2
//   X2 -> D E
//
// This preserves the original order of symbols while converting the grammar into
// binary form.
Grammar binarize(const Grammar& grammar) {
    Grammar result = grammar;
    result.rule.clear();
    unsigned nextId = 1;

    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            auto head = left;
            size_t i = 0;

            // While the remaining suffix still has more than two symbols, create
            // a new variable representing the unresolved suffix.
            while (rhs.size() - i > 2) {
                const auto tail = freshVariable(result, "X" + to_string(nextId++));
                result.rule[head].insert(RHS{rhs[i], tail});
                head = tail;
                ++i;
            }

            // The final remaining chunk, of size 1 or 2, is emitted directly.
            result.rule[head].insert(RHS(rhs.begin() + i, rhs.end()));
        }
    }

    return result;
}

// cnfError
// --------
// This is the final structural validator for the converted grammar.
//
// It checks two things:
// 1. The grammar itself is structurally valid.
// 2. The start variable's epsilon status remains consistent with the original
//    nullable status.
string cnfError(const Grammar& grammar, bool originalGeneratesEpsilon) {
    const auto structuralError = grammar.validationError();
    if (!structuralError.empty()) return structuralError;

    bool startHasEpsilon = false;
    for (const auto& [left, alternatives] : grammar.rule) {
        for (const auto& rhs : alternatives) {
            // CNF forbids the start variable from appearing on a RHS.
            if (find(rhs.begin(), rhs.end(), grammar.startVariable) != rhs.end())
                return "The start variable occurs on a RHS in " + left + ".";

            // Special case for start -> epsilon.
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

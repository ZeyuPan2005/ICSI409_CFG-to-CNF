#include "Grammar.h"

#include <algorithm>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {
bool isUpper(char c) {
    return c >= 'A' && c <= 'Z';
}

bool isLowerOrDigit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool isVariable(const string& token) {
    return !token.empty() && isUpper(token.front()) &&
           all_of(token.begin() + 1, token.end(), [](char c) {
               return isUpper(c) || isLowerOrDigit(c) || c == '_';
           });
}

bool isTerminal(const string& token) {
    return token.size() == 1 && (isLowerOrDigit(token.front()) || token == "#");
}

void printSet(ostream& out, const set<string>& values) {
    out << '{';
    bool first = true;
    for (const auto& value : values) {
        if (!first) out << ", ";
        out << value;
        first = false;
    }
    out << '}';
}
} // namespace

Grammar Grammar::parse(istream& input) {
    Grammar grammar;
    // Keep the first reference line so an undefined variable can be reported
    // after all LHS definitions have been read, allowing forward references.
    map<string, size_t> firstReference;
    string line;
    size_t lineNumber = 0;
    while (getline(input, line)) {
        ++lineNumber;
        istringstream tokens(line);
        string left, arrow;
        if (!(tokens >> left)) continue;
        const auto prefix = "Line " + to_string(lineNumber) + ": ";
        if (!isVariable(left))
            throw runtime_error(prefix + "invalid left-hand variable '" + left + "'.");
        if (!(tokens >> arrow) || arrow != "->")
            throw runtime_error(prefix + "expected a separate '->' after " + left + ".");
        if (grammar.startVariable.empty()) grammar.startVariable = left;
        grammar.variable.insert(left);

        // Read one alternative at a time. The stream extraction preserves
        // token boundaries instead of splitting a token into characters.
        do {
            RHS rhs;
            bool epsilon = false;
            string symbol;
            while (tokens >> symbol && symbol != "|") {
                if (symbol == u8"\u03b5")
                    throw runtime_error(prefix + "Greek epsilon is unsupported; use 'eps'.");
                const bool isEpsilon = symbol == "eps";
                if (epsilon || (isEpsilon && !rhs.empty()))
                    throw runtime_error(prefix + "epsilon must be alone in its alternative.");
                if (isEpsilon) {
                    epsilon = true;
                    continue;
                }
                if (isVariable(symbol)) firstReference.emplace(symbol, lineNumber);
                else if (isTerminal(symbol)) grammar.sigma.insert(symbol);
                else throw runtime_error(prefix + "invalid RHS token '" + symbol + "'.");
                rhs.push_back(symbol);
            }
            if (rhs.empty() && !epsilon)
                throw runtime_error(prefix + "empty alternative; use 'eps' for epsilon.");
            grammar.rule[left].insert(rhs);
        } while (tokens); // A consumed bar requires one more alternative.
    }
    if (input.bad() || !input.eof()) throw runtime_error("Failed to read input.");
    if (grammar.startVariable.empty()) throw runtime_error("Input contains no productions.");
    for (const auto& [name, firstLine] : firstReference)
        if (!grammar.variable.count(name))
            throw runtime_error("Line " + to_string(firstLine) +
                                     ": undefined RHS variable '" + name + "'.");
    return grammar;
}

string Grammar::validationError() const {
    // Parsing checks the external syntax. This pass checks the mathematical
    // sets and rules after construction, including disjoint V and Sigma.
    if (!variable.count(startVariable)) return "The start variable must belong to V.";
    if (variable.count("") || sigma.count("")) return "A symbol name is empty.";
    for (const auto& v : variable)
        if (sigma.count(v)) return "Symbol belongs to both V and Sigma: " + v;
    for (const auto& [left, alternatives] : rule) {
        if (!variable.count(left)) return "Unknown left-hand variable: " + left;
        for (const auto& rhs : alternatives)
            for (const auto& symbol : rhs)
                if (!variable.count(symbol) && !sigma.count(symbol))
                    return "Unknown right-hand symbol: " + symbol;
    }
    return {};
}

void Grammar::print(ostream& output) const {
    output << "V = ";
    printSet(output, variable);
    output << "\nSigma = ";
    printSet(output, sigma);
    output << "\nStart = " << startVariable << "\nRules:\n";
    bool any = false;
    for (const auto& [left, alternatives] : rule) {
        if (alternatives.empty()) continue;
        any = true;
        output << "  " << left << " -> ";
        bool first = true;
        for (const auto& rhs : alternatives) {
            if (!first) output << " | ";
            if (rhs.empty()) output << "eps";
            for (size_t i = 0; i < rhs.size(); ++i) {
                if (i) output << ' ';
                output << rhs[i];
            }
            first = false;
        }
        output << '\n';
    }
    if (!any) output << "  (none)\n";
}

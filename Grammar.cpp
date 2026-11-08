#include "Grammar.h"

#include <algorithm>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {
// A variable name must begin with an uppercase letter.
// This helper checks whether a single character is uppercase.
bool isUpper(char c) {
    return c >= 'A' && c <= 'Z';
}

// Check for a lowercase letter or digit.
// Uppercase letters and underscores in variable names are handled by isVariable().
bool isLowerOrDigit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

// Returns true if token is a valid variable name.
// A variable must start with an uppercase letter and then
// continue with uppercase/lowercase letters, digits, or underscores.
bool isVariable(const string& token) {
    return !token.empty() && isUpper(token.front()) &&
           all_of(token.begin() + 1, token.end(), [](char c) {
               return isUpper(c) || isLowerOrDigit(c) || c == '_';
           });
}

// Returns true if token is a valid terminal symbol.
// In this grammar, terminals are single-character lowercase/digit symbols
// or the special '#' symbol.
bool isTerminal(const string& token) {
    return token.size() == 1 && (isLowerOrDigit(token.front()) || token == "#");
}

// Prints a set in the form {x, y, z}.
// The output is used for displaying V and Sigma in the grammar printout.
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

// Parse a grammar from a text stream.
// The function reads line by line, validates each production, and constructs a Grammar object.
Grammar Grammar::parse(istream& input) {
    Grammar grammar;

    // Record the first line where each referenced variable appears on the right-hand side.
    // This allows forward references such as: S -> A; A -> a
    // We do not reject them immediately because the variable may be declared later.
    map<string, size_t> firstReference;

    // The current line being read from the input stream.
    string line;
    size_t lineNumber = 0;

    // Process the grammar one line at a time.
    while (getline(input, line)) {
        ++lineNumber;

        // Treat each line as a token stream separated by spaces.
        istringstream tokens(line);
        string left, arrow;

        // Ignore empty lines.
        if (!(tokens >> left)) continue;

        // Prefix to include line numbers in error messages.
        const auto prefix = "Line " + to_string(lineNumber) + ": ";

        // Left-hand side must be a valid variable name.
        if (!isVariable(left))
            throw runtime_error(prefix + "invalid left-hand variable '" + left + "'.");

        // The production must contain the arrow "->" between the LHS and RHS.
        if (!(tokens >> arrow) || arrow != "->")
            throw runtime_error(prefix + "expected a separate '->' after " + left + ".");

        // The first production determines the grammar's start variable.
        if (grammar.startVariable.empty()) grammar.startVariable = left;

        // Add the left-hand variable to the variable set V.
        grammar.variable.insert(left);

        // A production can have multiple alternatives separated by '|'.
        // Example: A -> a B | c | eps
        do {
            // rhs stores one alternative on the right-hand side.
            // Example: ["a", "B"]
            RHS rhs;

            // epsilon is true if this alternative is explicitly written as 'eps'.
            bool epsilon = false;
            string symbol;

            // Parse symbols until the end of the current alternative or a pipe symbol.
            while (tokens >> symbol && symbol != "|") {
                // This project forbids the Unicode epsilon character 'ε'.
                // Use the ASCII word "eps" instead.
                if (symbol == u8"\u03b5")
                    throw runtime_error(prefix + "Greek epsilon is unsupported; use 'eps'.");

                const bool isEpsilon = symbol == "eps";

                // 'eps' must be the whole alternative by itself.
                // For example: A -> eps is valid, but A -> a eps is not.
                if (epsilon || (isEpsilon && !rhs.empty()))
                    throw runtime_error(prefix + "epsilon must be alone in its alternative.");

                // If we see 'eps', mark this alternative as epsilon and continue.
                if (isEpsilon) {
                    epsilon = true;
                    continue;
                }

                // If the symbol is a variable, remember it for later validation.
                // If it is a terminal, add it to Sigma.
                if (isVariable(symbol)) firstReference.emplace(symbol, lineNumber);
                else if (isTerminal(symbol)) grammar.sigma.insert(symbol);
                else throw runtime_error(prefix + "invalid RHS token '" + symbol + "'.");

                // Keep the symbol in the current right-hand side sequence.
                rhs.push_back(symbol);
            }

            // An alternative cannot be empty unless it is epsilon.
            if (rhs.empty() && !epsilon)
                throw runtime_error(prefix + "empty alternative; use 'eps' for epsilon.");

            // Store this alternative in the production table.
            // Example: rule["A"] = { ["a", "B"], ["c"], [] }
            grammar.rule[left].insert(rhs);
        } while (tokens); // If a '|' was consumed, continue with the next alternative.
    }

    // After parsing, the stream should be in a clean terminal state.
    if (input.bad() || !input.eof()) throw runtime_error("Failed to read input.");

    // Every grammar must have at least one production.
    if (grammar.startVariable.empty()) throw runtime_error("Input contains no productions.");

    // Final validation: every variable used on the RHS must be defined on the LHS somewhere.
    for (const auto& [name, firstLine] : firstReference)
        if (!grammar.variable.count(name))
            throw runtime_error("Line " + to_string(firstLine) +
                                     ": undefined RHS variable '" + name + "'.");

    return grammar;
}

// Validate the semantic consistency of the parsed grammar.
// This pass checks the structure after parsing, not the raw input syntax.
// It ensures that the grammar follows the mathematical definition of a context-free grammar:
//   - the start symbol must be in V
//   - V and Sigma must be disjoint
//   - every production LHS must be a variable
//   - every symbol on the RHS must belong to V or Sigma
string Grammar::validationError() const {
    // Check that the start variable is actually a defined variable.
    if (!variable.count(startVariable)) return "The start variable must belong to V.";

    // Reject empty names in both symbol sets.
    if (variable.count("") || sigma.count("")) return "A symbol name is empty.";

    // A symbol cannot belong to both the variable set and the terminal set.
    for (const auto& v : variable)
        if (sigma.count(v)) return "Symbol belongs to both V and Sigma: " + v;

    // Every left-hand side must be a valid variable, and every symbol in every right-hand side
    // must belong to either V or Sigma.
    for (const auto& [left, alternatives] : rule) {
        if (!variable.count(left)) return "Unknown left-hand variable: " + left;
        for (const auto& rhs : alternatives)
            for (const auto& symbol : rhs)
                if (!variable.count(symbol) && !sigma.count(symbol))
                    return "Unknown right-hand symbol: " + symbol;
    }

    // No semantic issues found.
    return {};
}

// Print the grammar for intermediate and final stage reports.
// The start rule appears first, followed by sorted rules and alternatives.
// The format is:
//   V = {...}
//   Sigma = {...}
//   Start = S
//   Rules:
//     A -> a B | c
//     B -> eps
void Grammar::print(ostream& output) const {
    // Print the set of variables.
    output << "V = ";
    printSet(output, variable);

    // Print the set of terminals.
    output << "\nSigma = ";
    printSet(output, sigma);

    // Print the start symbol and the rule header.
    output << "\nStart = " << startVariable << "\nRules:\n";

    // Print the start rule first; keep map order for the remaining rules.
    vector<string> displayOrder;
    if (rule.count(startVariable)) displayOrder.push_back(startVariable);
    for (const auto& entry : rule)
        if (entry.first != startVariable) displayOrder.push_back(entry.first);

    bool any = false;
    for (const auto& left : displayOrder) {
        const auto& alternatives = rule.at(left);
        if (alternatives.empty()) continue;
        any = true;

        output << "  " << left << " -> ";
        bool first = true;

        // Print all alternatives for this left-hand side.
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

    // If no productions exist, print a placeholder message.
    if (!any) output << "  (none)\n";
}

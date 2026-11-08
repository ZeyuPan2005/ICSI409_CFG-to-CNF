#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

struct Grammar {
    // This is G = (V, Sigma, R, S). Each RHS keeps token order so a
    // multi-character variable remains one grammar element.
    using RHS = std::vector<std::string>;

    std::set<std::string> variable;
    std::set<std::string> sigma;
    std::map<std::string, std::set<RHS>> rule;
    std::string startVariable;

    // Parse whitespace-separated input. Epsilon is stored as an empty RHS.
    static Grammar parse(std::istream& input);
    // An empty message means the grammar is well formed.
    std::string validationError() const;
    void print(std::ostream& output) const;
};

#endif

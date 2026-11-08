#ifndef CONVERTER_H
#define CONVERTER_H

#include "Grammar.h"

// Converter is the stateful orchestrator for one test case.
// It keeps the current working grammar and applies a fixed sequence of
// CNF-normalization steps in order, while keeping the original grammar's
// nullable information for final validation.
class Converter {
public:
    // One Converter owns the mutable grammar for one Driver test case.
    explicit Converter(const Grammar& original);

    // Return the working grammar after the last transformation step.
    const Grammar& currentGrammar() const;

    // Each method applies one CNF transformation and updates current.
    void introduceNewStartVariable();
    void removeEpsilonProductions();
    void removeUnitProductions();
    void removeUselessVariables();
    void replaceTerminals();
    void binarize();

    // Validate the final grammar against CNF requirements.
    std::string cnfError() const;

private:
    // Each stage replaces this value with the result returned by ConverterTool.
    Grammar current;

    // Cached before the conversion pipeline starts, because epsilon removal can
    // change the grammar structure and affect the final validity check.
    bool originalGeneratesEpsilon;
};

#endif

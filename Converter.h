#ifndef CONVERTER_H
#define CONVERTER_H

#include "Grammar.h"

class Converter {
public:
    // One Converter owns the mutable grammar for one Driver test case.
    explicit Converter(const Grammar& original);
    const Grammar& currentGrammar() const;
    void introduceNewStartVariable();
    void removeEpsilonProductions();
    void removeUnitProductions();
    void removeUselessVariables();
    void replaceTerminals();
    void binarize();
    std::string cnfError() const;

private:
    // Each stage replaces this value with the result returned by ConverterTool.
    Grammar current;
    bool originalGeneratesEpsilon;
};

#endif

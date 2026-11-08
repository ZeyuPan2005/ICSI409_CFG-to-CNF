#include "Converter.h"
#include "ConverterTool.h"

using namespace std;

// Capture nullability before epsilon removal changes the working grammar.
Converter::Converter(const Grammar& original)
    : current(original), originalGeneratesEpsilon(ConverterTool::generatesEpsilon(original)) {}

const Grammar& Converter::currentGrammar() const {
    return current;
}

void Converter::introduceNewStartVariable() {
    current = ConverterTool::introduceNewStartVariable(current);
}

void Converter::removeEpsilonProductions() {
    current = ConverterTool::removeEpsilonProductions(current);
}

void Converter::removeUnitProductions() {
    current = ConverterTool::removeUnitProductions(current);
}

void Converter::removeUselessVariables() {
    current = ConverterTool::removeUselessVariables(current);
}

void Converter::replaceTerminals() {
    current = ConverterTool::replaceTerminals(current);
}

void Converter::binarize() {
    current = ConverterTool::binarize(current);
}

string Converter::cnfError() const {
    return ConverterTool::cnfError(current, originalGeneratesEpsilon);
}

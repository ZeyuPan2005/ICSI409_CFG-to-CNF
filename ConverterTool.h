#ifndef CONVERTER_TOOL_H
#define CONVERTER_TOOL_H

#include "Grammar.h"

namespace ConverterTool {
// Each function is stateless. It returns a new Grammar so Converter controls
// the order in which the mathematical transformations are applied.
Grammar introduceNewStartVariable(const Grammar& grammar);
Grammar removeEpsilonProductions(const Grammar& grammar);
Grammar removeUnitProductions(const Grammar& grammar);
Grammar removeUselessVariables(const Grammar& grammar);
Grammar replaceTerminals(const Grammar& grammar);
Grammar binarize(const Grammar& grammar);
bool generatesEpsilon(const Grammar& grammar);
std::string cnfError(const Grammar& grammar, bool originalGeneratesEpsilon);
}

#endif

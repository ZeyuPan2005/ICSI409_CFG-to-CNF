#ifndef CONVERTER_TOOL_H
#define CONVERTER_TOOL_H

#include "Grammar.h"

// ConverterTool provides the individual grammar transformations as pure
// functions. The goal is to keep each step deterministic and easy to test:
// input -> output, without hidden shared state.
//
// The Converter class owns the execution order and the current working grammar.
// This separation keeps the mathematical transformation logic independent from
// the driver's control flow.
namespace ConverterTool {
    // Each function is stateless. It returns a new Grammar so Converter controls
    // the order in which the mathematical transformations are applied.
    Grammar introduceNewStartVariable(const Grammar& grammar);
    Grammar removeEpsilonProductions(const Grammar& grammar);
    Grammar removeUnitProductions(const Grammar& grammar);
    Grammar removeUselessVariables(const Grammar& grammar);
    Grammar replaceTerminals(const Grammar& grammar);
    Grammar binarize(const Grammar& grammar);

    // Helper functions used by the conversion pipeline and final validation.
    bool generatesEpsilon(const Grammar& grammar);
    std::string cnfError(const Grammar& grammar, bool originalGeneratesEpsilon);
}

#endif

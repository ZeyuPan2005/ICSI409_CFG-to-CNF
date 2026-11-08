#include "Converter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace fs = filesystem;

namespace {
// Print one transformation stage to the output file.
void printStage(ostream& out, const string& heading, const Grammar& grammar) {
    out << heading << "\n\n";
    grammar.print(out);
    out << '\n';
}
}

int main() {
    // Process each sample grammar and save the full transformation trace.
    const fs::path inputDir = "tests";
    const fs::path outputDir = "outputs";
    // Each case corresponds to a grammar file named <case>.txt.
    const vector<string> cases = {"test1", "test2", "test3", "test4", "test5", "test6"};
    size_t passed = 0;
    for (const auto& name : cases) {
        try {
            // Parse and validate the grammar before opening its report. Each
            // Converter instance then works on its own copy.
            const auto inputPath = inputDir / (name + ".txt");
            ifstream input(inputPath);
            if (!input) throw runtime_error("Cannot open " + inputPath.string());
            const Grammar original = Grammar::parse(input);
            const auto inputError = original.validationError();
            if (!inputError.empty()) throw runtime_error(inputError);
            fs::create_directories(outputDir);
            const auto outputPath = outputDir / (name + ".txt");
            ofstream output(outputPath);
            if (!output) throw runtime_error("Cannot open " + outputPath.string());
            output.exceptions(ios::failbit | ios::badbit);
            printStage(output, "1. Original Grammar", original);
            Converter converter(original);

            // Record every simplification step, even when the grammar remains unchanged.
            converter.introduceNewStartVariable();
            printStage(output, "2. New Start Variable", converter.currentGrammar());
            converter.removeEpsilonProductions();
            printStage(output, "3. Remove Epsilon Productions", converter.currentGrammar());
            converter.removeUnitProductions();
            printStage(output, "4. Remove Unit Productions", converter.currentGrammar());
            converter.removeUselessVariables();
            printStage(output, "5. Remove Useless Variables", converter.currentGrammar());
            converter.replaceTerminals();
            printStage(output, "6. Replace Terminals", converter.currentGrammar());
            converter.binarize();
            printStage(output, "7. Binarization", converter.currentGrammar());
            printStage(output, "8. Final CNF Validation", converter.currentGrammar());

            // Validate the final CNF grammar and report the result.
            const auto error = converter.cnfError();
            const auto verdict = error.empty() ? "PASS" : "FAIL";
            output << "CNF Validation: " << verdict << '\n';
            if (!error.empty()) output << "Reason: " << error << '\n';
            output.close();
            cout << name << " - " << verdict << '\n';
            if (!error.empty()) cerr << "Reason: " << error << '\n';
            if (error.empty()) ++passed;
        } catch (const exception& error) {
            cerr << name << " - FAIL: " << error.what() << '\n';
        }
    }
    // If any sample fails to parse, open, or validate, the overall run is reported as failed.
    cout << "Passed " << passed << " of " << cases.size() << " tests.\n";
    return passed == cases.size() ? 0 : 1;
}

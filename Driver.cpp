#include "Converter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace fs = filesystem;

namespace {
void printStage(ostream& out, const string& heading, const Grammar& grammar) {
    out << heading << "\n\n";
    grammar.print(out);
    out << '\n';
}
}

int main() {
    // Read each lecture input and write a report with the same filename.
    const fs::path inputDir = "tests";
    const fs::path outputDir = "outputs";
    const vector<string> cases = {"test1", "test2", "test3", "test4", "test5", "test6"};
    size_t passed = 0;
    for (const auto& name : cases) {
        try {
            // Parsing and validation happen before output is opened. A new
            // Converter keeps this test's working copy independent of others.
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

            // Record every simplification stage, including stages with no change.
            converter.introduceNewStartVariable();
            printStage(output, "2. New Start Variable", converter.currentGrammar());
            converter.removeEpsilonProductions();
            printStage(output, "3. Remove Epsilon Productions", converter.currentGrammar());
            converter.removeUnitProductions();
            printStage(output, "4. Remove Unit Productions", converter.currentGrammar());
            converter.removeUselessVariables();
            printStage(output, "5. Remove Useless Variables", converter.currentGrammar());

            // CNF conversion first isolates terminals, then splits long RHSs.
            converter.replaceTerminals();
            printStage(output, "6. Replace Terminals", converter.currentGrammar());
            converter.binarize();
            printStage(output, "7. Binarization", converter.currentGrammar());

            // Validate the final structure and its start-variable nullability.
            printStage(output, "8. Final CNF Validation", converter.currentGrammar());
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
    // A parse, file, or validation failure makes the overall run fail.
    cout << "Passed " << passed << " of " << cases.size() << " tests.\n";
    return passed == cases.size() ? 0 : 1;
}

#include "Converter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

namespace fs = filesystem;

namespace {
// Print one transformation stage to the output file.
void printStage(ostream& out, const string& heading, const Grammar& grammar) {
    out << heading << "\n\n";
    grammar.print(out);
    out << '\n';
}

// Compare numeric parts of filenames as numbers (test2 before test10).
bool naturalLess(const fs::path& a, const fs::path& b) {
    const string left = a.filename().string();
    const string right = b.filename().string();
    size_t i = 0, j = 0;

    while (i < left.size() && j < right.size()) {
        const bool leftDigit = isdigit(static_cast<unsigned char>(left[i]));
        const bool rightDigit = isdigit(static_cast<unsigned char>(right[j]));
        if (leftDigit && rightDigit) {
            size_t endI = i, endJ = j;
            while (endI < left.size() && isdigit(static_cast<unsigned char>(left[endI]))) ++endI;
            while (endJ < right.size() && isdigit(static_cast<unsigned char>(right[endJ]))) ++endJ;

            size_t significantI = i, significantJ = j;
            while (significantI < endI && left[significantI] == '0') ++significantI;
            while (significantJ < endJ && right[significantJ] == '0') ++significantJ;
            const size_t lengthI = endI - significantI;
            const size_t lengthJ = endJ - significantJ;
            if (lengthI != lengthJ) return lengthI < lengthJ;
            const int comparison = left.compare(significantI, lengthI, right, significantJ, lengthJ);
            if (comparison != 0) return comparison < 0;
            i = endI;
            j = endJ;
        } else {
            if (left[i] != right[j]) return left[i] < right[j];
            ++i;
            ++j;
        }
    }
    if (i != left.size() || j != right.size()) return i == left.size();
    return left < right; // Deterministic tie-break for names such as test1 and test01.
}
}

int main() {
    const fs::path inputDir = "tests";
    const fs::path outputDir = "outputs";
    vector<fs::path> testFiles;

    try {
        if (!fs::is_directory(inputDir))
            throw runtime_error("Missing tests/ directory.");

        // Discover test files without requiring edits to Driver.cpp.
        for (const auto& entry : fs::directory_iterator(inputDir))
            if (entry.is_regular_file() && entry.path().extension() == ".txt")
                testFiles.push_back(entry.path());

        if (testFiles.empty()) throw runtime_error("No .txt tests found in tests/.");
        sort(testFiles.begin(), testFiles.end(), naturalLess);

        fs::create_directories(outputDir);
        // Old text reports must not be mistaken for results from this run.
        for (const auto& entry : fs::directory_iterator(outputDir))
            if (entry.is_regular_file() && entry.path().extension() == ".txt")
                fs::remove(entry.path());
    } catch (const exception& error) {
        cerr << "Setup - FAIL: " << error.what() << '\n';
        return 1;
    }

    size_t passed = 0;
    for (const auto& inputPath : testFiles) {
        const string name = inputPath.stem().string();
        const auto outputPath = outputDir / inputPath.filename();
        try {
            ofstream output(outputPath);
            if (!output) throw runtime_error("Cannot open " + outputPath.string());
            output.exceptions(ios::failbit | ios::badbit);

            string error;
            try {
                ifstream input(inputPath);
                if (!input) throw runtime_error("Cannot open " + inputPath.string());
                const Grammar original = Grammar::parse(input);
                const auto inputError = original.validationError();
                if (!inputError.empty()) throw runtime_error(inputError);

                printStage(output, "1. Original Grammar", original);
                Converter converter(original);
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
                error = converter.cnfError();
            } catch (const exception& failure) {
                error = failure.what();
            }

            output << "CNF Validation: " << (error.empty() ? "PASS" : "FAIL") << '\n';
            if (!error.empty()) output << "Reason: " << error << '\n';
            output.close();

            if (error.empty()) {
                cout << name << " - PASS\n";
                ++passed;
            } else {
                cout << name << " - FAIL: " << error << '\n';
            }
        } catch (const exception& error) {
            cout << name << " - FAIL: Cannot write report: " << error.what() << '\n';
        }
    }

    cout << "Passed " << passed << " of " << testFiles.size() << " tests.\n";
    return passed == testFiles.size() ? 0 : 1;
}

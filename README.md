# CFG-to-CNF Converter

## Overview

This C++17 project converts context-free grammars into Chomsky normal form (CNF). It automatically discovers .txt grammars in tests/, records every conversion stage, and writes one report per test case.

## Features

- Represents a grammar as G = (V, Sigma, R, S).
- Parses whitespace-separated grammar tokens, including multi-character variables.
- Introduces a fresh start variable when the original start appears on a right-hand side.
- Removes epsilon productions, unit productions, and useless variables.
- Replaces terminals in long productions and binarizes longer right-hand sides.
- Validates the final CNF structure and the start variable's epsilon rule.

## Project Structure

- Grammar.h and Grammar.cpp represent G = (V, Sigma, R, S), parse input, validate symbols, and print grammars.
- Converter.h and Converter.cpp hold one working Grammar for each test case and invoke the stages in order.
- ConverterTool.h and ConverterTool.cpp contain the stateless transformation algorithms and final CNF validation.
- Driver.cpp is the only entry point. It discovers .txt files in tests/, processes them in natural filename order, creates a separate Converter for each input, and reports success or failure.
- tests/ contains six supplied grammars (test1.txt through test6.txt); you may add more .txt test files.
- outputs/ contains generated .txt reports with filenames matching the corresponding inputs.

## Requirements

A C++17-compatible compiler and the standard library are required. The project is tested with g++ 11.4 on Ubuntu 22.04.

## Build and Run

From the project root in WSL/Ubuntu or another Bash environment, compile directly to the root executable:

    g++ -std=c++17 -Wall -Wextra -pedantic \
        Driver.cpp Grammar.cpp Converter.cpp ConverterTool.cpp \
        -o cfg_to_cnf
    ./cfg_to_cnf

The program automatically processes all regular .txt files in tests/ (case-sensitive extension) in natural filename order: test2.txt comes before test10.txt. To add a test, place its .txt file in tests/; no source change is required. An absent or empty tests/ directory is reported as an error. The program returns a nonzero status if any test fails or if setup fails.

**Important: before processing tests, the program deletes all existing .txt files directly inside outputs/.** It then writes fresh reports with the same filenames as the inputs. Do not store manually edited .txt files in outputs/, because they will be deleted on the next run. Other file types and subdirectories are not cleared. When no tests are found or tests/ is missing, the program reports an error without clearing outputs/.

## Input Format

Each nonblank line specifies one left-hand variable and one or more right-hand-side alternatives separated by |. The same left-hand variable may appear on multiple lines, and blank lines are ignored. The arrow -> and alternative separator | must be separate tokens. One or more spaces or tabs may appear between grammar elements.

    S -> a S b | eps
    X1 -> b

The first production's left-hand side is the start variable. A variable matches [A-Z][A-Za-z0-9_]*, so names may contain multiple characters. Xa is one variable, while X a is two grammar elements. Concatenated strings such as aSb are invalid.

The permitted terminal-token set is {a, ..., z} union {0, ..., 9} union {#}. Each terminal is one lowercase English letter, one digit, or the literal #. This permitted set is distinct from Sigma, which contains only the terminals used by a particular input grammar.

Only the standalone ASCII token eps represents epsilon. It is stored internally as an empty RHS and printed as eps. Greek epsilon is rejected. Epsilon must be the only token in its alternative. Undefined variables, malformed arrows, empty alternatives, and unsupported symbols are rejected with line-numbered errors.

## Output Format

For a successfully parsed grammar, every report contains these numbered sections:

1. Original Grammar
2. New Start Variable
3. Remove Epsilon Productions
4. Remove Unit Productions
5. Remove Useless Variables
6. Replace Terminals
7. Binarization
8. Final CNF Validation

Each section prints V, Sigma, Start, and Rules. The start variable's rules appear first; other left-hand sides and alternatives use deterministic sorted order. Therefore, the Original Grammar section preserves the grammar but not necessarily the input order. Rules use two-space indentation, one space between RHS tokens, and eps for an empty RHS. The final section prints the final grammar and either CNF Validation: PASS or CNF Validation: FAIL with a reason.

If parsing or validation fails, the corresponding output file instead records CNF Validation: FAIL and a Reason; a failure later in conversion may leave a partial stage trace before that result. Console output also reports the per-file result and total passed count.

The converter names terminal helpers by the terminal they represent: Ta -> a, Tb -> b, T0 -> 0, or Thash -> #. The same terminal shares one helper variable within a conversion. Long productions use X1, X2, ... for binary decomposition; identical suffixes in different productions may receive separate X variables. Generated names avoid collisions with existing variables by adding a numeric suffix when needed (for example, Ta_1).

## Test Cases

The six supplied grammars exercise different parts of the pipeline. Additional .txt files in tests/ are run automatically.

| File | Focus |
| --- | --- |
| test1.txt | Nullable recursive start and matched terminal pairs |
| test2.txt | Nullable suffix variable in a recursive production |
| test3.txt | Recursive concatenation and balanced wrapping |
| test4.txt | Recursive wrapping with a nullable helper variable |
| test5.txt | Literal #, digit terminals, and long productions |
| test6.txt | A non-S start variable, repeated nullable occurrences, and a unit rule |

The output files show the original grammar, all intermediate transformations, and the final validation result.

## Limitations

The input grammar is restricted to the token formats described above and one production per line. Nullable expansion may produce exponentially many alternatives. The validator checks CNF structure, start-variable restrictions, and epsilon membership. It does not prove language equivalence for every possible word.


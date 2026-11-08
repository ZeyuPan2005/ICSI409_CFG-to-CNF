# CFG-to-CNF Converter

## Overview

This C++17 project converts a context-free grammar into Chomsky normal form (CNF). It reads six grammar files, records every conversion stage, and writes one deterministic report per test case.

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
- Driver.cpp is the only entry point. It opens each input, creates a separate Converter, writes all eight stages, and prints the summary.
- tests/test1.txt through tests/test6.txt contain the six primary grammars.
- outputs/test1.txt through outputs/test6.txt contain the generated reports.

## Requirements

A C++17-compatible compiler and the standard library are required. The project is tested with g++ 11.4 on Ubuntu 22.04.

## Build and Run

From the project root, compile directly to the root executable:

    g++ -std=c++17 -Wall -Wextra -pedantic \
        Driver.cpp Grammar.cpp Converter.cpp ConverterTool.cpp \
        -o cfg_to_cnf
    ./cfg_to_cnf

The program reads the six files in tests/ and writes the six corresponding files in outputs/. It returns a nonzero status if a test cannot be parsed, opened, or validated.

## Input Format

Each production occupies one line. The arrow and alternative separator are separate tokens. One or more spaces or tabs may appear between grammar elements.

    S -> a S b | eps
    X1 -> b

The first production's left-hand side is the start variable. A variable matches [A-Z][A-Za-z0-9_]*, so names may contain multiple characters. Xa is one variable, while X a is two grammar elements. Concatenated strings such as aSb are invalid.

The permitted terminal-token set is {a, ..., z} union {0, ..., 9} union {#}. Each terminal is one lowercase English letter, one digit, or the literal #. This permitted set is distinct from Sigma, which contains only the terminals used by a particular input grammar.

Only the standalone ASCII token eps represents epsilon. It is stored internally as an empty RHS and printed as eps. Greek epsilon is rejected. Epsilon must be the only token in its alternative. Undefined variables, malformed arrows, empty alternatives, and unsupported symbols are rejected with line-numbered errors.

## Output Format

Every report contains these numbered sections:

1. Original Grammar
2. New Start Variable
3. Remove Epsilon Productions
4. Remove Unit Productions
5. Remove Useless Variables
6. Replace Terminals
7. Binarization
8. Final CNF Validation

Each section prints V, Sigma, Start, and Rules. Rules use two-space indentation, one space between RHS tokens, and eps for an empty RHS. The final section prints the final grammar and either CNF Validation: PASS or CNF Validation: FAIL with a reason.

## Test Cases

The six primary grammars exercise different parts of the pipeline.

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


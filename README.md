# CFG-to-CNF Converter

## Overview

This C++17 course project converts context-free grammars to Chomsky normal form. It prints the grammar after each conversion stage and writes one report for each of the six primary inputs.

## Features

- Introduces a fresh start variable when required.
- Removes epsilon productions while preserving start-variable epsilon.
- Removes unit productions, including chains and cycles.
- Removes non-generating and unreachable variables.
- Replaces terminals in long productions.
- Binarizes productions longer than two symbols.
- Validates the final CNF structure.

## Requirements

- C++17-compatible compiler
- Standard C++ library
- Ubuntu, WSL, or another environment with a compatible g++

## Build and Run

From the project root:

    g++ -std=c++17 -Wall -Wextra -pedantic \
        Driver.cpp Grammar.cpp Converter.cpp ConverterTool.cpp \
        -o cfg_to_cnf
    ./cfg_to_cnf

The executable reads tests/test1.txt through tests/test6.txt. It writes outputs/test1.txt through outputs/test6.txt and prints a summary to standard output.

## Input Format

Each production occupies one line. Grammar elements are whitespace-separated, and multiple spaces or tabs are allowed.

    S -> a S b | eps
    X1 -> b

The first production's left-hand side is the start variable. Variable names match [A-Z][A-Za-z0-9_]*. A terminal is one lowercase letter, one digit, or the literal #. The token Xa is one variable. X a is two elements. Concatenated text such as aSb is invalid.

Only the standalone ASCII token eps represents epsilon. It is stored internally as an empty right-hand side and printed as eps. Greek epsilon is rejected. Epsilon must be the only token in its alternative. Undefined variables and malformed arrows or alternatives are rejected with line-numbered errors.

## Output

Each output file contains these numbered sections:

1. Original Grammar
2. New Start Variable
3. Remove Epsilon Productions
4. Remove Unit Productions
5. Remove Useless Variables
6. Replace Terminals
7. Binarization
8. Final CNF Validation

Every section displays V, Sigma, Start, and Rules. The final section displays the final grammar and reports CNF Validation: PASS or a failure reason.

## Project Structure

- Grammar.h, Grammar.cpp store, parse, validate, and print a grammar.
- Converter.h, Converter.cpp manage one conversion state per test case.
- ConverterTool.h, ConverterTool.cpp implement the transformation functions and CNF validation.
- Driver.cpp controls file I/O, stage order, reports, and the test summary.
- tests/ contains the six input grammars.
- outputs/ contains their six generated reports.

## Limitations

The input alphabet is limited to single-character lowercase letters, digits, and #. Nullable expansion can produce exponentially many alternatives. Structural CNF validation does not constitute a formal proof of language equivalence for every possible word.


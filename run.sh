#!/bin/sh
# Compile and run the whole Tagpy front end with ONE command:
#   lexical analyzer (front.c)  ->  "Symbol Table.txt"
#   syntax analyzer  (syntax_analyzer.c)  ->  "Parsing Table.txt"
#
# Usage:  ./run.sh [file.tagpy] [trace]
#   file.tagpy  the Tagpy program to check (default: sample.tagpy)
#   trace       also show the full Enter/Exit parse trace on the screen
#
# Run it from the folder that contains front.c and syntax_analyzer.c.

SRC="${1:-sample.tagpy}"
MODE="-q"
[ "$2" = "trace" ] && MODE=""

echo "[1/4] Compiling the lexical analyzer (front.c)..."
gcc front.c -o front || { echo "Build failed."; exit 1; }

echo "[2/4] Compiling the syntax analyzer (syntax_analyzer.c)..."
gcc syntax_analyzer.c -o syntax_analyzer || { echo "Build failed."; exit 1; }

echo "[3/4] Lexical analysis of $SRC ..."
./front "$SRC" || { echo "Lexical analysis failed."; exit 1; }

echo "[4/4] Syntax analysis ..."
./syntax_analyzer $MODE
RC=$?

echo
echo "Output files:"
echo "  Symbol Table.txt   (tokens from the lexical analyzer)"
echo "  Parsing Table.txt  (parsing table from the syntax analyzer)"
exit $RC

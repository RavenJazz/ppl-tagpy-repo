# Tagpy Lexical Analyzer and Syntax Analyzer

COSC 303 – Principles of Programming Languages.

This project has two phases for **Tagpy**:

1. **Lexical analyzer** (`front.c`) – reads a Tagpy source file and writes every
   token to `Symbol Table.txt`.
2. **Syntax analyzer** (`syntax_analyzer.c`) – reads `Symbol Table.txt` one token
   at a time and checks it against the Tagpy EBNF grammar with a
   **recursive-descent parser** (based on Sebesta, _Concepts of Programming
   Languages_).

```text
sample.tagpy --> [ front.c ] --> Symbol Table.txt --> [ syntax_analyzer.c ] --> Parsing Table.txt
 (source)        lexical          (one token per        syntax analysis          (parsing table
                 analyzer          row)                                           + syntax errors)
```

**Run everything with one command:**

```bash
run.bat sample.tagpy             # Windows
sh run.sh sample.tagpy           # Linux / macOS
```

## Files

| File                              | Purpose                                                                               |
| --------------------------------- | ------------------------------------------------------------------------------------- |
| `front.c`                         | Lexical analyzer                                                                      |
| `syntax_analyzer.c`               | Syntax analyzer (recursive-descent parser)                                            |
| `run.bat` / `run.sh`              | One command: compiles both programs, runs the lexer, then the parser                  |
| `syntax_analyzer_explanation.txt` | Explains the syntax analyzer code, with the transition diagrams and the parsing table |
| `syntax_test.tagpy`               | A correct program that uses every statement – the parser accepts it                   |
| `syntax_error_test.tagpy`         | A program with 11 deliberate syntax errors                                            |

## Input file type

Tagpy source files use the **`.tagpy`** extension (the lexical analyzer rejects
other extensions).

## Compile

Each phase is a single source file, so no build script is needed:

```bash
gcc front.c -o front                              # Linux / macOS
gcc syntax_analyzer.c -o syntax_analyzer
gcc front.c -o front.exe                          # Windows (MinGW)
gcc syntax_analyzer.c -o syntax_analyzer.exe
```

## Run

### One command (recommended)

```bash
run.bat syntax_test.tagpy         # Windows
sh run.sh syntax_test.tagpy       # Linux / macOS  (or: chmod +x run.sh; ./run.sh ...)
```

The script compiles `front.c` and `syntax_analyzer.c`, runs the lexical
analyzer on the file, then runs the syntax analyzer. Run it from the folder
that contains the `.c` files.

- With no file name it uses `sample.tagpy`.
- Add `trace` after the file name (`run.bat syntax_test.tagpy trace`) to also
  show the full Enter/Exit parse trace on the screen.
- It stops if a program does not compile or the lexer rejects the file.
- Output files: `Symbol Table.txt` and `Parsing Table.txt`.

### Step by step

```bash
./front syntax_test.tagpy         # writes "Symbol Table.txt"
./syntax_analyzer                 # reads "Symbol Table.txt", writes "Parsing Table.txt"
```

(Windows: `front.exe syntax_test.tagpy` then `syntax_analyzer.exe`.)

---

# Part 1 – Lexical analyzer (`front.c`)

A hand-written lexical analyzer. Tokenization is done with a finite automaton
(`keyword_lookup` is a DFA) plus one-character lookahead for operators; no
premade lexer and no string-comparison / word-matching functions are used.

```bash
./front sample.tagpy
```

With no argument it reads `sample.tagpy`. The output is written to
**`Symbol Table.txt`** in the current folder:

```text
TOKEN TYPE              LEXEME
----------              ------
LINE_COMMENT            # global state
RW_NUM                  num
IDENTIFIER              a
ASSIGN                  =
INT_LITERAL             10
NEWLINE                 \n
```

Invalid input is reported on the screen with its line number **and** written
to the table as `INVALID`; the analyzer keeps scanning afterwards.

## What is recognized

| Category                        | Token types                                                                                                                                                                              |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Identifier                      | `IDENTIFIER` (letters, digits, `_`, Ñ/ñ; case-insensitive keywords)                                                                                                                      |
| Keywords (statements)           | `KW_KUNG`, `KW_KUNDIKUNG`, `KW_KUNDI`, `KW_BAWAT`, `KW_SA`, `KW_HABANG`, `KW_ITUKOY`, `KW_IBALIK`, `KW_SURI`, `KW_SAKALI`                                                                |
| Reserved words (data types)     | `RW_NUM`, `RW_TUL`, `RW_BAKA`, `RW_KAR`, `RW_PISI`, `RW_LISTA`                                                                                                                           |
| Reserved words (input / output) | `RW_SULAT`, `RW_SULATLN`, `RW_KUNIN`                                                                                                                                                     |
| Constants                       | `INT_LITERAL`, `FLOAT_LITERAL`, `STRING_LITERAL` (`"…"` or `'…'`), `TRUE_LITERAL` (`totoo`), `FALSE_LITERAL` (`palso`)                                                                   |
| Noise words                     | `NOISE_WORD` (`ang`, `ay`)                                                                                                                                                               |
| Comments                        | `LINE_COMMENT` (`# …`), `BLOCK_COMMENT` (`## … ##`)                                                                                                                                      |
| Arithmetic operators (7)        | `ADD +`, `SUBTRACT -`, `MULTIPLY *`, `DIVIDE /`, `EXPONENT **`, `MODULO %`, `FLOOR_DIVIDE //`                                                                                            |
| Boolean operators (9)           | `EQUAL ==`, `NOT_EQUAL !=`, `LESS_THAN <`, `GREATER_THAN >`, `LESS_EQUAL <=`, `GREATER_EQUAL >=`, `AND at`, `OR o`, `NOT hindi`                                                          |
| Assignment                      | `ASSIGN =`                                                                                                                                                                               |
| Delimiters / brackets           | `LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE COLON COMMA SEMICOLON DOT`                                                                                                                |
| Layout (off-side rule)          | `NEWLINE`, `INDENT`, `DEDENT` (**keep `SHOW_LAYOUT_IN_TABLE` at 1 in `front.c`** – the syntax analyzer needs these rows)                                                                 |
| Invalid                         | `INVALID` – unknown characters, lone `!`, unterminated strings / block comments, numbers glued to letters (`3x`), unsupported non-ASCII, unmatched or unclosed brackets, bad indentation |

> **Change for the syntax analyzer:** the keyword `habang` (the `while` loop in
> the grammar) was added to the lexer's DFA as `KW_HABANG`. Before this it was
> read as an `IDENTIFIER`, so a `habang` loop could not be parsed.

## Lexer test files

| File                 | Purpose                                                               |
| -------------------- | --------------------------------------------------------------------- |
| `sample.tagpy`       | One example of every token class                                      |
| `keyword_test.tagpy` | Keywords vs. identifiers (`kung`, `kungx`, `Kung`, `num1`, `ng`, Ñ/ñ) |
| `invalid_test.tagpy` | Invalid-token handling                                                |
| `proposal.tagpy`     | Every code snippet from the Group 5 proposal                          |

---

# Part 2 – Syntax analyzer (`syntax_analyzer.c`)

```bash
./syntax_analyzer                 # parse "Symbol Table.txt" (full Enter/Exit trace)
./syntax_analyzer -q              # quiet: only syntax errors and the summary
./syntax_analyzer other.txt       # parse a different symbol table file
```

Every run writes the parsing table to **`Parsing Table.txt`**. The exit code is `0` when the program is syntactically correct and `2` when
syntax errors were found.

## How it meets the syntax analyzer criteria

| Criteria                                        | Where                                                                                                    |
| ----------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| Input read one by one from the symbol table     | `lex()` → `readRow()` reads **one row** of `Symbol Table.txt` per call; the parser only sees `nextToken` |
| Parser algorithm: **Recursive Descent Parsing** | One C function per EBNF rule, driven by `lex()` / `nextToken` as in Sebesta                              |
| Transition diagram                              | `syntax_analyzer_explanation.txt`, section 10                                                            |
| Parsing table                                   | `Parsing Table.txt` (written on every run) and `syntax_analyzer_explanation.txt`, sections 11-12         |
| Input statement                                 | `input_stmt()` – `type IDENTIFIER = kunin(...)`                                                          |
| Output statement                                | `output_stmt()` – `sulat(...)`, `sulatln(...)`                                                           |
| Assignment statement                            | `assignment_stmt()`                                                                                      |
| Condition statement                             | `conditional_stmt()` – `kung` / `kundikung` / `kundi`                                                    |
| Iterative statement                             | `for_stmt()` (`bawat … sa`), `while_stmt()` (`habang`)                                                   |
| Declaration statement                           | `declaration_stmt()`                                                                                     |
| Error recovery – **method used: panic mode**    | `error()` → `guarded()` → `synchronize()`                                                                |
| Error messages                                  | `error()` / `errorMsg()` – message, code line, token number, token found                                 |

Also parsed: `return_stmt()` (`ibalik`), `call_stmt()`, `function_def()` (`itukoy`),
`match_stmt()` (`suri` / `sakali`), collections (list, dictionary, set), and the
full expression grammar (`o`, `at`, `hindi`, relational, `+ - * / % // **`).

## What the parser does with the Symbol Table

- **Comments and noise words** (`LINE_COMMENT`, `BLOCK_COMMENT`, `NOISE_WORD`)
  are skipped by `lex()`, because the grammar discards them before parsing.
- `NEWLINE`, `INDENT` and `DEDENT` rows are used as real tokens
  (`statement ::= simple_stmt NEWLINE`,
  `block ::= COLON NEWLINE INDENT statement { statement } DEDENT`).
- `INVALID` rows (lexical errors) are reported again as syntax errors where
  they appear.

## Output: `Parsing Table.txt`

One row for each step the parser takes:

```text
STEP   LINE  NON-TERMINAL               LOOKAHEAD       LEXEME                ACTION
----   ----  ------------               ---------       ------                ------
1      1     program                    RW_NUM          num                   apply: { statement }
2      1     statement                  RW_NUM          num                   apply: simple_stmt NEWLINE | compound_stmt
3      1     statement                  RW_NUM          num                   select: simple_stmt NEWLINE
...
8      1     type                       RW_NUM          num                   match: token consumed
```

| ACTION                                   | Meaning                                                    |
| ---------------------------------------- | ---------------------------------------------------------- |
| `apply: …`                               | the grammar rule is applied (its right-hand side is shown) |
| `select: …`                              | the lookahead token selected this alternative of the rule  |
| `match: token consumed`                  | the token was accepted and the next one was read           |
| `ERROR: …`                               | a syntax error (same message as on the screen)             |
| `PANIC MODE: …` / `discard (panic mode)` | error recovery                                             |

The end of the file has a SUMMARY (tokens read, rows, syntax errors, result) and
the list of every syntax error. See section 12 of
`syntax_analyzer_explanation.txt` for an example.

## Error recovery (panic mode)

When a rule fails, `error()` prints one message and jumps back to the closest
`guarded()` call (every statement and every `sakali` clause is guarded).
`synchronize()` then discards tokens up to a synchronizing token – `NEWLINE`
(end of the bad line), `DEDENT` (end of the block) or end of file – and skips
the indented block that belonged to the bad line. Parsing continues with the
next statement, so one run reports all the errors without false follow-up errors.

## Error messages

```text
Syntax error (code line 3, token #14): expected ':' to start the block, but found the end of the line
Syntax error (code line 11, token #52): expected 'sa' after the loop variable, but found IDENTIFIER 'nums'
Syntax error (code line 14, token #67): 'kundi' has no matching 'kung' before it
```

_code line_ counts the lines that contain code (the Symbol Table has no line
numbers, so blank and comment-only lines are not counted); _token #_ is the
position of the bad token in the Symbol Table.

## Syntax analyzer test files

| File                      | Result                                                                 |
| ------------------------- | ---------------------------------------------------------------------- |
| `syntax_test.tagpy`       | Every statement type – **0 syntax errors**                             |
| `syntax_error_test.tagpy` | 11 deliberate mistakes – all reported, parsing recovers after each one |

```bash
run.bat syntax_test.tagpy
run.bat syntax_error_test.tagpy
```

See `syntax_analyzer_explanation.txt` for a full explanation of the code, the
transition diagrams and the parsing table.

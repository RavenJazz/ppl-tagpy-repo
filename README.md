# Tagpy Lexical Analyzer

COSC 303 – Principles of Programming Languages. A hand-written lexical analyzer
for **Tagpy**. Tokenization is done with a finite automaton (`keyword_lookup`
is a DFA) plus one-character lookahead for operators; no premade lexer and no
string-comparison / word-matching functions are used.

## Input file type

Tagpy source files use the **`.tagpy`** extension (the analyzer rejects other
extensions).

## Compile

Single source file, so no build script is needed:

```bash
gcc front.c -o front          # Linux / macOS
gcc front.c -o front.exe      # Windows (MinGW)
```

## Run

```bash
./front sample.tagpy          # or: front.exe sample.tagpy
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

| Category | Token types |
|---|---|
| Identifier | `IDENTIFIER` (letters, digits, `_`, Ñ/ñ; case-insensitive keywords) |
| Key words | `KW_KUNG`, `KW_KUNDIKUNG`, `KW_KUNDI`, `KW_BAWAT`, `KW_SA`, `KW_ITUKOY`, `KW_IBALIK`, `KW_SURI`, `KW_SAKALI` |
| Reserved words | `RW_NUM`, `RW_TUL`, `RW_BAKA`, `RW_KAR`, `RW_PISI`, `RW_LISTA`, `RW_SULAT`, `RW_SULATLN`, `RW_KUNIN` |
| Constants | `INT_LITERAL`, `FLOAT_LITERAL`, `STRING_LITERAL` (`"…"` or `'…'`), `TRUE_LITERAL` (`totoo`), `FALSE_LITERAL` (`palso`) |
| Noise words | `NOISE_WORD` (`ang`, `ay`) |
| Comments | `LINE_COMMENT` (`# …`), `BLOCK_COMMENT` (`## … ##`) |
| Arithmetic operators (7) | `ADD +`, `SUBTRACT -`, `MULTIPLY *`, `DIVIDE /`, `EXPONENT **`, `MODULO %`, `FLOOR_DIVIDE //` |
| Boolean operators (9) | `EQUAL ==`, `NOT_EQUAL !=`, `LESS_THAN <`, `GREATER_THAN >`, `LESS_EQUAL <=`, `GREATER_EQUAL >=`, `AND at`, `OR o`, `NOT hindi` |
| Assignment | `ASSIGN =` |
| Delimiters / brackets | `LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE COLON COMMA SEMICOLON DOT` |
| Layout (off-side rule) | `NEWLINE`, `INDENT`, `DEDENT` (set `SHOW_LAYOUT_IN_TABLE` to 0 in `front.c` to hide them) |
| Invalid | `INVALID` – unknown characters, lone `!`, unterminated strings / block comments, numbers glued to letters (`3x`), unsupported non-ASCII, unmatched or unclosed brackets, bad indentation |

## Test files

| File | Purpose |
|---|---|
| `sample.tagpy` | One example of every token class |
| `keyword_test.tagpy` | Keywords vs. identifiers (`kung`, `kungx`, `Kung`, `num1`, `ng`, Ñ/ñ) |
| `invalid_test.tagpy` | Invalid-token handling |

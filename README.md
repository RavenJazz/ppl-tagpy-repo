# Getting Started

## Usage Instructions

1. **Copy the files** to your local machine.
2. **Modify input equation:** Update the `front.in` file with your desired expression.
   
   *Example `front.in`:*
   ```text
   x = 20 + 3
   ```

3. **Compile the program:**
   ```bash
   gcc front.c -o front.exe
   ```

4. **Run the executable:**
   ```bash
   ./front.exe
   ```

### Fixed-Field Lexer

The original Tagpy lexer remains the default. To use the fixed-field lexer,
change `fixed_field_mode` to `1` in `front.c`. It emits one token for each
configured field, including empty fields. Each token exposes its source
position through `tokenLine` and `tokenColumn`, and its field name through
`tokenFieldName`.

The default layout is FORTRAN-style, with 1-based inclusive columns:

| Field | Columns | Token |
|---|---:|---|
| label | 1-5 | `FIXED_FIELD_LABEL` |
| continuation | 6 | `FIXED_FIELD_CONTINUATION` |
| statement | 7-72 | `FIXED_FIELD_STATEMENT` |
| sequence | 73-80 | `FIXED_FIELD_SEQUENCE` |

Edit `fixedFieldLayout` and `fixedFieldCount` in `front.c` to configure another
layout. Short lines produce empty values for fields beyond the line. Text past
the last configured column is ignored. Tabs expand to the next 8-column tab
stop before columns are counted. Blank lines and lines beginning with `C`,
`c`, `*`, `!`, or `#` in column 1 are skipped. A nonblank, nonzero continuation
field appends that line's statement field to the preceding statement; a
continuation without a preceding statement is reported as an error. Field
columns and concatenated statement length are limited by `MAX_FIXED_COLUMNS`.
The queued fields for one logical statement are limited by `MAX_FIXED_TOKENS`.
Increase these constants if your format needs larger limits.

### Expected Output

```text
Next token is: 11, Next lexeme is x
Next token is: 20, Next lexeme is =
Next token is: 10, Next lexeme is 20
Next token is: 21, Next lexeme is +
Next token is: 10, Next lexeme is 3
Next token is: -1, Next lexeme is EOF
```

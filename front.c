/*
 * Tagpy Lexical Analyzer  (COSC 303 - Principles of Programming Languages)
 *
 * Reads a Tagpy source file (extension .tagpy) and writes every token it
 * recognizes to a text file named "Symbol Table.txt" using the format
 *
 *      TOKEN TYPE              LEXEME
 *
 * Keywords, reserved words, noise words and identifiers are recognized by a
 * hand-built DFA (keyword_lookup) - no string comparison or word matching
 * functions are used.  Operators and delimiters use one character of
 * lookahead.  Anything that is not a valid token is reported and written to
 * the table as INVALID; the analyzer keeps going.
 *
 * Build:  gcc front.c -o front
 * Run:    ./front sample.tagpy      (or just ./front to use sample.tagpy)
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Safe wrapper: tolower on a possibly-high byte */
#define LOWER(c) tolower((unsigned char)(c))

#define DEFAULT_INPUT "sample.tagpy"
#define SYMBOL_TABLE_FILE "Symbol Table.txt"
#define SHOW_LAYOUT_IN_TABLE 1 /* 1 = also list NEWLINE / INDENT / DEDENT */
#define LEXEME_MAX 4096
#define TAB_WIDTH 4

/* Global declarations */
int charClass;
char lexeme[LEXEME_MAX];
int nextChar; /* int so EOF and bytes >= 0x80 are handled safely */
int lexLen;
int lexTruncated = 0;
int nextToken;
int tokLine = 1; /* line on which the current token starts */
int errorCount = 0;
int tokenCount = 0;
FILE *in_fp;
FILE *out_fp;
int indent_stack[100] = {0};
int indent_top = 0;
int pending_dedents = 0;
int is_at_line_start = 1;
int line_has_token = 0;

/* Function declarations */
void addChar(void);
void getChar(void);
int peekChar(void);
int lex(void);
int lookup(int ch);
int keyword_lookup();
void pushBracket(char open);
void popBracket(char close);
void checkUnclosed(void);
void reportError(int line, const char *msg, const char *text);

/* Bracket matching and line tracking */
#define MAX_BRACKET_DEPTH 100
char bracketStack[MAX_BRACKET_DEPTH];
int bracketLine[MAX_BRACKET_DEPTH];
int bracketTop = 0;
int lineNo = 1;

/* Character classes */
#define LETTER 0
#define DIGIT 1
#define UNKNOWN 99

/* Token codes */
#define INT_LIT 10
#define IDENT 11
#define NOISE_WORD 13
#define ASSIGN_OP 20
#define ADD_OP 21
#define SUB_OP 22
#define MULT_OP 23
#define DIV_OP 24
#define LEFT_PAREN 25
#define RIGHT_PAREN 26
/* Arithmetic operator tokens */
#define EXP_OP 27       /* **  */
#define MOD_OP 28       /* %   */
#define FLOOR_DIV_OP 29 /* //  */
/* Boolean operator tokens */
#define EQ_OP 30  /* == */
#define NEQ_OP 31 /* != */
#define LT_OP 32  /* <  */
#define GT_OP 33  /* >  */
#define LE_OP 34  /* <= */
#define GE_OP 35  /* >= */
/* Logical operator tokens */
#define AT_OP 36    /* at    */
#define O_OP 37     /* o     */
#define HINDI_OP 38 /* hindi */
/* Literal tokens */
#define TRUE_LIT 39   /* totoo            */
#define FALSE_LIT 40  /* palso            */
#define FLOAT_LIT 41  /* 10.5             */
#define STRING_LIT 42 /* "text" or 'text' */
/* Index brackets */
#define LEFT_BRACKET 43  /* [ */
#define RIGHT_BRACKET 44 /* ] */
/* Statement keyword tokens */
#define KW_KUNG 45      /* kung      */
#define KW_KUNDIKUNG 46 /* kundikung */
#define KW_KUNDI 47     /* kundi     */
#define KW_BAWAT 48     /* bawat     */
#define KW_SA 49        /* sa        */
/* Declaration (data type) reserved word tokens */
#define TYPE_NUM 50   /* num   */
#define TYPE_TUL 51   /* tul   */
#define TYPE_BAKA 52  /* baka  */
#define TYPE_KAR 53   /* kar   */
#define TYPE_PISI 54  /* pisi  */
#define TYPE_LISTA 55 /* lista */
/* Input / output reserved word tokens */
#define IO_SULAT 56   /* sulat   */
#define IO_SULATLN 57 /* sulatln */
#define IO_KUNIN 58   /* kunin   */
/* Delimiters */
#define COLON 59 /* : */
#define COMMA 60 /* , */
/* Blank space / off-side rule tokens */
#define TOKEN_NEWLINE 61
#define TOKEN_INDENT 62
#define TOKEN_DEDENT 63
#define TOKEN_ERROR 64 /* INVALID */
#define MAX_INDENT_DEPTH 100
/* Brace tokens for dictionaries and sets */
#define LEFT_BRACE 65  /* { */
#define RIGHT_BRACE 66 /* } */
/* Function / type keyword tokens */
#define KW_ITUKOY 67 /* itukoy */
#define KW_IBALIK 68 /* ibalik */
#define KW_SURI 69   /* suri   */
#define KW_SAKALI 70 /* sakali */
/* Added for the Symbol Table */
#define SEMICOLON 71     /* ; */
#define DOT 72           /* . */
#define LINE_COMMENT 73  /* # ...        */
#define BLOCK_COMMENT 74 /* ## ... ##    */

/******************************************************/
/* tokenName - the TOKEN TYPE column of the Symbol Table */
const char *tokenName(int t)
{
    switch (t)
    {
    case IDENT:         return "IDENTIFIER";
    case NOISE_WORD:    return "NOISE_WORD";
    case INT_LIT:       return "INT_LITERAL";
    case FLOAT_LIT:     return "FLOAT_LITERAL";
    case STRING_LIT:    return "STRING_LITERAL";
    case TRUE_LIT:      return "TRUE_LITERAL";
    case FALSE_LIT:     return "FALSE_LITERAL";
    /* arithmetic operators (7) */
    case ADD_OP:        return "ADD";
    case SUB_OP:        return "SUBTRACT";
    case MULT_OP:       return "MULTIPLY";
    case DIV_OP:        return "DIVIDE";
    case EXP_OP:        return "EXPONENT";
    case MOD_OP:        return "MODULO";
    case FLOOR_DIV_OP:  return "FLOOR_DIVIDE";
    case ASSIGN_OP:     return "ASSIGN";
    /* boolean operators (9) */
    case EQ_OP:         return "EQUAL";
    case NEQ_OP:        return "NOT_EQUAL";
    case LT_OP:         return "LESS_THAN";
    case GT_OP:         return "GREATER_THAN";
    case LE_OP:         return "LESS_EQUAL";
    case GE_OP:         return "GREATER_EQUAL";
    case AT_OP:         return "AND";
    case O_OP:          return "OR";
    case HINDI_OP:      return "NOT";
    /* keywords */
    case KW_KUNG:       return "KW_KUNG";
    case KW_KUNDIKUNG:  return "KW_KUNDIKUNG";
    case KW_KUNDI:      return "KW_KUNDI";
    case KW_BAWAT:      return "KW_BAWAT";
    case KW_SA:         return "KW_SA";
    case KW_ITUKOY:     return "KW_ITUKOY";
    case KW_IBALIK:     return "KW_IBALIK";
    case KW_SURI:       return "KW_SURI";
    case KW_SAKALI:     return "KW_SAKALI";
    /* reserved words */
    case TYPE_NUM:      return "RW_NUM";
    case TYPE_TUL:      return "RW_TUL";
    case TYPE_BAKA:     return "RW_BAKA";
    case TYPE_KAR:      return "RW_KAR";
    case TYPE_PISI:     return "RW_PISI";
    case TYPE_LISTA:    return "RW_LISTA";
    case IO_SULAT:      return "RW_SULAT";
    case IO_SULATLN:    return "RW_SULATLN";
    case IO_KUNIN:      return "RW_KUNIN";
    /* delimiters and brackets */
    case LEFT_PAREN:    return "LPAREN";
    case RIGHT_PAREN:   return "RPAREN";
    case LEFT_BRACKET:  return "LBRACKET";
    case RIGHT_BRACKET: return "RBRACKET";
    case LEFT_BRACE:    return "LBRACE";
    case RIGHT_BRACE:   return "RBRACE";
    case COLON:         return "COLON";
    case COMMA:         return "COMMA";
    case SEMICOLON:     return "SEMICOLON";
    case DOT:           return "DOT";
    /* comments */
    case LINE_COMMENT:  return "LINE_COMMENT";
    case BLOCK_COMMENT: return "BLOCK_COMMENT";
    /* layout */
    case TOKEN_NEWLINE: return "NEWLINE";
    case TOKEN_INDENT:  return "INDENT";
    case TOKEN_DEDENT:  return "DEDENT";
    case TOKEN_ERROR:   return "INVALID";
    }
    return "UNKNOWN";
}

/******************************************************/
/* hasTagpyExtension - the input file must end in .tagpy (checked one
   character at a time, no string functions) */
int hasTagpyExtension(const char *path)
{
    static const char ext[] = {'.', 't', 'a', 'g', 'p', 'y'};
    int len = 0, i;

    while (path[len])
        len++;
    if (len < 6)
        return 0;
    for (i = 0; i < 6; i++)
        if (LOWER(path[len - 6 + i]) != ext[i])
            return 0;
    return 1;
}

/******************************************************/
/* writeLexeme - print a lexeme on one table row (control chars escaped) */
void writeLexeme(const char *s)
{
    for (; *s; s++)
    {
        if (*s == '\n')
            fputs("\\n", out_fp);
        else if (*s == '\r')
            fputs("\\r", out_fp);
        else if (*s == '\t')
            fputs("\\t", out_fp);
        else
            fputc(*s, out_fp);
    }
}

/******************************************************/
/* finish - record the token that was just scanned and return its code */
int finish(int tok)
{
    int isLayout = (tok == TOKEN_NEWLINE || tok == TOKEN_INDENT ||
                    tok == TOKEN_DEDENT);
    int isComment = (tok == LINE_COMMENT || tok == BLOCK_COMMENT);

    nextToken = tok;

    if (lexTruncated)
    {
        reportError(tokLine, "lexeme too long, truncated", lexeme);
        lexTruncated = 0;
    }

    if (SHOW_LAYOUT_IN_TABLE || !isLayout)
    {
        fprintf(out_fp, "%-24s", tokenName(tok));
        writeLexeme(lexeme);
        fputc('\n', out_fp);
        tokenCount++;
    }

    /* comments and layout tokens do not count as "something on this line" */
    if (!isLayout && !isComment)
        line_has_token = 1;
    return tok;
}

/* finishText - finish a token whose lexeme is a fixed label */
int finishText(int tok, const char *text)
{
    lexLen = 0;
    lexeme[0] = '\0';
    while (*text)
    {
        lexeme[lexLen++] = *text++;
        lexeme[lexLen] = '\0';
    }
    return finish(tok);
}

/******************************************************/
/* reportError - invalid / unrecognized input handling */
void reportError(int line, const char *msg, const char *text)
{
    errorCount++;
    if (text && text[0])
        fprintf(stderr, "Lexical error (line %d): %s: %s\n", line, msg, text);
    else
        fprintf(stderr, "Lexical error (line %d): %s\n", line, msg);
}

/******************************************************/
/* main driver */
int main(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : DEFAULT_INPUT;

    if (!hasTagpyExtension(path))
    {
        fprintf(stderr, "ERROR - input file '%s' must have the .tagpy extension\n", path);
        return 1;
    }
    if ((in_fp = fopen(path, "r")) == NULL)
    {
        fprintf(stderr, "ERROR - cannot open %s\n", path);
        return 1;
    }
    if ((out_fp = fopen(SYMBOL_TABLE_FILE, "w")) == NULL)
    {
        fprintf(stderr, "ERROR - cannot create %s\n", SYMBOL_TABLE_FILE);
        fclose(in_fp);
        return 1;
    }

    fprintf(out_fp, "%-24s%s\n", "TOKEN TYPE", "LEXEME");
    fprintf(out_fp, "%-24s%s\n", "----------", "------");

    getChar();
    do
    {
        lex();
    } while (nextToken != EOF);
    checkUnclosed(); /* report any bracket that never closed */

    fclose(in_fp);
    fclose(out_fp);
    printf("Scanned %s: %d tokens written to \"%s\", %d lexical error(s).\n",
           path, tokenCount, SYMBOL_TABLE_FILE, errorCount);
    return 0;
}

/*****************************************************/
/* peekChar - look at the next character without consuming it
   (needed for two-character operators) */
int peekChar(void)
{
    int c = getc(in_fp);
    if (c != EOF)
        ungetc(c, in_fp);
    return c;
}

/*****************************************************/
/* lookup - operators and delimiters.  Leaves nextChar on the LAST
   character of the token; lex() advances past it. */
int lookup(int ch)
{
    switch (ch)
    {
    case '(':
        addChar();
        pushBracket('(');
        return finish(LEFT_PAREN);
    case ')':
        addChar();
        popBracket(')');
        return finish(RIGHT_PAREN);
    case '+':
        addChar();
        return finish(ADD_OP);
    case '-':
        addChar();
        return finish(SUB_OP);
    case '*':
        addChar();
        if (peekChar() == '*') /* ** */
        {
            getChar();
            addChar();
            return finish(EXP_OP);
        }
        return finish(MULT_OP);
    case '/':
        addChar();
        if (peekChar() == '/') /* // */
        {
            getChar();
            addChar();
            return finish(FLOOR_DIV_OP);
        }
        return finish(DIV_OP);
    case '%':
        addChar();
        return finish(MOD_OP);
    case '=':
        addChar();
        if (peekChar() == '=') /* == */
        {
            getChar();
            addChar();
            return finish(EQ_OP);
        }
        return finish(ASSIGN_OP);
    case '!':
        addChar();
        if (peekChar() == '=') /* != */
        {
            getChar();
            addChar();
            return finish(NEQ_OP);
        }
        reportError(tokLine, "lone '!' (did you mean != or hindi?)", lexeme);
        return finish(TOKEN_ERROR);
    case '<':
        addChar();
        if (peekChar() == '=') /* <= */
        {
            getChar();
            addChar();
            return finish(LE_OP);
        }
        return finish(LT_OP);
    case '>':
        addChar();
        if (peekChar() == '=') /* >= */
        {
            getChar();
            addChar();
            return finish(GE_OP);
        }
        return finish(GT_OP);
    case ':':
        addChar();
        return finish(COLON);
    case ',':
        addChar();
        return finish(COMMA);
    case ';':
        addChar();
        return finish(SEMICOLON);
    case '.':
        addChar();
        return finish(DOT);
    case '[':
        addChar();
        pushBracket('[');
        return finish(LEFT_BRACKET);
    case ']':
        addChar();
        popBracket(']');
        return finish(RIGHT_BRACKET);
    case '{':
        addChar();
        pushBracket('{');
        return finish(LEFT_BRACE);
    case '}':
        addChar();
        popBracket('}');
        return finish(RIGHT_BRACE);
    default:
        addChar();
        reportError(tokLine, "unknown character", lexeme);
        return finish(TOKEN_ERROR);
    }
}

/*****************************************************/
/* addChar - add nextChar to lexeme (long lexemes are truncated) */
void addChar(void)
{
    if (lexLen < LEXEME_MAX - 1)
    {
        lexeme[lexLen++] = (char)nextChar;
        lexeme[lexLen] = 0;
    }
    else
        lexTruncated = 1;
}

/*****************************************************/
/* pushBracket - remember an opening bracket ( [ { and its line */
void pushBracket(char open)
{
    if (bracketTop < MAX_BRACKET_DEPTH)
    {
        bracketStack[bracketTop] = open;
        bracketLine[bracketTop] = lineNo;
        bracketTop++;
    }
    else
        reportError(lineNo, "brackets nested too deeply", NULL);
}

/*****************************************************/
/* popBracket - check that a closing bracket matches the most
   recent open bracket */
void popBracket(char close)
{
    char open;
    char msg[96];

    if (bracketTop == 0)
    {
        snprintf(msg, sizeof msg, "'%c' has no matching opening bracket", close);
        reportError(lineNo, msg, NULL);
        return;
    }
    bracketTop--;
    open = bracketStack[bracketTop];
    if ((open == '(' && close != ')') ||
        (open == '[' && close != ']') ||
        (open == '{' && close != '}'))
    {
        snprintf(msg, sizeof msg, "'%c' does not match '%c' from line %d",
                 close, open, bracketLine[bracketTop]);
        reportError(lineNo, msg, NULL);
    }
}

/*****************************************************/
/* checkUnclosed - any bracket still on the stack at end of input
   was never closed; it is written to the table as INVALID */
void checkUnclosed(void)
{
    while (bracketTop > 0)
    {
        char msg[96];
        bracketTop--;
        snprintf(msg, sizeof msg, "'%c' is never closed", bracketStack[bracketTop]);
        reportError(bracketLine[bracketTop], msg, NULL);
        lexLen = 0;
        lexeme[0] = '\0';
        lexeme[lexLen++] = bracketStack[bracketTop];
        lexeme[lexLen] = '\0';
        finish(TOKEN_ERROR);
    }
}

/*****************************************************/
/* getChar - get the next character and its class.
   Supports ASCII characters and Ñ / ñ only. */
void getChar(void)
{
    static int enyeContinuation = 0;
    int c = getc(in_fp);

    if (c == EOF)
    {
        enyeContinuation = 0;
        nextChar = EOF;
        charClass = EOF;
        return;
    }

    nextChar = c;

    if (nextChar == '\n')
        lineNo++;

    /* Second UTF-8 byte of Ñ / ñ */
    if (enyeContinuation)
    {
        enyeContinuation = 0;
        charClass = LETTER;
        return;
    }

    /* Ñ = C3 91, ñ = C3 B1 */
    if (nextChar == 0xC3)
    {
        int following = peekChar();

        if (following == 0x91 || following == 0xB1)
        {
            enyeContinuation = 1;
            charClass = LETTER;
        }
        else
        {
            charClass = UNKNOWN;
        }
        return;
    }

    if ((nextChar >= 'A' && nextChar <= 'Z') ||
        (nextChar >= 'a' && nextChar <= 'z'))
        charClass = LETTER;
    else if (nextChar >= '0' && nextChar <= '9')
        charClass = DIGIT;
    else
        charClass = UNKNOWN;
}

/*****************************************************/
/* lexComment - "# ..." (line) or "## ... ##" (block).  nextChar is '#'. */
int lexComment(void)
{
    addChar(); /* first '#' */
    if (peekChar() == '#') /* block comment */
    {
        int closed = 0;

        getChar();
        addChar(); /* second '#' */
        getChar();
        while (nextChar != EOF)
        {
            if (nextChar == '#' && peekChar() == '#')
            {
                addChar();
                getChar();
                addChar();
                getChar();
                closed = 1;
                break;
            }
            addChar();
            getChar();
        }
        if (!closed)
        {
            reportError(tokLine, "unterminated block comment", NULL);
            return finish(TOKEN_ERROR);
        }
        return finish(BLOCK_COMMENT);
    }

    getChar();
    while (nextChar != EOF && nextChar != '\n')
    {
        addChar();
        getChar();
    }
    if (lexLen > 0 && lexeme[lexLen - 1] == '\r') /* CRLF files */
        lexeme[--lexLen] = '\0';
    return finish(LINE_COMMENT);
}

/*****************************************************/
/* lexString - "text" or 'text'.  nextChar is the opening quote. */
int lexString(int quote)
{
    addChar(); /* opening quote */
    getChar();
    while (nextChar != EOF && nextChar != '\n' && nextChar != quote)
    {
        if (nextChar == '\\') /* escape: keep the next character as-is */
        {
            addChar();
            getChar();
            if (nextChar == EOF || nextChar == '\n')
                break;
        }
        addChar();
        getChar();
    }
    if (nextChar == quote)
    {
        addChar();
        getChar();
        return finish(STRING_LIT);
    }
    reportError(tokLine, "unterminated string", lexeme);
    return finish(TOKEN_ERROR); /* newline / EOF is left for lex() */
}

/*****************************************************/
/* lexNumber - integer or float literal.  Digits followed directly by
   letters (3x) are one INVALID token. */
int lexNumber(void)
{
    int tok = INT_LIT;

    while (charClass == DIGIT)
    {
        addChar();
        getChar();
    }
    if (nextChar == '.' && isdigit(peekChar())) /* float literal */
    {
        addChar(); /* the '.' */
        getChar();
        while (charClass == DIGIT)
        {
            addChar();
            getChar();
        }
        tok = FLOAT_LIT;
    }
    if (charClass == LETTER || nextChar == '_')
    {
        while (charClass == LETTER || charClass == DIGIT || nextChar == '_')
        {
            addChar();
            getChar();
        }
        reportError(tokLine, "invalid number or identifier", lexeme);
        return finish(TOKEN_ERROR);
    }
    return finish(tok);
}

/*****************************************************/
/* lexNonAscii - a run of bytes >= 0x80 that is not Ñ / ñ is INVALID */
int lexNonAscii(void)
{
    while (nextChar != EOF && nextChar >= 0x80 && charClass == UNKNOWN)
    {
        addChar();
        getChar();
    }
    reportError(tokLine, "unsupported non-ASCII character", NULL);
    return finish(TOKEN_ERROR);
}

/*****************************************************/
/* lex - scan one token.  Besides the real tokens it produces the
   NEWLINE / INDENT / DEDENT layout tokens (off-side rule). */
int lex(void)
{
    lexLen = 0;
    lexeme[0] = '\0';

    if (pending_dedents > 0)
    {
        pending_dedents--;
        return finishText(TOKEN_DEDENT, "DEDENT");
    }

    for (;;)
    {
        /* start of a line: measure its indentation (not inside brackets) */
        if (is_at_line_start && bracketTop == 0)
        {
            int space_count = 0;

            while (nextChar == ' ' || nextChar == '\t' || nextChar == '\r')
            {
                if (nextChar == ' ')
                    space_count++;
                else if (nextChar == '\t')
                    space_count += TAB_WIDTH;
                getChar();
            }

            if (nextChar == '\n') /* blank line */
            {
                getChar();
                continue;
            }
            if (nextChar == EOF || nextChar == '#')
            {
                /* comment-only lines never change the indentation */
                is_at_line_start = 0;
            }
            else
            {
                is_at_line_start = 0;
                if (space_count > indent_stack[indent_top])
                {
                    if (indent_top >= MAX_INDENT_DEPTH - 1)
                    {
                        tokLine = lineNo;
                        reportError(tokLine, "indentation too deep", NULL);
                        return finishText(TOKEN_ERROR, "INDENT");
                    }
                    indent_stack[++indent_top] = space_count;
                    return finishText(TOKEN_INDENT, "INDENT");
                }
                if (space_count < indent_stack[indent_top])
                {
                    while (indent_top > 0 && space_count < indent_stack[indent_top])
                    {
                        indent_top--;
                        pending_dedents++;
                    }
                    if (space_count != indent_stack[indent_top])
                    {
                        tokLine = lineNo;
                        reportError(tokLine,
                                    "unindent does not match any outer indentation level",
                                    NULL);
                        return finishText(TOKEN_ERROR, "BAD_DEDENT");
                    }
                    pending_dedents--;
                    return finishText(TOKEN_DEDENT, "DEDENT");
                }
            }
        }

        /* blanks and line ends inside a line */
        if (nextChar == ' ' || nextChar == '\t' || nextChar == '\r' ||
            nextChar == '\f' || nextChar == '\v')
        {
            getChar();
            continue;
        }
        if (nextChar == '\n')
        {
            getChar();
            /* a newline inside ( ) [ ] { } does not end the statement
               and the next line's indentation is not measured */
            if (bracketTop > 0)
                continue;
            is_at_line_start = 1;
            if (line_has_token)
            {
                line_has_token = 0;
                return finishText(TOKEN_NEWLINE, "\n");
            }
            continue;
        }
        break;
    }

    tokLine = lineNo;

    if (nextChar == EOF)
    {
        /* the file ended without a final newline: close the last statement */
        if (line_has_token)
        {
            line_has_token = 0;
            return finishText(TOKEN_NEWLINE, "\n");
        }
        if (indent_top > 0)
        {
            pending_dedents = indent_top - 1;
            indent_top = 0;
            return finishText(TOKEN_DEDENT, "DEDENT");
        }
        nextToken = EOF;
        return nextToken; /* EOF is not a lexeme: nothing is written */
    }

    if (nextChar == '#')
        return lexComment();
    if (charClass == LETTER || nextChar == '_')
        return finish(keyword_lookup()); /* keyword / noise word / identifier */
    if (charClass == DIGIT)
        return lexNumber();
    if (nextChar == '"' || nextChar == '\'')
        return lexString(nextChar);
    if (nextChar >= 0x80)
        return lexNonAscii();

    /* operators and delimiters */
    lookup(nextChar);
    getChar();
    return nextToken;
}

/*****************************************************/
/* keyword_lookup - keywords, reserved words, noise words, and
   identifiers using a DFA state machine (case-insensitive).
   "ng" is a normal identifier. "ang" and "ay" are noise words. */
int keyword_lookup()
{
    int state = 0;

    addChar();

    switch (LOWER(nextChar))
    {
    case 'a':
        state = 1;
        break;
    case 'o':
        state = 6;
        break;
    case 'h':
        state = 7;
        break;
    case 't':
        state = 12;
        break;
    case 'p':
        state = 17;
        break;
    case 'b':
        state = 22;
        break;
    case 'k':
        state = 29;
        break;
    case 'l':
        state = 43;
        break;
    case 'n':
        state = 48;
        break;
    case 's':
        state = 55;
        break;
    case 'i':
        state = 66;
        break; /* itukoy, ibalik */
    default:
        state = 99;
        break;
    }
    getChar();

    while (charClass == LETTER || charClass == DIGIT || nextChar == '_')
    {
        int ch;
        addChar();
        ch = LOWER(nextChar);

        switch (state)
        {
        /* a... : ay, ang, at */
        case 1:
            if (ch == 'y')
                state = 2;
            else if (ch == 'n')
                state = 3;
            else if (ch == 't')
                state = 5;
            else
                state = 99;
            break;
        case 3:
            state = (ch == 'g') ? 4 : 99;
            break;

        /* hindi: 7-11 */
        case 7:
            state = (ch == 'i') ? 8 : 99;
            break;
        case 8:
            state = (ch == 'n') ? 9 : 99;
            break;
        case 9:
            state = (ch == 'd') ? 10 : 99;
            break;
        case 10:
            state = (ch == 'i') ? 11 : 99;
            break;

        /* totoo: 12-16, tul: 12,64,65 */
        case 12:
            if (ch == 'o')
                state = 13;
            else if (ch == 'u')
                state = 64;
            else
                state = 99;
            break;
        case 13:
            state = (ch == 't') ? 14 : 99;
            break;
        case 14:
            state = (ch == 'o') ? 15 : 99;
            break;
        case 15:
            state = (ch == 'o') ? 16 : 99;
            break;
        case 64:
            state = (ch == 'l') ? 65 : 99;
            break;

        /* palso: 17-21, pisi: 17,52-54 */
        case 17:
            if (ch == 'a')
                state = 18;
            else if (ch == 'i')
                state = 52;
            else
                state = 99;
            break;
        case 18:
            state = (ch == 'l') ? 19 : 99;
            break;
        case 19:
            state = (ch == 's') ? 20 : 99;
            break;
        case 20:
            state = (ch == 'o') ? 21 : 99;
            break;
        case 52:
            state = (ch == 's') ? 53 : 99;
            break;
        case 53:
            state = (ch == 'i') ? 54 : 99;
            break;

        /* baka, bawat: 22-28 */
        case 22:
            state = (ch == 'a') ? 23 : 99;
            break;
        case 23:
            if (ch == 'k')
                state = 24;
            else if (ch == 'w')
                state = 26;
            else
                state = 99;
            break;
        case 24:
            state = (ch == 'a') ? 25 : 99;
            break;
        case 26:
            state = (ch == 'a') ? 27 : 99;
            break;
        case 27:
            state = (ch == 't') ? 28 : 99;
            break;

        /* kar, kung, kundi, kundikung, kunin: 29-42 */
        case 29:
            if (ch == 'a')
                state = 30;
            else if (ch == 'u')
                state = 32;
            else
                state = 99;
            break;
        case 30:
            state = (ch == 'r') ? 31 : 99;
            break;
        case 32:
            state = (ch == 'n') ? 33 : 99;
            break;
        case 33:
            if (ch == 'd')
                state = 34;
            else if (ch == 'g')
                state = 40;
            else if (ch == 'i')
                state = 41;
            else
                state = 99;
            break;
        case 34:
            state = (ch == 'i') ? 35 : 99;
            break;
        case 35:
            state = (ch == 'k') ? 36 : 99;
            break;
        case 36:
            state = (ch == 'u') ? 37 : 99;
            break;
        case 37:
            state = (ch == 'n') ? 38 : 99;
            break;
        case 38:
            state = (ch == 'g') ? 39 : 99;
            break;
        case 41:
            state = (ch == 'n') ? 42 : 99;
            break;

        /* lista: 43-47 */
        case 43:
            state = (ch == 'i') ? 44 : 99;
            break;
        case 44:
            state = (ch == 's') ? 45 : 99;
            break;
        case 45:
            state = (ch == 't') ? 46 : 99;
            break;
        case 46:
            state = (ch == 'a') ? 47 : 99;
            break;

        /* num: 48-50 (ng is an identifier) */
        case 48:
            if (ch == 'g')
                state = 99;
            else if (ch == 'u')
                state = 49;
            else
                state = 99;
            break;
        case 49:
            state = (ch == 'm') ? 50 : 99;
            break;

        /* s...: sa, sakali, sulat, sulatln, suri */
        case 55:
            if (ch == 'a')
                state = 56;
            else if (ch == 'u')
                state = 57;
            else
                state = 99;
            break;
        case 56:
            state = (ch == 'k') ? 79 : 99;
            break; /* sa -> sak */
        case 79:
            state = (ch == 'a') ? 80 : 99;
            break;
        case 80:
            state = (ch == 'l') ? 81 : 99;
            break;
        case 81:
            state = (ch == 'i') ? 82 : 99;
            break;
        case 57:
            if (ch == 'l')
                state = 58;
            else if (ch == 'r')
                state = 77;
            else
                state = 99;
            break;
        case 58:
            state = (ch == 'a') ? 59 : 99;
            break;
        case 59:
            state = (ch == 't') ? 60 : 99;
            break;
        case 60:
            state = (ch == 'l') ? 61 : 99;
            break;
        case 61:
            state = (ch == 'n') ? 62 : 99;
            break;
        case 77:
            state = (ch == 'i') ? 78 : 99;
            break;

        /* i...: itukoy, ibalik */
        case 66:
            if (ch == 't')
                state = 67;
            else if (ch == 'b')
                state = 72;
            else
                state = 99;
            break;
        case 67:
            state = (ch == 'u') ? 68 : 99;
            break;
        case 68:
            state = (ch == 'k') ? 69 : 99;
            break;
        case 69:
            state = (ch == 'o') ? 70 : 99;
            break;
        case 70:
            state = (ch == 'y') ? 71 : 99;
            break;
        case 72:
            state = (ch == 'a') ? 73 : 99;
            break;
        case 73:
            state = (ch == 'l') ? 74 : 99;
            break;
        case 74:
            state = (ch == 'i') ? 75 : 99;
            break;
        case 75:
            state = (ch == 'k') ? 76 : 99;
            break;

        default:
            state = 99; /* sink state: identifier */
            break;
        }
        getChar();
    }

    /* Return the token for the final state reached */
    /* [EXPR] expression words */
    if (state == 5)
        return AT_OP;
    if (state == 6)
        return O_OP;
    if (state == 11)
        return HINDI_OP;
    if (state == 16)
        return TRUE_LIT;
    if (state == 21)
        return FALSE_LIT;
    /* [STMT] statement keywords */
    if (state == 25)
        return TYPE_BAKA;
    if (state == 28)
        return KW_BAWAT;
    if (state == 31)
        return TYPE_KAR;
    if (state == 35)
        return KW_KUNDI;
    if (state == 39)
        return KW_KUNDIKUNG;
    if (state == 40)
        return KW_KUNG;
    if (state == 42)
        return IO_KUNIN;
    if (state == 47)
        return TYPE_LISTA;
    if (state == 50)
        return TYPE_NUM;
    if (state == 54)
        return TYPE_PISI;
    if (state == 56)
        return KW_SA;
    if (state == 60)
        return IO_SULAT;
    if (state == 62)
        return IO_SULATLN;
    if (state == 65)
        return TYPE_TUL;
    /* [KWD] function / type keywords */
    if (state == 71)
        return KW_ITUKOY;
    if (state == 76)
        return KW_IBALIK;
    if (state == 78)
        return KW_SURI;
    if (state == 82)
        return KW_SAKALI;

    if (state == 2 || state == 4)
        return NOISE_WORD;

    return IDENT;
}
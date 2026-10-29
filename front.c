#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* Safe wrapper: tolower on a possibly-high byte */
#define LOWER(c) tolower((unsigned char)(c))

/* Global declarations */
int charClass;
char lexeme[1024];
int nextChar; /* int so EOF and bytes >= 0x80 are handled safely */
int lexLen;
int token;
int nextToken;
FILE *in_fp;
int indent_stack[100] = {0};
int indent_top = 0;
int pending_dedents = 0;
int is_at_line_start = 1;
int line_has_token = 0;
int skipAdvance = 0; /* set by lookup() when it must not consume the next char */

/* Function declarations */
void addChar();
void getChar();
void getNonBlank();
int peekChar();
int lex();
int lookup(int ch);
int keyword_lookup();
void pushBracket(char open);
void popBracket(char close);
void checkUnclosed();

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
/* [EXPR] Logical operator tokens */
#define AT_OP 36    /* at    */
#define O_OP 37     /* o     */
#define HINDI_OP 38 /* hindi */
/* [EXPR] Literal tokens */
#define TRUE_LIT 39   /* totoo            */
#define FALSE_LIT 40  /* palso            */
#define FLOAT_LIT 41  /* 10.5             */
#define STRING_LIT 42 /* "text" or 'text' */
/* [EXPR] Index brackets */
#define LEFT_BRACKET 43  /* [ */
#define RIGHT_BRACKET 44 /* ] */
/* [STMT] Statement keyword tokens */
#define KW_KUNG 45      /* kung      */
#define KW_KUNDIKUNG 46 /* kundikung */
#define KW_KUNDI 47     /* kundi     */
#define KW_BAWAT 48     /* bawat     */
#define KW_SA 49        /* sa        */
/* [STMT] Declaration keyword tokens */
#define TYPE_NUM 50   /* num   */
#define TYPE_TUL 51   /* tul   */
#define TYPE_BAKA 52  /* baka  */
#define TYPE_KAR 53   /* kar   */
#define TYPE_PISI 54  /* pisi  */
#define TYPE_LISTA 55 /* lista */
/* [STMT] Input / output keyword tokens */
#define IO_SULAT 56   /* sulat   */
#define IO_SULATLN 57 /* sulatln */
#define IO_KUNIN 58   /* kunin   */
/* [STMT] Delimiters used by statements */
#define COLON 59 /* : */
#define COMMA 60 /* , */
/* [LAYOUT] Blank space / off-side rule tokens */
#define TOKEN_NEWLINE 61
#define TOKEN_INDENT 62
#define TOKEN_DEDENT 63
#define TOKEN_ERROR 64
#define MAX_INDENT_DEPTH 100
/* [DELIM] Brace tokens for dictionaries and sets */
#define LEFT_BRACE 65  /* { */
#define RIGHT_BRACE 66 /* } */
/* [KWD] Function / type keyword tokens */
#define KW_ITUKOY 67 /* itukoy */
#define KW_IBALIK 68 /* ibalik */
#define KW_SURI 69   /* suri   */
#define KW_SAKALI 70 /* sakali */

/******************************************************/
/* main driver */
int main()
{
    /* Open the input data file and process its contents */
    if ((in_fp = fopen("front.in", "r")) == NULL)
    {
        printf("ERROR - cannot open front.in \n");
        return 1;
    }

    getChar();
    do
    {
        lex();
    } while (nextToken != EOF);
    checkUnclosed(); /* report any bracket that never closed */
    fclose(in_fp);
    return 0;
}

/*****************************************************/
/* peekChar - look at the next character without consuming it
   (needed for two-character operators) */
int peekChar()
{
    int c = getc(in_fp);
    if (c != EOF)
        ungetc(c, in_fp);
    return c;
}

/*****************************************************/
/* lookup - operators, delimiters, strings */
int lookup(int ch)
{
    switch (ch)
    {
    case '(':
        addChar();
        pushBracket('(');
        nextToken = LEFT_PAREN;
        break;
    case ')':
        addChar();
        popBracket(')');
        nextToken = RIGHT_PAREN;
        break;
    case '+':
        addChar();
        nextToken = ADD_OP;
        break;
    case '-':
        addChar();
        nextToken = SUB_OP;
        break;
    case '*':
        addChar();
        if (peekChar() == '*') /* ** */
        {
            getChar();
            addChar();
            nextToken = EXP_OP;
        }
        else
            nextToken = MULT_OP;
        break;
    case '/':
        addChar();
        if (peekChar() == '/') /* // */
        {
            getChar();
            addChar();
            nextToken = FLOOR_DIV_OP;
        }
        else
            nextToken = DIV_OP;
        break;
    case '%':
        addChar();
        nextToken = MOD_OP;
        break;
    case '=':
        addChar();
        if (peekChar() == '=') /* == */
        {
            getChar();
            addChar();
            nextToken = EQ_OP;
        }
        else
            nextToken = ASSIGN_OP;
        break;
    case '!':
        addChar();
        if (peekChar() == '=') /* != */
        {
            getChar();
            addChar();
            nextToken = NEQ_OP;
        }
        else
        {
            printf("Error - lone '!' on line %d \n", lineNo);
            nextToken = TOKEN_ERROR; /* report and keep lexing */
        }
        break;
    case '<':
        addChar();
        if (peekChar() == '=') /* <= */
        {
            getChar();
            addChar();
            nextToken = LE_OP;
        }
        else
            nextToken = LT_OP;
        break;
    case '>':
        addChar();
        if (peekChar() == '=') /* >= */
        {
            getChar();
            addChar();
            nextToken = GE_OP;
        }
        else
            nextToken = GT_OP;
        break;
    case ':':
        addChar();
        nextToken = COLON;
        break;
    case ',':
        addChar();
        nextToken = COMMA;
        break;
    case '[':
        addChar();
        pushBracket('[');
        nextToken = LEFT_BRACKET;
        break;
    case ']':
        addChar();
        popBracket(']');
        nextToken = RIGHT_BRACKET;
        break;
    case '{':
        addChar();
        pushBracket('{');
        nextToken = LEFT_BRACE;
        break;
    case '}':
        addChar();
        popBracket('}');
        nextToken = RIGHT_BRACE;
        break;
    case '"':
    case '\'':
        lexeme[0] = 0;
        lexLen = 0;
        getChar();
        while (charClass != EOF && nextChar != ch && nextChar != '\n')
        {
            addChar();
            getChar();
        }
        if (nextChar == ch) /* closing quote found; lex() consumes it */
            nextToken = STRING_LIT;
        else
        {
            printf("Error - unterminated string on line %d \n", lineNo);
            nextToken = TOKEN_ERROR;
            skipAdvance = 1; /* don't swallow the newline / EOF */
        }
        break;
    default:
        addChar();
        printf("Error - unknown character '%c' on line %d \n", ch, lineNo);
        nextToken = TOKEN_ERROR; /* report and keep lexing */
        break;
    }
    return nextToken;
}

/*****************************************************/
/* addChar - add nextChar to lexeme */
void addChar()
{
    if (lexLen < (int)sizeof lexeme - 1)
    {
        lexeme[lexLen++] = (char)nextChar;
        lexeme[lexLen] = 0;
    }
    else
        printf("Error - lexeme is too long \n");
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
        printf("Error - brackets nested too deeply \n");
}

/*****************************************************/
/* popBracket - check that a closing bracket matches the most
   recent open bracket */
void popBracket(char close)
{
    char open;

    if (bracketTop == 0)
    {
        printf("Error - '%c' on line %d has no matching opening bracket \n",
               close, lineNo);
        return;
    }
    bracketTop--;
    open = bracketStack[bracketTop];
    if ((open == '(' && close != ')') ||
        (open == '[' && close != ']') ||
        (open == '{' && close != '}'))
        printf("Error - '%c' on line %d does not match '%c' from line %d \n",
               close, lineNo, open, bracketLine[bracketTop]);
}

/*****************************************************/
/* checkUnclosed - any bracket still on the stack at end of input
   was never closed */
void checkUnclosed()
{
    while (bracketTop > 0)
    {
        bracketTop--;
        printf("Error - '%c' from line %d is never closed \n",
               bracketStack[bracketTop], bracketLine[bracketTop]);
    }
}

/*****************************************************/
/* getChar - get the next character and its class.
   Supports ASCII characters and Ñ / ñ only. */
void getChar()
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
/* getNonBlank - skip whitespace and comments (# / ## ... ##) */
void getNonBlank()
{
    while (isspace((unsigned char)nextChar) || nextChar == '#')
    {
        if (isspace((unsigned char)nextChar))
        {
            getChar();
        }
        else if (nextChar == '#')
        {
            if (peekChar() == '#')
            {
                getChar();
                getChar();
                while (nextChar != EOF)
                {
                    if (nextChar == '#' && peekChar() == '#')
                    {
                        getChar();
                        getChar();
                        break;
                    }
                    getChar();
                }
            }
            else
            {
                while (nextChar != EOF && nextChar != '\n')
                    getChar();
            }
        }
    }
}

/*****************************************************/
/* lex - a lexical analyzer for Tagpy: tokens plus the
   NEWLINE / INDENT / DEDENT layout tokens */
int lex()
{
    lexLen = 0;
    lexeme[0] = '\0';

    if (pending_dedents > 0)
    {
        pending_dedents--;
        nextToken = TOKEN_DEDENT;
        snprintf(lexeme, sizeof lexeme, "DEDENT");
        printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
        return nextToken;
    }

    for (;;)
    {
        if (is_at_line_start)
        {
            int space_count = 0;

            while (nextChar == ' ' || nextChar == '\t')
            {
                space_count += (nextChar == '\t') ? 4 : 1;
                getChar();
            }

            /* Ignore blank lines and comment-only lines. */
            if (nextChar == '\n' || (nextChar == '\r' && peekChar() == '\n'))
            {
                getChar();
                if (nextChar == '\n')
                    getChar();
                continue;
            }

            if (nextChar == '#')
            {
                if (peekChar() == '#')
                {
                    getChar();
                    getChar();
                    while (charClass != EOF)
                    {
                        if (nextChar == '#' && peekChar() == '#')
                        {
                            getChar();
                            getChar();
                            break;
                        }
                        if (nextChar == '\n')
                        {
                            is_at_line_start = 1;
                            line_has_token = 0;
                        }
                        getChar();
                    }
                    if (is_at_line_start)
                        continue;
                }
                else
                {
                    while (charClass != EOF && nextChar != '\n')
                        getChar();
                    if (nextChar == '\n')
                        getChar();
                    continue;
                }
            }

            if (charClass == EOF)
            {
                if (indent_top > 0)
                {
                    pending_dedents = indent_top - 1;
                    indent_top = 0;
                    nextToken = TOKEN_DEDENT;
                    snprintf(lexeme, sizeof lexeme, "DEDENT");
                    printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                    return nextToken;
                }
                nextToken = EOF;
                strcpy(lexeme, "EOF");
                printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                return nextToken;
            }

            is_at_line_start = 0;
            if (space_count > indent_stack[indent_top])
            {
                if (indent_top >= MAX_INDENT_DEPTH - 1)
                {
                    printf("Error: Indentation too deep.\n");
                    nextToken = TOKEN_ERROR;
                    strcpy(lexeme, "ERROR");
                    printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                    return nextToken;
                }
                indent_stack[++indent_top] = space_count;
                nextToken = TOKEN_INDENT;
                strcpy(lexeme, "INDENT");
                printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                return nextToken;
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
                    printf("Error: Unindent does not match any outer indentation level.\n");
                    nextToken = TOKEN_ERROR;
                    strcpy(lexeme, "ERROR");
                    printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                    return nextToken;
                }
                pending_dedents--;
                nextToken = TOKEN_DEDENT;
                strcpy(lexeme, "DEDENT");
                printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                return nextToken;
            }
        }

        if (nextChar == '#')
        {
            if (peekChar() == '#')
            {
                getChar();
                getChar();
                while (charClass != EOF)
                {
                    if (nextChar == '#' && peekChar() == '#')
                    {
                        getChar();
                        getChar();
                        break;
                    }
                    if (nextChar == '\n' && bracketTop == 0)
                    {
                        is_at_line_start = 1;
                        line_has_token = 0;
                    }
                    getChar();
                }
                continue;
            }
            while (charClass != EOF && nextChar != '\n')
                getChar();
            continue;
        }

        if (nextChar != EOF && isspace((unsigned char)nextChar))
        {
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
                    nextToken = TOKEN_NEWLINE;
                    strcpy(lexeme, "NEWLINE");
                    printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
                    return nextToken;
                }
                continue;
            }
            getChar();
            continue;
        }
        break;
    }

    switch (charClass)
    {
    /* identifiers, keywords, noise words */
    case LETTER:
        nextToken = keyword_lookup();
        break;
    /* integer and float literals */
    case DIGIT:
        addChar();
        getChar();
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
            nextToken = FLOAT_LIT;
        }
        else
            nextToken = INT_LIT;
        break;
    /* parentheses, operators, delimiters, strings */
    case UNKNOWN:
        if (nextChar >= 0x80)
        {
            fprintf(stderr,
                    "Lexical error: unsupported non-ASCII character at line %d\n",
                    lineNo);
            exit(1);
        }
        skipAdvance = 0;
        lookup(nextChar);
        if (!skipAdvance)
            getChar();
        skipAdvance = 0;
        break;
    case EOF:
        /* the file ended without a final newline: close the last statement */
        if (line_has_token)
        {
            line_has_token = 0;
            nextToken = TOKEN_NEWLINE;
            strcpy(lexeme, "NEWLINE");
            printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
            return nextToken;
        }
        if (indent_top > 0)
        {
            pending_dedents = indent_top - 1;
            indent_top = 0;
            nextToken = TOKEN_DEDENT;
            strcpy(lexeme, "DEDENT");
            printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
            return nextToken;
        }
        nextToken = EOF;
        strcpy(lexeme, "EOF");
        break;
    } /* End of switch */
    if (nextToken != EOF)
        line_has_token = 1;
    printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
    return nextToken;
} /* End of function lex */

/*****************************************************/
/* keyword_lookup - keywords, noise words, and identifiers using a
   DFA state machine (case-insensitive).
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
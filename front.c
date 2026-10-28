#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* Global declarations */
/* Variables */
int charClass;
char lexeme[1024];
char nextChar;
int lexLen;
int token;
int nextToken;
FILE *in_fp, *fopen(const char *filename, const char *mode);
int indent_stack[100] = {0};
int indent_top = 0;
int pending_dedents = 0;
int is_at_line_start = 1;
int line_has_token = 0;
int tokenLine = 1;
int tokenColumn = 1;
const char *tokenFieldName = NULL;
/* Function declarations */
void addChar();
void getChar();
void getNonBlank();
int peekChar();
int lex();
int keyword_lookup();
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
/* New arithmetic operator tokens */
#define EXP_OP 27       /* **  */
#define MOD_OP 28       /* %   */
#define FLOOR_DIV_OP 29 /* //  */
/* New boolean operator tokens */
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
/* [EXPR] Literal tokens (primary expressions) */
#define TRUE_LIT 39   /* Totoo            */
#define FALSE_LIT 40  /* Palso            */
#define FLOAT_LIT 41  /* 10.5             */
#define STRING_LIT 42 /* "text" or 'text' */
/* [EXPR] Index brackets, e.g. "computer"[0] */
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
#define TOKEN_NEWLINE 61
#define TOKEN_INDENT 62
#define TOKEN_DEDENT 63
#define TOKEN_ERROR 64
#define MAX_INDENT_DEPTH 100
#define FIXED_FIELD_LABEL 65
#define FIXED_FIELD_CONTINUATION 66
#define FIXED_FIELD_STATEMENT 67
#define FIXED_FIELD_SEQUENCE 68
#define MAX_FIXED_FIELDS 16
#define MAX_FIXED_COLUMNS 256
#define MAX_FIXED_TOKENS 256
#define MAX_FIXED_LINE_LENGTH 4096

typedef struct
{
    const char *name;
    int startColumn;
    int endColumn;
    int tokenCode;
} FixedFieldSpec;

/* Default FORTRAN-style layout; edit these 1-based inclusive columns for your format. */
FixedFieldSpec fixedFieldLayout[] = {
    {"label", 1, 5, FIXED_FIELD_LABEL},
    {"continuation", 6, 6, FIXED_FIELD_CONTINUATION},
    {"statement", 7, 72, FIXED_FIELD_STATEMENT},
    {"sequence", 73, 80, FIXED_FIELD_SEQUENCE}};
int fixedFieldCount = sizeof fixedFieldLayout / sizeof fixedFieldLayout[0];

/* Set to 1 to use column-based fields; 0 preserves the original Tagpy lexer. */
int fixed_field_mode = 0;

typedef struct
{
    int tokenCode;
    int line;
    int column;
    const char *fieldName;
    char value[MAX_FIXED_COLUMNS + 1];
} FixedFieldToken;

static FixedFieldToken fixedTokens[MAX_FIXED_TOKENS];
static int fixedTokenCount = 0;
static int fixedTokenIndex = 0;
static int fixedStatementToken = -1;
static int fixedLineNumber = 0;
static int fixedInputEnded = 0;
static int fixedLookaheadValid = 0;
static char fixedLookahead[MAX_FIXED_LINE_LENGTH];
static int fixedLookaheadLine = 0;
/******************************************************/
/* main driver */
int main()
{
    /* Open the input data file and process its contents */
    if ((in_fp = fopen("front.in", "r")) == NULL)
        printf("ERROR - cannot open front.in \n");
    else
    {
        if (!fixed_field_mode)
            getChar();
        do
        {
            lex();
        } while (nextToken != EOF);
    }
    return 0;
}
/*****************************************************/
/* peekChar - look at the next character in the input
 without consuming it (needed for two-character operators) */
int peekChar()
{
    int c = getc(in_fp);
    if (c != EOF)
        ungetc(c, in_fp);
    return c;
}
/*****************************************************/
/* lookup - a function to lookup operators and parentheses
 and return the token */
 /*can be used for any symbol case or repeating symbol lexemes -> operators, etc.*/
int lookup(char ch)
{
    switch (ch)
    {
    case '(':
        addChar();
        nextToken = LEFT_PAREN;
        break;
    case ')':
        addChar();
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
            nextToken = EOF; /* a lone '!' is not a valid token */
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
    case ':': /* [STMT] */
        addChar();
        nextToken = COLON;
        break;
    case ',': /* [STMT] */
        addChar();
        nextToken = COMMA;
        break;
    case '[': /* [EXPR] */
        addChar();
        nextToken = LEFT_BRACKET;
        break;
    case ']': /* [EXPR] */
        addChar();
        nextToken = RIGHT_BRACKET;
        break;
    case '"': 
    case '\'':
        lexeme[0] = 0; 
        getChar(); 
        while (charClass != EOF && nextChar != ch && nextChar != '\n')
        {
            addChar();
            getChar();
        }
        if (nextChar == ch) /* closing quote found */
            nextToken = STRING_LIT;
        else
        {
            printf("Error - unterminated string \n");
            nextToken = EOF; /* not a valid token, same as a lone '!' */
        }
        break;
    default:
        addChar();
        nextToken = EOF;
        break;
    }
    return nextToken;
}
/*****************************************************/
/* addChar - a function to add nextChar to lexeme */
void addChar()
{
    if (lexLen <= 98)
    {
        lexeme[lexLen++] = nextChar;
        lexeme[lexLen] = 0;
    }
    else
        printf("Error - lexeme is too long \n");
}
/*****************************************************/
/* getChar - a function to get the next character of
 input and determine its character class */
void getChar()
{
    if ((nextChar = getc(in_fp)) != EOF)
    {
        if (isalpha(nextChar))
            charClass = LETTER;
        else if (isdigit(nextChar))
            charClass = DIGIT;
        else
            charClass = UNKNOWN;
    }
    else
        charClass = EOF;
}
/*****************************************************/
/* getNonBlank - a function to call getChar until it
 returns a non-whitespace character

 Used for returning strings as non-whitespace characters if inside a comment (# / ##)*/
void getNonBlank() {
    while (isspace(nextChar) || nextChar == '#'){
        if (isspace(nextChar)){
            getChar();
        }else if (nextChar == '#'){
            if (peekChar() == '#'){
                getChar();
                getChar();
                while (nextChar != EOF){
                    if (nextChar == '#' && peekChar() == '#'){
                        getChar(); getChar();
                        break;
                    }
                    getChar();
                }
            }else{
                while (nextChar != EOF && nextChar != '\n'){
                    getChar();
                }
            }
        }
    }
}

/*****************************************************/
/* lex - a simple lexical analyzer for arithmetic
 expressions */
static int fixed_layout_validated = 0;
static int fixed_layout_invalid = 0;
static int fixedStatementFieldIndex = -1;
static int fixedContinuationFieldIndex = -1;

static int validate_fixed_layout(void)
{
    int i, j;

    if (fixedFieldCount < 1 || fixedFieldCount > MAX_FIXED_FIELDS)
        return 0;

    for (i = 0; i < fixedFieldCount; i++)
    {
        if (fixedFieldLayout[i].name == NULL ||
            fixedFieldLayout[i].startColumn < 1 ||
            fixedFieldLayout[i].endColumn < fixedFieldLayout[i].startColumn ||
            fixedFieldLayout[i].endColumn > MAX_FIXED_COLUMNS)
            return 0;

        for (j = 0; j < i; j++)
        {
            if (fixedFieldLayout[i].startColumn <= fixedFieldLayout[j].endColumn &&
                fixedFieldLayout[j].startColumn <= fixedFieldLayout[i].endColumn)
                return 0;
        }
    }
    return 1;
}

static int fixed_error(const char *message, int line, int column)
{
    nextToken = TOKEN_ERROR;
    tokenLine = line;
    tokenColumn = column;
    tokenFieldName = "error";
    snprintf(lexeme, sizeof lexeme, "%s", message);
    printf("Error at line %d, column %d: %s\n", line, column, message);
    return nextToken;
}

static int read_fixed_line(char *line, size_t capacity, int *lineNumber)
{
    char raw[MAX_FIXED_LINE_LENGTH];
    size_t i, expandedLength = 0;
    int c;

    if (fixedLookaheadValid)
    {
        snprintf(line, capacity, "%s", fixedLookahead);
        *lineNumber = fixedLookaheadLine;
        fixedLookaheadValid = 0;
        return 1;
    }

    if (fixedInputEnded || fgets(raw, sizeof raw, in_fp) == NULL)
    {
        fixedInputEnded = 1;
        return 0;
    }

    fixedLineNumber++;
    *lineNumber = fixedLineNumber;
    i = strlen(raw);
    if (i > 0 && raw[i - 1] == '\n')
        raw[--i] = '\0';
    if (i > 0 && raw[i - 1] == '\r')
        raw[--i] = '\0';

    if (i == sizeof raw - 1 && !feof(in_fp) && raw[i - 1] != '\n')
    {
        c = getc(in_fp);
        if (c != EOF && c != '\n')
        {
            while ((c = getc(in_fp)) != EOF && c != '\n')
                ;
            return -1;
        }
    }

    for (i = 0; raw[i] != '\0'; i++)
    {
        if (raw[i] == '\t')
        {
            size_t spaces = 8 - (expandedLength % 8);
            while (spaces-- > 0 && expandedLength + 1 < capacity)
                line[expandedLength++] = ' ';
        }
        else if (expandedLength + 1 < capacity)
            line[expandedLength++] = raw[i];
    }
    line[expandedLength] = '\0';
    return 1;
}

static void extract_fixed_field(const char *line, const FixedFieldSpec *field,
                                char *value, size_t capacity)
{
    size_t start = (size_t)field->startColumn - 1;
    size_t end = (size_t)field->endColumn;
    size_t lineLength = strlen(line);
    size_t valueLength, first = 0, last;

    if (start >= lineLength)
    {
        value[0] = '\0';
        return;
    }
    if (end > lineLength)
        end = lineLength;
    valueLength = end - start;
    if (valueLength >= capacity)
        valueLength = capacity - 1;
    memcpy(value, line + start, valueLength);
    value[valueLength] = '\0';

    last = valueLength;
    while (first < last && isspace((unsigned char)value[first]))
        first++;
    while (last > first && isspace((unsigned char)value[last - 1]))
        last--;
    if (first > 0)
        memmove(value, value + first, last - first);
    value[last - first] = '\0';
}

static int enqueue_fixed_field(int fieldIndex, const char *value, int line)
{
    FixedFieldToken *item;

    if (fixedTokenCount >= MAX_FIXED_TOKENS)
        return 0;

    item = &fixedTokens[fixedTokenCount];
    item->tokenCode = fixedFieldLayout[fieldIndex].tokenCode;
    item->line = line;
    item->column = fixedFieldLayout[fieldIndex].startColumn;
    item->fieldName = fixedFieldLayout[fieldIndex].name;
    snprintf(item->value, sizeof item->value, "%s", value);
    if (strcmp(item->fieldName, "statement") == 0)
        fixedStatementToken = fixedTokenCount;
    fixedTokenCount++;
    return 1;
}

static int return_fixed_token(void)
{
    FixedFieldToken *item = &fixedTokens[fixedTokenIndex++];

    nextToken = item->tokenCode;
    tokenLine = item->line;
    tokenColumn = item->column;
    tokenFieldName = item->fieldName;
    snprintf(lexeme, sizeof lexeme, "%s", item->value);
    printf("Next token is: %d, field %s at line %d, column %d: %s\n",
           nextToken, tokenFieldName, tokenLine, tokenColumn, lexeme);

    if (fixedTokenIndex == fixedTokenCount)
    {
        fixedTokenIndex = 0;
        fixedTokenCount = 0;
        fixedStatementToken = -1;
    }
    return nextToken;
}

static int lex_fixed_field(void)
{
    char line[MAX_FIXED_LINE_LENGTH];
    char values[MAX_FIXED_FIELDS][MAX_FIXED_COLUMNS + 1];
    int lineNumber, readStatus, i;

    if (!fixed_layout_validated)
    {
        fixed_layout_validated = 1;
        if (!validate_fixed_layout())
        {
            fixed_layout_invalid = 1;
            return fixed_error("invalid fixed-field layout", 1, 1);
        }
        for (i = 0; i < fixedFieldCount; i++)
        {
            if (strcmp(fixedFieldLayout[i].name, "statement") == 0)
                fixedStatementFieldIndex = i;
            if (strcmp(fixedFieldLayout[i].name, "continuation") == 0)
                fixedContinuationFieldIndex = i;
        }
    }
    else if (fixed_layout_invalid)
    {
        nextToken = EOF;
        tokenLine = 1;
        tokenColumn = 1;
        tokenFieldName = NULL;
        strcpy(lexeme, "EOF");
        return nextToken;
    }

    for (;;)
    {
        int length, isBlank = 1, isComment, isContinuation;

        readStatus = read_fixed_line(line, sizeof line, &lineNumber);
        if (readStatus < 0)
        {
            fixedTokenCount = fixedTokenIndex = 0;
            fixedStatementToken = -1;
            return fixed_error("physical line exceeds the input buffer", lineNumber, MAX_FIXED_LINE_LENGTH);
        }
        if (readStatus == 0)
        {
            if (fixedTokenCount > 0)
                return return_fixed_token();
            nextToken = EOF;
            tokenLine = fixedLineNumber + 1;
            tokenColumn = 1;
            tokenFieldName = NULL;
            strcpy(lexeme, "EOF");
            printf("Next token is: %d, Next lexeme is %s\n", nextToken, lexeme);
            return nextToken;
        }

        length = (int)strlen(line);
        for (i = 0; i < length; i++)
        {
            if (!isspace((unsigned char)line[i]))
            {
                isBlank = 0;
                break;
            }
        }
        if (isBlank)
            continue;

        isComment = line[0] == 'C' || line[0] == 'c' || line[0] == '*' ||
                    line[0] == '!' || line[0] == '#';
        if (isComment)
            continue;

        for (i = 0; i < fixedFieldCount; i++)
            extract_fixed_field(line, &fixedFieldLayout[i], values[i], sizeof values[i]);

        isContinuation = fixedContinuationFieldIndex >= 0 &&
                         values[fixedContinuationFieldIndex][0] != '\0' &&
                         values[fixedContinuationFieldIndex][0] != '0';
        if (isContinuation)
        {
            size_t currentLength, appendLength;
            FixedFieldToken *statement;

            if (fixedStatementToken < 0 || fixedStatementFieldIndex < 0)
            {
                fixedTokenCount = fixedTokenIndex = 0;
                fixedStatementToken = -1;
                return fixed_error("continuation line has no preceding statement",
                                   lineNumber, fixedFieldLayout[fixedContinuationFieldIndex].startColumn);
            }

            statement = &fixedTokens[fixedStatementToken];
            currentLength = strlen(statement->value);
            appendLength = strlen(values[fixedStatementFieldIndex]);
            if (currentLength + appendLength >= sizeof statement->value)
            {
                fixedTokenCount = fixedTokenIndex = 0;
                fixedStatementToken = -1;
                return fixed_error("continued statement exceeds the field buffer",
                                   lineNumber, fixedFieldLayout[fixedStatementFieldIndex].startColumn);
            }
            memcpy(statement->value + currentLength, values[fixedStatementFieldIndex], appendLength + 1);

            for (i = 0; i < fixedFieldCount; i++)
            {
                if (i != fixedStatementFieldIndex && !enqueue_fixed_field(i, values[i], lineNumber))
                {
                    fixedTokenCount = fixedTokenIndex = 0;
                    fixedStatementToken = -1;
                    return fixed_error("too many fields in a continued statement",
                                       lineNumber, fixedFieldLayout[i].startColumn);
                }
            }
            continue;
        }

        if (fixedTokenCount > 0)
        {
            snprintf(fixedLookahead, sizeof fixedLookahead, "%s", line);
            fixedLookaheadLine = lineNumber;
            fixedLookaheadValid = 1;
            return return_fixed_token();
        }

        fixedStatementToken = -1;
        for (i = 0; i < fixedFieldCount; i++)
        {
            if (!enqueue_fixed_field(i, values[i], lineNumber))
            {
                fixedTokenCount = fixedTokenIndex = 0;
                fixedStatementToken = -1;
                return fixed_error("too many fields in a record",
                                   lineNumber, fixedFieldLayout[i].startColumn);
            }
        }
    }
}

int lex()
{
    if (fixed_field_mode)
        return lex_fixed_field();

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
                    if (nextChar == '\n')
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

        if (isspace((unsigned char)nextChar))
        {
            if (nextChar == '\n')
            {
                getChar();
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
        /* Parse identifiers  -> use this for keywords, noise words, etc.*/
    case LETTER:
        nextToken = keyword_lookup();
        break;
        /* Parse integer literals */
    case DIGIT:
        addChar();
        getChar();
        while (charClass == DIGIT)
        {
            addChar();
            getChar();
        }
        if (nextChar == '.' && isdigit(peekChar())) /* [EXPR] float literal */
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
        /* Parentheses and operators */
    case UNKNOWN:
        lookup(nextChar);
        getChar();
        break;
        /* EOF */
    case EOF:
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
    printf("Next token is: %d, Next lexeme is %s\n",
           nextToken, lexeme);
    return nextToken;
} /* End of function lex */

/* keyword_lookup - a function to lookup keywords,
 noise words, and identifiers using a DFA state machine */
int keyword_lookup()
{
    int state = 0; /* State 0: Start state*/
    
    addChar();

    switch (nextChar) {
        case 'a': 
            state = 1; 
            break;
        case 'o': /* [EXPR] 'o' */
            state = 6;
            break;
        case 'h': /* [EXPR] start of 'hindi' */
            state = 7;
            break;
        case 'T': /* [EXPR] start of 'Totoo' */
            state = 12;
            break;
        case 'P': /* [EXPR] start of 'Palso' */
            state = 17;
            break;
        case 'b': /* [STMT] start of 'b...' */
            state = 22;
            break;
        case 'k': /* [STMT] start of 'k...' */
            state = 29;
            break;
        case 'l': /* [STMT] start of 'l...' */
            state = 43;
            break;
        case 'n': /* [STMT] start of 'n...' */
            state = 48;
            break;
        case 'p': /* [STMT] start of 'p...' */
            state = 51;
            break;
        case 's': /* [STMT] start of 's...' */
            state = 55;
            break;
        case 't': /* [STMT] start of 't...' */
            state = 63;
            break;
        default:  
            state = 99; //sink state for identifiers
            break;
    }
    
    getChar();
    
    /* Consume characters and transition states */
    while (charClass == LETTER || charClass == DIGIT || nextChar == '_'){
        addChar();
        
        switch (state){
            case 1:
                if (nextChar == 'y') state = 2; //'ay'
                else if (nextChar == 'n') state = 3; // 'an'
                else if (nextChar == 't') state = 5; // [EXPR] 'at'
                else state = 99;
                break;
            
            case 3: // 'an'
                if (nextChar == 'g') state = 4; //'ang'
                else state = 99;
                break;
            
            /* [EXPR] states 7-11: h-i-n-d-i, state 11 = 'hindi' */
            case 7:
                if (nextChar == 'i') state = 8;
                else state = 99;
                break;
            case 8:
                if (nextChar == 'n') state = 9;
                else state = 99;
                break;
            case 9:
                if (nextChar == 'd') state = 10;
                else state = 99;
                break;
            case 10:
                if (nextChar == 'i') state = 11;
                else state = 99;
                break;

            /* [EXPR] states 12-16: T-o-t-o-o, state 16 = 'Totoo' */
            case 12:
                if (nextChar == 'o') state = 13;
                else state = 99;
                break;
            case 13:
                if (nextChar == 't') state = 14;
                else state = 99;
                break;
            case 14:
                if (nextChar == 'o') state = 15;
                else state = 99;
                break;
            case 15:
                if (nextChar == 'o') state = 16;
                else state = 99;
                break;

            /* [EXPR] states 17-21: P-a-l-s-o, state 21 = 'Palso' */
            case 17:
                if (nextChar == 'a') state = 18;
                else state = 99;
                break;
            case 18:
                if (nextChar == 'l') state = 19;
                else state = 99;
                break;
            case 19:
                if (nextChar == 's') state = 20;
                else state = 99;
                break;
            case 20:
                if (nextChar == 'o') state = 21;
                else state = 99;
                break;

            /* [STMT] state 22 = 'b' */
            case 22:
                if (nextChar == 'a') state = 23;
                else state = 99;
                break;

            /* [STMT] state 23 = 'ba' */
            case 23:
                if (nextChar == 'k') state = 24;
                else if (nextChar == 'w') state = 26;
                else state = 99;
                break;

            /* [STMT] state 24 = 'bak' */
            case 24:
                if (nextChar == 'a') state = 25;
                else state = 99;
                break;

            /* [STMT] state 26 = 'baw' */
            case 26:
                if (nextChar == 'a') state = 27;
                else state = 99;
                break;

            /* [STMT] state 27 = 'bawa' */
            case 27:
                if (nextChar == 't') state = 28;
                else state = 99;
                break;

            /* [STMT] state 29 = 'k' */
            case 29:
                if (nextChar == 'a') state = 30;
                else if (nextChar == 'u') state = 32;
                else state = 99;
                break;

            /* [STMT] state 30 = 'ka' */
            case 30:
                if (nextChar == 'r') state = 31;
                else state = 99;
                break;

            /* [STMT] state 32 = 'ku' */
            case 32:
                if (nextChar == 'n') state = 33;
                else state = 99;
                break;

            /* [STMT] state 33 = 'kun' */
            case 33:
                if (nextChar == 'd') state = 34;
                else if (nextChar == 'g') state = 40;
                else if (nextChar == 'i') state = 41;
                else state = 99;
                break;

            /* [STMT] state 34 = 'kund' */
            case 34:
                if (nextChar == 'i') state = 35;
                else state = 99;
                break;

            /* [STMT] state 35 = 'kundi' */
            case 35:
                if (nextChar == 'k') state = 36;
                else state = 99;
                break;

            /* [STMT] state 36 = 'kundik' */
            case 36:
                if (nextChar == 'u') state = 37;
                else state = 99;
                break;

            /* [STMT] state 37 = 'kundiku' */
            case 37:
                if (nextChar == 'n') state = 38;
                else state = 99;
                break;

            /* [STMT] state 38 = 'kundikun' */
            case 38:
                if (nextChar == 'g') state = 39;
                else state = 99;
                break;

            /* [STMT] state 41 = 'kuni' */
            case 41:
                if (nextChar == 'n') state = 42;
                else state = 99;
                break;

            /* [STMT] state 43 = 'l' */
            case 43:
                if (nextChar == 'i') state = 44;
                else state = 99;
                break;

            /* [STMT] state 44 = 'li' */
            case 44:
                if (nextChar == 's') state = 45;
                else state = 99;
                break;

            /* [STMT] state 45 = 'lis' */
            case 45:
                if (nextChar == 't') state = 46;
                else state = 99;
                break;

            /* [STMT] state 46 = 'list' */
            case 46:
                if (nextChar == 'a') state = 47;
                else state = 99;
                break;

            /* [STMT] state 48 = 'n' */
            case 48:
                if (nextChar == 'u') state = 49;
                else state = 99;
                break;

            /* [STMT] state 49 = 'nu' */
            case 49:
                if (nextChar == 'm') state = 50;
                else state = 99;
                break;

            /* [STMT] state 51 = 'p' */
            case 51:
                if (nextChar == 'i') state = 52;
                else state = 99;
                break;

            /* [STMT] state 52 = 'pi' */
            case 52:
                if (nextChar == 's') state = 53;
                else state = 99;
                break;

            /* [STMT] state 53 = 'pis' */
            case 53:
                if (nextChar == 'i') state = 54;
                else state = 99;
                break;

            /* [STMT] state 55 = 's' */
            case 55:
                if (nextChar == 'a') state = 56;
                else if (nextChar == 'u') state = 57;
                else state = 99;
                break;

            /* [STMT] state 57 = 'su' */
            case 57:
                if (nextChar == 'l') state = 58;
                else state = 99;
                break;

            /* [STMT] state 58 = 'sul' */
            case 58:
                if (nextChar == 'a') state = 59;
                else state = 99;
                break;

            /* [STMT] state 59 = 'sula' */
            case 59:
                if (nextChar == 't') state = 60;
                else state = 99;
                break;

            /* [STMT] state 60 = 'sulat' */
            case 60:
                if (nextChar == 'l') state = 61;
                else state = 99;
                break;

            /* [STMT] state 61 = 'sulatl' */
            case 61:
                if (nextChar == 'n') state = 62;
                else state = 99;
                break;

            /* [STMT] state 63 = 't' */
            case 63:
                if (nextChar == 'u') state = 64;
                else state = 99;
                break;

            /* [STMT] state 64 = 'tu' */
            case 64:
                if (nextChar == 'l') state = 65;
                else state = 99;
                break;

            default: //sink state
                state = 99; 
                break;
        }
        
        getChar();
    }
    
    /* Return the correct token based on the final state reached */
    /* [EXPR] accepting states of the expression words */
    if (state == 5)
        return AT_OP;     /* at    */
    if (state == 6)
        return O_OP;      /* o     */
    if (state == 11)
        return HINDI_OP;  /* hindi */
    if (state == 16)
        return TRUE_LIT;  /* Totoo */
    if (state == 21)
        return FALSE_LIT; /* Palso */
    /* [STMT] accepting states of the statement keywords */
    if (state == 25)
        return TYPE_BAKA; /* baka */
    if (state == 28)
        return KW_BAWAT; /* bawat */
    if (state == 31)
        return TYPE_KAR; /* kar */
    if (state == 35)
        return KW_KUNDI; /* kundi */
    if (state == 39)
        return KW_KUNDIKUNG; /* kundikung */
    if (state == 40)
        return KW_KUNG; /* kung */
    if (state == 42)
        return IO_KUNIN; /* kunin */
    if (state == 47)
        return TYPE_LISTA; /* lista */
    if (state == 50)
        return TYPE_NUM; /* num */
    if (state == 54)
        return TYPE_PISI; /* pisi */
    if (state == 56)
        return KW_SA; /* sa */
    if (state == 60)
        return IO_SULAT; /* sulat */
    if (state == 62)
        return IO_SULATLN; /* sulatln */
    if (state == 65)
        return TYPE_TUL; /* tul */
    if (state == 2 || state == 4) {
        return NOISE_WORD;
    } else {
        return IDENT;
    }
}

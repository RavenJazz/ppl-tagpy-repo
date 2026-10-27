/* front.c - a lexical analyzer system for simple
 arithmetic and boolean expressions */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
/* Global declarations */
/* Variables */
int charClass;
char lexeme[100];
char nextChar;
int lexLen;
int token;
int nextToken;
FILE *in_fp, *fopen(const char *filename, const char *mode);
/* Function declarations */
void addChar();
void getChar();
void getNonBlank();
int peekChar();
int lex();
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
/******************************************************/
/* main driver */
int main()
{
    /* Open the input data file and process its contents */
    if ((in_fp = fopen("front.in", "r")) == NULL)
        printf("ERROR - cannot open front.in \n");
    else
    {
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
int lex()
{
    lexLen = 0;
    getNonBlank();
    switch (charClass)
    {
        /* Parse identifiers  -> use this for keywords, noise words, etc.*/
    case LETTER:
        addChar();
        getChar();
        while (charClass == LETTER || charClass == DIGIT || nextChar == '_')
        {
            addChar();
            getChar();
        }
        //Noise words
        if (strcmp(lexeme, "ay") == 0 || strcmp(lexeme, "ang") == 0){
            nextToken = NOISE_WORD;
        }else{
        //identifiers are the last option if something is not a valid token
        nextToken = IDENT;
        }
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
        nextToken = INT_LIT;
        break;
        /* Parentheses and operators */
    case UNKNOWN:
        lookup(nextChar);
        getChar();
        break;
        /* EOF */
    case EOF:
        nextToken = EOF;
        lexeme[0] = 'E';
        lexeme[1] = 'O';
        lexeme[2] = 'F';
        lexeme[3] = 0;
        break;
    } /* End of switch */
    printf("Next token is: %d, Next lexeme is %s\n",
           nextToken, lexeme);
    return nextToken;
} /* End of function lex */

#include <stdio.h>
#include <ctype.h>
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
int keyword_lookup();
void pushBracket(char open);
void popBracket(char close);
void checkUnclosed();
/* Bracket matching and line tracking */
#define MAX_BRACKET_DEPTH 100
char bracketStack[MAX_BRACKET_DEPTH];
int bracketLine[MAX_BRACKET_DEPTH];
int bracketTop= 0;
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
/* [DELIM] Brace token for the mapped collection (dictionaries and sets) */
#define LEFT_BRACE 61 /* { */
#define RIGHT_BRACE 62 /* } */
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
        checkUnclosed(); /* report any bracket that never closed */
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
        pushBracket('[');
        nextToken = LEFT_BRACKET;
        break;
    case ']': /* [EXPR] */
        addChar();
        popBracket(']');
        nextToken = RIGHT_BRACKET;
        break;
    case '{': /* [DELIM] */
        addChar();
        pushBracket ('{');
        nextToken = LEFT_BRACE;
        break;
    case '}': /* [DELIM] */
        addChar();
        popBracket ('}');
        nextToken = RIGHT_BRACE;
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
/* pushBracket - remember an opening bracket ( [ { on the stack,
   along with where the line is found on */
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
/* popBracket - check that a closing bracket ) ] } matches the 
   most recent open bracket. And reports an error if there is 
   none or if its the wrong kind */
void popBracket(char close)
{
    char open;

    if (bracketTop == 0)
    {
        printf("Error - '%c' on line %d has no matching opening bracket \n", 
              close, lineNo);
        return ;
    }
    bracketTop--;
    open = bracketStack[bracketTop];
    if((open == '(' && close != ')') ||
       (open == '[' && close != ']') ||
       (open == '{' && close != '}'))
        printf("Error - '%c' on line %d does not match '%c' from line %d \n", 
              close, lineNo, open, bracketLine[bracketTop]);
}
/*****************************************************/
/* checkUnclosed - called at the end of input; any opening 
   bracket still on the stack was never closed */
void checkUnclosed()
{
    while (bracketTop > 0)
    {
        bracketTop--;
        printf("Error - '%c' from line %d is never closed \n", bracketStack[bracketTop], bracketLine[bracketTop]);
    }
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

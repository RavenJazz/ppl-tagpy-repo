/*
 * Tagpy Syntax Analyzer  (COSC 303 - Principles of Programming Languages)
 *
 * Second phase of the Tagpy translator.  Based on the recursive-descent
 * parser in Sebesta, "Concepts of Programming Languages": every
 * non-terminal of the group's EBNF grammar is one C function, and the
 * parser is driven by lex() and the global nextToken exactly like the
 * book's front.c.
 *
 * INPUT   The Symbol Table written by the lexical analyzer (front.c).
 *         lex() reads ONE ROW of "Symbol Table.txt" per call, so the
 *         parser receives its input one token at a time.
 *         Comments and noise words (ang, ay) are discarded by lex(),
 *         because the grammar says they are "discarded before parsing".
 *
 * OUTPUT  "Parsing Table.txt" - the parsing table of this run: one row per
 *         parsing step (STEP, LINE, NON-TERMINAL, LOOKAHEAD token, LEXEME,
 *         ACTION). ACTION is the rule applied, the alternative selected,
 *         the token matched, an error, or a panic-mode discard. The syntax
 *         errors and a summary are listed at the end of the file.
 *         The screen shows the Enter/Exit trace (or only errors with -q).
 *
 * ALGORITHM      Recursive Descent Parsing (top-down, one token of
 *                lookahead).  Where two rules start with the same token
 *                they were left-factored (see simple_stmt, declaration_stmt,
 *                identifier_stmt, primary_atom, dict_or_set_literal).
 *
 * ERROR RECOVERY Panic mode.  error() prints a message and jumps (longjmp)
 *                back to the closest guarded() call.  synchronize() then
 *                throws tokens away until the end of the bad statement
 *                (NEWLINE / DEDENT), skips an indented block that belonged
 *                to the bad line, and parsing continues with the next
 *                statement, so several errors can be reported in one run.
 *
 * Build:  gcc syntax_analyzer.c -o syntax_analyzer
 * Run:    ./front sample.tagpy            (lexer -> "Symbol Table.txt")
 *         ./syntax_analyzer               (parser -> "Parsing Table.txt")
 *         ./run.sh sample.tagpy           (or run.bat) all of the above
 *         ./syntax_analyzer -q            (quiet: only errors + summary)
 *         ./syntax_analyzer other.txt     (read a different symbol table)
 *
 * PARSING TABLE (which rule is chosen for the lookahead token nextToken)
 *   statement      KW_KUNG KW_BAWAT KW_HABANG KW_ITUKOY KW_SURI -> compound_stmt
 *                  type | IDENTIFIER | RW_SULAT(LN) | KW_IBALIK  -> simple_stmt NEWLINE
 *   simple_stmt    type -> declaration_stmt   IDENTIFIER -> identifier_stmt
 *                  RW_SULAT, RW_SULATLN -> output_stmt   KW_IBALIK -> return_stmt
 *   identifier_stmt  ASSIGN -> assignment_stmt   LPAREN -> call_stmt
 *   declaration_stmt ASSIGN then RW_KUNIN -> input_stmt, otherwise expression
 *   compound_stmt  KW_KUNG -> conditional_stmt   KW_BAWAT, KW_HABANG -> iterative_stmt
 *                  KW_ITUKOY -> function_def     KW_SURI -> match_stmt
 *   (the complete table is in syntax_analyzer_explanation.txt)
 */
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#define SYMBOL_TABLE_FILE "Symbol Table.txt"
#define PARSING_TABLE_FILE "Parsing Table.txt"
#define TYPE_COLUMN_WIDTH 24 /* the TOKEN TYPE column written by front.c */
#define ROW_MAX 4300         /* front.c lexemes are at most 4096 chars   */
#define MAX_RULE_DEPTH 512   /* rule names remembered for the table      */
#define MAX_LOGGED_ERRORS 200
#define ERR_LEN 300

/* Token codes - the same numbers and names as front.c */
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
#define EXP_OP 27
#define MOD_OP 28
#define FLOOR_DIV_OP 29
#define EQ_OP 30
#define NEQ_OP 31
#define LT_OP 32
#define GT_OP 33
#define LE_OP 34
#define GE_OP 35
#define AT_OP 36    /* at    (AND) */
#define O_OP 37     /* o     (OR)  */
#define HINDI_OP 38 /* hindi (NOT) */
#define TRUE_LIT 39
#define FALSE_LIT 40
#define FLOAT_LIT 41
#define STRING_LIT 42
#define LEFT_BRACKET 43
#define RIGHT_BRACKET 44
#define KW_KUNG 45
#define KW_KUNDIKUNG 46
#define KW_KUNDI 47
#define KW_BAWAT 48
#define KW_SA 49
#define TYPE_NUM 50
#define TYPE_TUL 51
#define TYPE_BAKA 52
#define TYPE_KAR 53
#define TYPE_PISI 54
#define TYPE_LISTA 55
#define IO_SULAT 56
#define IO_SULATLN 57
#define IO_KUNIN 58
#define COLON 59
#define COMMA 60
#define TOKEN_NEWLINE 61
#define TOKEN_INDENT 62
#define TOKEN_DEDENT 63
#define TOKEN_ERROR 64 /* INVALID */
#define LEFT_BRACE 65
#define RIGHT_BRACE 66
#define KW_ITUKOY 67
#define KW_IBALIK 68
#define KW_SURI 69
#define KW_SAKALI 70
#define SEMICOLON 71
#define DOT 72
#define LINE_COMMENT 73
#define BLOCK_COMMENT 74
#define KW_HABANG 75

/* Global declarations */
int nextToken;            /* the lookahead token (EOF at the end)         */
char lexeme[ROW_MAX];     /* its lexeme                                    */
int tokenNo = 0;          /* how many tokens the parser has received       */
int codeLine = 1;         /* n-th line that contains code (see lex)        */
int errorCount = 0;
int trace = 1;            /* 1 = print Enter/Exit lines like Sebesta       */
int depth = 0;            /* nesting depth, only used to indent the trace  */
FILE *in_fp;              /* the Symbol Table (input)                      */
FILE *table_fp;           /* the Parsing Table (output)                    */
int step = 0;             /* row number in the Parsing Table               */
int panicking = 0;        /* 1 while synchronize() is discarding tokens    */
const char *ruleStack[MAX_RULE_DEPTH]; /* rule being parsed at each depth  */
char errLog[MAX_LOGGED_ERRORS][ERR_LEN];
int loggedErrors = 0;
jmp_buf *handler = NULL;  /* where error() jumps: the closest guarded()    */
static char row[ROW_MAX]; /* one row of the Symbol Table                   */

/* The TOKEN TYPE names written by front.c's tokenName(), and their codes */
typedef struct
{
    const char *name;
    int code;
} TokenEntry;

static const TokenEntry tokenTable[] = {
    {"IDENTIFIER", IDENT},        {"NOISE_WORD", NOISE_WORD},
    {"INT_LITERAL", INT_LIT},     {"FLOAT_LITERAL", FLOAT_LIT},
    {"STRING_LITERAL", STRING_LIT},
    {"TRUE_LITERAL", TRUE_LIT},   {"FALSE_LITERAL", FALSE_LIT},
    {"ADD", ADD_OP},              {"SUBTRACT", SUB_OP},
    {"MULTIPLY", MULT_OP},        {"DIVIDE", DIV_OP},
    {"EXPONENT", EXP_OP},         {"MODULO", MOD_OP},
    {"FLOOR_DIVIDE", FLOOR_DIV_OP}, {"ASSIGN", ASSIGN_OP},
    {"EQUAL", EQ_OP},             {"NOT_EQUAL", NEQ_OP},
    {"LESS_THAN", LT_OP},         {"GREATER_THAN", GT_OP},
    {"LESS_EQUAL", LE_OP},        {"GREATER_EQUAL", GE_OP},
    {"AND", AT_OP},               {"OR", O_OP},
    {"NOT", HINDI_OP},
    {"KW_KUNG", KW_KUNG},         {"KW_KUNDIKUNG", KW_KUNDIKUNG},
    {"KW_KUNDI", KW_KUNDI},       {"KW_BAWAT", KW_BAWAT},
    {"KW_SA", KW_SA},             {"KW_ITUKOY", KW_ITUKOY},
    {"KW_IBALIK", KW_IBALIK},     {"KW_SURI", KW_SURI},
    {"KW_SAKALI", KW_SAKALI},     {"KW_HABANG", KW_HABANG},
    {"RW_NUM", TYPE_NUM},         {"RW_TUL", TYPE_TUL},
    {"RW_BAKA", TYPE_BAKA},       {"RW_KAR", TYPE_KAR},
    {"RW_PISI", TYPE_PISI},       {"RW_LISTA", TYPE_LISTA},
    {"RW_SULAT", IO_SULAT},       {"RW_SULATLN", IO_SULATLN},
    {"RW_KUNIN", IO_KUNIN},
    {"LPAREN", LEFT_PAREN},       {"RPAREN", RIGHT_PAREN},
    {"LBRACKET", LEFT_BRACKET},   {"RBRACKET", RIGHT_BRACKET},
    {"LBRACE", LEFT_BRACE},       {"RBRACE", RIGHT_BRACE},
    {"COLON", COLON},             {"COMMA", COMMA},
    {"SEMICOLON", SEMICOLON},     {"DOT", DOT},
    {"LINE_COMMENT", LINE_COMMENT}, {"BLOCK_COMMENT", BLOCK_COMMENT},
    {"NEWLINE", TOKEN_NEWLINE},   {"INDENT", TOKEN_INDENT},
    {"DEDENT", TOKEN_DEDENT},     {"INVALID", TOKEN_ERROR},
    {NULL, 0}};

/* Function declarations - one function per rule of the EBNF grammar */
int lex(void);
void report(const char *msg);
void choose(const char *production);
void error(const char *expected);
void errorMsg(const char *msg);
void expect(int tok, const char *what);
void guarded(void (*rule)(void));
void synchronize(void);
/* program structure */
void program(void);
void statement(void);
void simple_stmt(void);
void compound_stmt(void);
void block(void);
/* simple statements */
void declaration_stmt(void);
void input_stmt(void);
void identifier_stmt(void);
void assignment_stmt(void);
void call_stmt(void);
void output_stmt(void);
void return_stmt(void);
void type(void);
/* compound statements */
void conditional_stmt(void);
void if_clause(void);
void elif_clause(void);
void else_clause(void);
void iterative_stmt(void);
void for_stmt(void);
void while_stmt(void);
void function_def(void);
void parameter_list(void);
void parameter(void);
void match_stmt(void);
void case_block(void);
void case_clause(void);
void pattern(void);
/* expressions */
void expression(void);
void logical_expression(void);
void boolean_or_expr(void);
void boolean_and_expr(void);
void boolean_not_expr(void);
void relational_expression(void);
void arithmetic_expression(void);
void term(void);
void factor(void);
void power(void);
void primary_expression(void);
void primary_atom(void);
void function_call_rest(void);
void expression_list(void);
void collection_literal(void);
void list_literal(void);
void dict_or_set_literal(void);
void key_value_pair(void);

/******************************************************/
/* Small helpers (no string.h: strings are compared one char at a time) */

int sameText(const char *a, const char *b)
{
    while (*a && *b && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

const char *tokenName(int t)
{
    int i;

    if (t == EOF)
        return "EOF";
    for (i = 0; tokenTable[i].name != NULL; i++)
        if (tokenTable[i].code == t)
            return tokenTable[i].name;
    return "UNKNOWN";
}

int codeOf(const char *name)
{
    int i;

    for (i = 0; tokenTable[i].name != NULL; i++)
        if (sameText(tokenTable[i].name, name))
            return tokenTable[i].code;
    return TOKEN_ERROR; /* a name front.c never writes */
}

int isType(int t)
{
    return t == TYPE_NUM || t == TYPE_TUL || t == TYPE_BAKA ||
           t == TYPE_KAR || t == TYPE_PISI || t == TYPE_LISTA;
}

/* FIRST(expression) - used to decide whether an optional expression is there */
int startsExpression(int t)
{
    return t == HINDI_OP || t == ADD_OP || t == SUB_OP || t == INT_LIT ||
           t == FLOAT_LIT || t == TRUE_LIT || t == FALSE_LIT ||
           t == STRING_LIT || t == LEFT_BRACKET || t == LEFT_BRACE ||
           t == IDENT || t == LEFT_PAREN;
}

/* The right-hand side of every rule, shown in the ACTION column of the
   Parsing Table when the rule is applied */
typedef struct
{
    const char *rule;
    const char *rhs;
} Production;

static const Production productions[] = {
    {"program", "{ statement }"},
    {"statement", "simple_stmt NEWLINE | compound_stmt"},
    {"simple_stmt", "declaration_stmt | assignment_stmt | input_stmt | output_stmt | return_stmt | call_stmt"},
    {"compound_stmt", "conditional_stmt | iterative_stmt | function_def | match_stmt"},
    {"block", "COLON NEWLINE INDENT statement { statement } DEDENT"},
    {"type", "RW_NUM | RW_TUL | RW_BAKA | RW_KAR | RW_PISI | RW_LISTA"},
    {"declaration_stmt", "type IDENTIFIER [ ASSIGN expression ]"},
    {"input_stmt", "RW_KUNIN LPAREN [ expression ] RPAREN  (rest of: type IDENTIFIER ASSIGN ...)"},
    {"identifier_stmt", "IDENTIFIER ( assignment_stmt | call_stmt )"},
    {"assignment_stmt", "IDENTIFIER ASSIGN expression"},
    {"call_stmt", "function_call"},
    {"output_stmt", "( RW_SULAT | RW_SULATLN ) LPAREN expression RPAREN"},
    {"return_stmt", "KW_IBALIK [ expression ]"},
    {"conditional_stmt", "if_clause { elif_clause } [ else_clause ]"},
    {"if_clause", "KW_KUNG expression block"},
    {"elif_clause", "KW_KUNDIKUNG expression block"},
    {"else_clause", "KW_KUNDI block"},
    {"iterative_stmt", "for_stmt | while_stmt"},
    {"for_stmt", "KW_BAWAT IDENTIFIER KW_SA expression block"},
    {"while_stmt", "KW_HABANG expression block"},
    {"function_def", "KW_ITUKOY IDENTIFIER LPAREN [ parameter_list ] RPAREN block"},
    {"parameter_list", "parameter { COMMA parameter }"},
    {"parameter", "[ type ] IDENTIFIER"},
    {"match_stmt", "KW_SURI expression case_block"},
    {"case_block", "COLON NEWLINE INDENT case_clause { case_clause } DEDENT"},
    {"case_clause", "KW_SAKALI pattern block"},
    {"pattern", "INT_LITERAL | FLOAT_LITERAL | STRING_LITERAL | TRUE_LITERAL | FALSE_LITERAL | IDENTIFIER"},
    {"expression", "logical_expression"},
    {"logical_expression", "boolean_or_expr"},
    {"boolean_or_expr", "boolean_and_expr { OR boolean_and_expr }"},
    {"boolean_and_expr", "boolean_not_expr { AND boolean_not_expr }"},
    {"boolean_not_expr", "NOT boolean_not_expr | relational_expression"},
    {"relational_expression", "arithmetic_expression [ rel_op arithmetic_expression ]"},
    {"arithmetic_expression", "term { ( ADD | SUBTRACT ) term }"},
    {"term", "factor { ( MULTIPLY | DIVIDE | MODULO | FLOOR_DIVIDE ) factor }"},
    {"factor", "[ ADD | SUBTRACT ] power"},
    {"power", "primary_expression [ EXPONENT power ]"},
    {"primary_expression", "primary_atom { LBRACKET expression RBRACKET }"},
    {"primary_atom", "INT_LITERAL | FLOAT_LITERAL | TRUE_LITERAL | FALSE_LITERAL | STRING_LITERAL | collection_literal | function_call | IDENTIFIER | LPAREN expression RPAREN"},
    {"function_call", "IDENTIFIER LPAREN [ expression_list ] RPAREN"},
    {"expression_list", "expression { COMMA expression }"},
    {"collection_literal", "list_literal | dict_literal | set_literal"},
    {"list_literal", "LBRACKET [ expression_list ] RBRACKET"},
    {"dict_literal / set_literal", "LBRACE [ key_value_pair { COMMA key_value_pair } ] RBRACE | LBRACE expression_list RBRACE"},
    {"key_value_pair", "expression COLON expression"},
    {NULL, NULL}};

const char *productionOf(const char *rule)
{
    int i;

    for (i = 0; productions[i].rule != NULL; i++)
        if (sameText(productions[i].rule, rule))
            return productions[i].rhs;
    return "";
}

/* the rule that is being parsed right now */
const char *currentRule(void)
{
    int d = (depth > MAX_RULE_DEPTH) ? MAX_RULE_DEPTH : depth;

    return (d > 0) ? ruleStack[d - 1] : "(start)";
}

/* logRow - add one row to the Parsing Table file */
void logRow(const char *rule, int tok, const char *text, const char *action)
{
    if (table_fp == NULL)
        return;
    step++;
    fprintf(table_fp, "%-6d %-5d %-26s %-15s %-21.20s %s\n", step, codeLine,
            rule, tokenName(tok), text, action);
}

/* choose - record which alternative of a rule the lookahead token selected */
void choose(const char *production)
{
    char action[400];

    snprintf(action, sizeof action, "select: %s", production);
    logRow(currentRule(), nextToken, lexeme, action);
}

/* Enter / Exit trace lines (Sebesta prints these in every function) */
void enter(const char *rule)
{
    char action[800];

    if (trace)
        printf("%*sEnter <%s>\n", depth * 2, "", rule);
    snprintf(action, sizeof action, "apply: %s", productionOf(rule));
    logRow(rule, nextToken, lexeme, action);
    if (depth < MAX_RULE_DEPTH)
        ruleStack[depth] = rule;
    depth++;
}

void leave(const char *rule)
{
    depth--;
    if (trace)
        printf("%*sExit <%s>\n", depth * 2, "", rule);
}

/******************************************************/
/* readRow - read the next row of the Symbol Table into nextToken and
   lexeme.  Row layout (written by front.c):  %-24s TOKEN TYPE, then LEXEME.
   Returns 0 at the end of the file. */
int readRow(void)
{
    char name[TYPE_COLUMN_WIDTH + 1];
    int len, n, start, i;

    for (;;)
    {
        if (fgets(row, sizeof row, in_fp) == NULL)
            return 0;
        len = 0;
        while (row[len] != '\0' && row[len] != '\n' && row[len] != '\r')
            len++;
        if (row[len] == '\0') /* row longer than the buffer: drop the rest */
        {
            int c;
            while ((c = getc(in_fp)) != EOF && c != '\n')
                ;
        }
        row[len] = '\0';
        if (len > 0) /* skip empty rows */
            break;
    }

    n = 0;
    while (n < TYPE_COLUMN_WIDTH && row[n] != '\0' && row[n] != ' ')
    {
        name[n] = row[n];
        n++;
    }
    name[n] = '\0';

    start = (len > TYPE_COLUMN_WIDTH) ? TYPE_COLUMN_WIDTH : len;
    for (i = 0; row[start + i] != '\0'; i++)
        lexeme[i] = row[start + i];
    lexeme[i] = '\0';

    nextToken = codeOf(name);
    return 1;
}

/******************************************************/
/* lex - get the next token FROM THE SYMBOL TABLE, one token per call.
   Comments and noise words are skipped here ("discarded before parsing").
   codeLine goes up each time the parser moves past a NEWLINE token, so it
   counts lines that contain code (blank and comment-only lines are not
   in the table, so they are not counted). */
int lex(void)
{
    /* the current token is being consumed: one "match" row in the table */
    if (nextToken != 0 && nextToken != EOF)
        logRow(currentRule(), nextToken, lexeme,
               panicking ? "discard (panic mode)" : "match: token consumed");

    if (nextToken == TOKEN_NEWLINE)
        codeLine++;

    do
    {
        if (!readRow())
        {
            nextToken = EOF;
            lexeme[0] = '\0';
            if (trace)
                printf("%*sNext token is: EOF\n", depth * 2, "");
            return nextToken;
        }
    } while (nextToken == LINE_COMMENT || nextToken == BLOCK_COMMENT ||
             nextToken == NOISE_WORD);

    tokenNo++;
    if (trace)
        printf("%*sNext token is: %-14s Next lexeme is %s\n", depth * 2, "",
               tokenName(nextToken), lexeme);
    return nextToken;
}

/******************************************************/
/* Error messages and error recovery */

/* describe the token that was found instead of the expected one */
void describeFound(char *buf, size_t n)
{
    switch (nextToken)
    {
    case EOF:
        snprintf(buf, n, "the end of the file");
        break;
    case TOKEN_NEWLINE:
        snprintf(buf, n, "the end of the line");
        break;
    case TOKEN_INDENT:
        snprintf(buf, n, "an unexpected indentation");
        break;
    case TOKEN_DEDENT:
        snprintf(buf, n, "the end of an indented block");
        break;
    case TOKEN_ERROR:
        snprintf(buf, n, "an invalid token '%.60s' (rejected by the lexical analyzer)",
                 lexeme);
        break;
    default:
        snprintf(buf, n, "%s '%.60s'", tokenName(nextToken), lexeme);
        break;
    }
}

void jumpToRecovery(void)
{
    if (handler == NULL) /* cannot happen: program() guards every statement */
        exit(2);
    longjmp(*handler, 1);
}

/* report - print the message, add an ERROR row to the Parsing Table, and
   remember the message for the summary at the end of the table */
void report(const char *msg)
{
    char action[700];

    errorCount++;
    printf("Syntax error (code line %d, token #%d): %s\n", codeLine, tokenNo,
           msg);
    snprintf(action, sizeof action, "ERROR: %s", msg);
    logRow(currentRule(), nextToken, lexeme, action);
    if (loggedErrors < MAX_LOGGED_ERRORS)
        snprintf(errLog[loggedErrors++], ERR_LEN, "code line %d, token #%d: %s",
                 codeLine, tokenNo, msg);
}

/* error - "expected X, but found Y", then panic mode */
void error(const char *expected)
{
    char found[160], msg[500];

    describeFound(found, sizeof found);
    snprintf(msg, sizeof msg, "expected %s, but found %s", expected, found);
    report(msg);
    jumpToRecovery();
}

/* errorMsg - an error whose whole message is already known */
void errorMsg(const char *msg)
{
    report(msg);
    jumpToRecovery();
}

/* expect - the token must be tok: consume it, otherwise it is an error */
void expect(int tok, const char *what)
{
    if (nextToken == tok)
        lex();
    else
        error(what);
}

/* skipBlock - nextToken is INDENT: throw away everything up to and
   including the DEDENT that closes it (nested blocks are counted) */
void skipBlock(void)
{
    int level = 0;

    while (nextToken != EOF)
    {
        if (nextToken == TOKEN_INDENT)
            level++;
        else if (nextToken == TOKEN_DEDENT)
            level--;
        lex();
        if (level == 0)
            break;
    }
}

/* synchronize - PANIC MODE.  Discard tokens until the parser is back at
   the start of a statement.  Synchronizing tokens: NEWLINE (end of the bad
   line), DEDENT (end of the enclosing block) and EOF. */
void synchronize(void)
{
    if (trace)
        printf("%*s-- panic mode: skipping to the end of the statement --\n",
               depth * 2, "");
    logRow(currentRule(), nextToken, lexeme,
           "PANIC MODE: discard tokens up to the end of the statement");
    panicking = 1;

    if (nextToken == TOKEN_INDENT) /* unexpected indentation: skip its block */
        skipBlock();
    else
    {
        while (nextToken != EOF && nextToken != TOKEN_NEWLINE &&
               nextToken != TOKEN_DEDENT)
            lex();
        if (nextToken == TOKEN_NEWLINE)
            lex();
        if (nextToken == TOKEN_INDENT) /* the bad line owned an indented block */
            skipBlock();
    }
    panicking = 0;
}

/* guarded - run one grammar rule (a statement or a case clause).  If the
   rule hits a syntax error, error() longjmp()s back here and the parser
   recovers instead of stopping. */
void guarded(void (*rule)(void))
{
    jmp_buf here;
    jmp_buf *saved = handler;
    int savedDepth = depth;

    handler = &here;
    if (setjmp(here) == 0)
    {
        rule();
        handler = saved;
    }
    else
    {
        handler = saved;
        depth = savedDepth;
        synchronize();
    }
}

/******************************************************/
/* ===== PROGRAM STRUCTURE =====
   program ::= { statement }                                              */
void program(void)
{
    enter("program");
    while (nextToken != EOF)
    {
        if (nextToken == TOKEN_DEDENT) /* a DEDENT with no block to close */
        {
            report("indentation does not match any open block");
            lex();
            continue;
        }
        guarded(statement);
    }
    leave("program");
}

/* statement ::= simple_stmt NEWLINE | compound_stmt */
void statement(void)
{
    enter("statement");
    switch (nextToken)
    {
    case KW_KUNG:
    case KW_BAWAT:
    case KW_HABANG:
    case KW_ITUKOY:
    case KW_SURI:
        choose("compound_stmt");
        compound_stmt();
        break;
    default:
        choose("simple_stmt NEWLINE");
        simple_stmt();
        expect(TOKEN_NEWLINE, "the end of the line after the statement");
        break;
    }
    leave("statement");
}

/* simple_stmt ::= declaration_stmt | assignment_stmt | input_stmt |
                   output_stmt | return_stmt | call_stmt */
void simple_stmt(void)
{
    enter("simple_stmt");
    switch (nextToken)
    {
    case TYPE_NUM:
    case TYPE_TUL:
    case TYPE_BAKA:
    case TYPE_KAR:
    case TYPE_PISI:
    case TYPE_LISTA:
        choose("declaration_stmt (or input_stmt)");
        declaration_stmt(); /* also covers input_stmt (same first tokens) */
        break;
    case IDENT:
        choose("identifier_stmt (assignment_stmt or call_stmt)");
        identifier_stmt(); /* assignment_stmt or call_stmt */
        break;
    case IO_SULAT:
    case IO_SULATLN:
        choose("output_stmt");
        output_stmt();
        break;
    case KW_IBALIK:
        choose("return_stmt");
        return_stmt();
        break;
    case KW_KUNDIKUNG:
        errorMsg("'kundikung' has no matching 'kung' before it");
        break;
    case KW_KUNDI:
        errorMsg("'kundi' has no matching 'kung' before it");
        break;
    case KW_SAKALI:
        errorMsg("'sakali' can only be used inside a 'suri' block");
        break;
    case KW_SA:
        errorMsg("'sa' can only be used in a 'bawat' loop");
        break;
    case TOKEN_INDENT:
        errorMsg("unexpected indentation");
        break;
    default:
        error("a statement (declaration, assignment, kunin, sulat/sulatln, "
              "ibalik, function call, kung, bawat, habang, itukoy or suri)");
        break;
    }
    leave("simple_stmt");
}

/* compound_stmt ::= conditional_stmt | iterative_stmt | function_def |
                     match_stmt */
void compound_stmt(void)
{
    enter("compound_stmt");
    switch (nextToken)
    {
    case KW_KUNG:
        choose("conditional_stmt");
        conditional_stmt();
        break;
    case KW_BAWAT:
    case KW_HABANG:
        choose("iterative_stmt");
        iterative_stmt();
        break;
    case KW_ITUKOY:
        choose("function_def");
        function_def();
        break;
    case KW_SURI:
        choose("match_stmt");
        match_stmt();
        break;
    default:
        error("a compound statement");
        break;
    }
    leave("compound_stmt");
}

/* block ::= COLON NEWLINE INDENT statement { statement } DEDENT */
void block(void)
{
    enter("block");
    expect(COLON, "':' to start the block");
    expect(TOKEN_NEWLINE, "a new line after ':'");
    expect(TOKEN_INDENT, "an indented block (the line after ':' must be "
                         "indented)");
    guarded(statement);
    while (nextToken != TOKEN_DEDENT && nextToken != EOF)
        guarded(statement);
    expect(TOKEN_DEDENT, "the end of the indented block");
    leave("block");
}

/******************************************************/
/* ===== DECLARATION, INPUT, ASSIGNMENT, OUTPUT, RETURN, CALL ===== */

/* type ::= RW_NUM | RW_TUL | RW_BAKA | RW_KAR | RW_PISI | RW_LISTA */
void type(void)
{
    enter("type");
    if (isType(nextToken))
        lex();
    else
        error("a data type (num, tul, baka, kar, pisi or lista)");
    leave("type");
}

/* declaration_stmt ::= type IDENTIFIER [ ASSIGN expression ]
   input_stmt       ::= type IDENTIFIER ASSIGN RW_KUNIN LPAREN [ expression ]
                        RPAREN
   Both rules start with "type IDENTIFIER ASSIGN", so they are left-factored:
   after the ASSIGN, the next token RW_KUNIN selects input_stmt, anything
   else is the expression of a declaration. */
void declaration_stmt(void)
{
    enter("declaration_stmt");
    type();
    expect(IDENT, "a variable name after the data type");
    if (nextToken == ASSIGN_OP)
    {
        lex();
        if (nextToken == IO_KUNIN)
        {
            choose("input_stmt");
            input_stmt();
        }
        else
        {
            choose("expression (declaration with a value)");
            expression();
        }
    }
    leave("declaration_stmt");
}

/* the rest of input_stmt, from RW_KUNIN on:
   RW_KUNIN LPAREN [ expression ] RPAREN */
void input_stmt(void)
{
    enter("input_stmt");
    expect(IO_KUNIN, "'kunin'");
    expect(LEFT_PAREN, "'(' after kunin");
    if (startsExpression(nextToken))
        expression();
    expect(RIGHT_PAREN, "')' to close kunin(...)");
    leave("input_stmt");
}

/* assignment_stmt ::= IDENTIFIER ASSIGN expression
   call_stmt       ::= function_call
   function_call   ::= IDENTIFIER LPAREN [ expression_list ] RPAREN
   Both start with IDENTIFIER (left-factored): the token after the name
   decides - ASSIGN means assignment, LPAREN means a function call. */
void identifier_stmt(void)
{
    enter("identifier_stmt");
    expect(IDENT, "a name");
    if (nextToken == ASSIGN_OP)
    {
        choose("assignment_stmt");
        assignment_stmt();
    }
    else if (nextToken == LEFT_PAREN)
    {
        choose("call_stmt");
        call_stmt();
    }
    else
        error("'=' (assignment) or '(' (function call) after the name");
    leave("identifier_stmt");
}

/* the rest of assignment_stmt, from ASSIGN on: ASSIGN expression */
void assignment_stmt(void)
{
    enter("assignment_stmt");
    expect(ASSIGN_OP, "'='");
    expression();
    leave("assignment_stmt");
}

/* the rest of call_stmt, from LPAREN on */
void call_stmt(void)
{
    enter("call_stmt");
    function_call_rest();
    leave("call_stmt");
}

/* output_stmt ::= RW_SULAT LPAREN expression RPAREN
                 | RW_SULATLN LPAREN expression RPAREN */
void output_stmt(void)
{
    enter("output_stmt");
    if (nextToken == IO_SULAT || nextToken == IO_SULATLN)
        lex();
    else
        error("'sulat' or 'sulatln'");
    expect(LEFT_PAREN, "'(' after sulat/sulatln");
    expression();
    expect(RIGHT_PAREN, "')' to close the output statement");
    leave("output_stmt");
}

/* return_stmt ::= KW_IBALIK [ expression ] */
void return_stmt(void)
{
    enter("return_stmt");
    expect(KW_IBALIK, "'ibalik'");
    if (startsExpression(nextToken))
        expression();
    leave("return_stmt");
}

/******************************************************/
/* ===== CONDITIONAL STATEMENT =====
   conditional_stmt ::= if_clause { elif_clause } [ else_clause ] */
void conditional_stmt(void)
{
    enter("conditional_stmt");
    if_clause();
    while (nextToken == KW_KUNDIKUNG)
        elif_clause();
    if (nextToken == KW_KUNDI)
        else_clause();
    leave("conditional_stmt");
}

/* if_clause ::= "kung" expression block */
void if_clause(void)
{
    enter("if_clause");
    expect(KW_KUNG, "'kung'");
    expression();
    block();
    leave("if_clause");
}

/* elif_clause ::= "kundikung" expression block */
void elif_clause(void)
{
    enter("elif_clause");
    expect(KW_KUNDIKUNG, "'kundikung'");
    expression();
    block();
    leave("elif_clause");
}

/* else_clause ::= "kundi" block */
void else_clause(void)
{
    enter("else_clause");
    expect(KW_KUNDI, "'kundi'");
    block();
    leave("else_clause");
}

/******************************************************/
/* ===== ITERATIVE STATEMENT =====
   iterative_stmt ::= for_stmt | while_stmt                               */
void iterative_stmt(void)
{
    enter("iterative_stmt");
    if (nextToken == KW_BAWAT)
    {
        choose("for_stmt");
        for_stmt();
    }
    else if (nextToken == KW_HABANG)
    {
        choose("while_stmt");
        while_stmt();
    }
    else
        error("'bawat' or 'habang'");
    leave("iterative_stmt");
}

/* for_stmt ::= "bawat" IDENTIFIER "sa" expression block */
void for_stmt(void)
{
    enter("for_stmt");
    expect(KW_BAWAT, "'bawat'");
    expect(IDENT, "a loop variable after 'bawat'");
    expect(KW_SA, "'sa' after the loop variable");
    expression();
    block();
    leave("for_stmt");
}

/* while_stmt ::= "habang" expression block */
void while_stmt(void)
{
    enter("while_stmt");
    expect(KW_HABANG, "'habang'");
    expression();
    block();
    leave("while_stmt");
}

/******************************************************/
/* ===== FUNCTIONS =====
   function_def ::= KW_ITUKOY IDENTIFIER LPAREN [ parameter_list ] RPAREN
                    block                                                 */
void function_def(void)
{
    enter("function_def");
    expect(KW_ITUKOY, "'itukoy'");
    expect(IDENT, "a function name after 'itukoy'");
    expect(LEFT_PAREN, "'(' after the function name");
    if (nextToken != RIGHT_PAREN)
        parameter_list();
    expect(RIGHT_PAREN, "')' to close the parameter list");
    block();
    leave("function_def");
}

/* parameter_list ::= parameter { COMMA parameter } */
void parameter_list(void)
{
    enter("parameter_list");
    parameter();
    while (nextToken == COMMA)
    {
        lex();
        parameter();
    }
    leave("parameter_list");
}

/* parameter ::= [ type ] IDENTIFIER */
void parameter(void)
{
    enter("parameter");
    if (isType(nextToken))
        type();
    expect(IDENT, "a parameter name");
    leave("parameter");
}

/******************************************************/
/* ===== MATCH =====
   match_stmt ::= KW_SURI expression case_block                           */
void match_stmt(void)
{
    enter("match_stmt");
    expect(KW_SURI, "'suri'");
    expression();
    case_block();
    leave("match_stmt");
}

/* case_block ::= COLON NEWLINE INDENT case_clause { case_clause } DEDENT */
void case_block(void)
{
    enter("case_block");
    expect(COLON, "':' after the suri expression");
    expect(TOKEN_NEWLINE, "a new line after ':'");
    expect(TOKEN_INDENT, "an indented list of 'sakali' clauses");
    guarded(case_clause);
    while (nextToken == KW_SAKALI)
        guarded(case_clause);
    expect(TOKEN_DEDENT, "another 'sakali' clause or the end of the suri block");
    leave("case_block");
}

/* case_clause ::= KW_SAKALI pattern block */
void case_clause(void)
{
    enter("case_clause");
    expect(KW_SAKALI, "'sakali'");
    pattern();
    block();
    leave("case_clause");
}

/* pattern ::= INT_LITERAL | FLOAT_LITERAL | STRING_LITERAL |
               TRUE_LITERAL | FALSE_LITERAL | IDENTIFIER */
void pattern(void)
{
    enter("pattern");
    switch (nextToken)
    {
    case INT_LIT:
    case FLOAT_LIT:
    case STRING_LIT:
    case TRUE_LIT:
    case FALSE_LIT:
    case IDENT:
        lex();
        break;
    default:
        error("a pattern (number, string, totoo, palso or a name) after "
              "'sakali'");
        break;
    }
    leave("pattern");
}

/******************************************************/
/* ===== LOGICAL AND RELATIONAL EXPRESSIONS ===== */

/* expression ::= logical_expression */
void expression(void)
{
    enter("expression");
    logical_expression();
    leave("expression");
}

/* logical_expression ::= boolean_or_expr */
void logical_expression(void)
{
    enter("logical_expression");
    boolean_or_expr();
    leave("logical_expression");
}

/* boolean_or_expr ::= boolean_and_expr { OR boolean_and_expr }
   (the { } of EBNF becomes a while loop - same pattern as Sebesta's expr) */
void boolean_or_expr(void)
{
    enter("boolean_or_expr");
    boolean_and_expr();
    while (nextToken == O_OP)
    {
        lex();
        boolean_and_expr();
    }
    leave("boolean_or_expr");
}

/* boolean_and_expr ::= boolean_not_expr { AND boolean_not_expr } */
void boolean_and_expr(void)
{
    enter("boolean_and_expr");
    boolean_not_expr();
    while (nextToken == AT_OP)
    {
        lex();
        boolean_not_expr();
    }
    leave("boolean_and_expr");
}

/* boolean_not_expr ::= NOT boolean_not_expr | relational_expression */
void boolean_not_expr(void)
{
    enter("boolean_not_expr");
    if (nextToken == HINDI_OP)
    {
        choose("NOT boolean_not_expr");
        lex();
        boolean_not_expr();
    }
    else
    {
        choose("relational_expression");
        relational_expression();
    }
    leave("boolean_not_expr");
}

/* relational_expression ::= arithmetic_expression
                             [ rel_op arithmetic_expression ]
   rel_op ::= EQUAL | NOT_EQUAL | LESS_THAN | GREATER_THAN | LESS_EQUAL |
              GREATER_EQUAL                                               */
void relational_expression(void)
{
    enter("relational_expression");
    arithmetic_expression();
    if (nextToken == EQ_OP || nextToken == NEQ_OP || nextToken == LT_OP ||
        nextToken == GT_OP || nextToken == LE_OP || nextToken == GE_OP)
    {
        lex();
        arithmetic_expression();
    }
    leave("relational_expression");
}

/******************************************************/
/* ===== ARITHMETIC EXPRESSIONS (Sebesta's expr, term, factor) ===== */

/* arithmetic_expression ::= term { ( ADD | SUBTRACT ) term }
   Parses strings in the language generated by the rule:
   <expr> -> <term> {(+ | -) <term>}                                      */
void arithmetic_expression(void)
{
    enter("arithmetic_expression");
    /* Parse the first term */
    term();
    /* As long as the next token is + or -, get the next token and parse
       the next term */
    while (nextToken == ADD_OP || nextToken == SUB_OP)
    {
        lex();
        term();
    }
    leave("arithmetic_expression");
}

/* term ::= factor { ( MULTIPLY | DIVIDE | MODULO | FLOOR_DIVIDE ) factor } */
void term(void)
{
    enter("term");
    /* Parse the first factor */
    factor();
    /* As long as the next token is * / % or //, get the next token and
       parse the next factor */
    while (nextToken == MULT_OP || nextToken == DIV_OP ||
           nextToken == MOD_OP || nextToken == FLOOR_DIV_OP)
    {
        lex();
        factor();
    }
    leave("term");
}

/* factor ::= [ ADD | SUBTRACT ] power     (optional unary + or -) */
void factor(void)
{
    enter("factor");
    if (nextToken == ADD_OP || nextToken == SUB_OP)
        lex();
    power();
    leave("factor");
}

/* power ::= primary_expression [ EXPONENT power ]   (right-associative) */
void power(void)
{
    enter("power");
    primary_expression();
    if (nextToken == EXP_OP)
    {
        lex();
        power();
    }
    leave("power");
}

/* primary_expression ::= primary_atom { LBRACKET expression RBRACKET } */
void primary_expression(void)
{
    enter("primary_expression");
    primary_atom();
    while (nextToken == LEFT_BRACKET)
    {
        lex();
        expression();
        expect(RIGHT_BRACKET, "']' to close the index");
    }
    leave("primary_expression");
}

/* primary_atom ::= INT_LITERAL | FLOAT_LITERAL | TRUE_LITERAL |
                    FALSE_LITERAL | STRING_LITERAL | collection_literal |
                    function_call | IDENTIFIER | LPAREN expression RPAREN
   function_call and IDENTIFIER both start with IDENTIFIER (left-factored):
   an LPAREN right after the name makes it a function call. */
void primary_atom(void)
{
    enter("primary_atom");
    switch (nextToken)
    {
    case INT_LIT:
    case FLOAT_LIT:
    case TRUE_LIT:
    case FALSE_LIT:
    case STRING_LIT:
        choose("literal (the token itself)");
        lex();
        break;
    case LEFT_BRACKET:
    case LEFT_BRACE:
        choose("collection_literal");
        collection_literal();
        break;
    case IDENT:
        choose("IDENTIFIER (or function_call if '(' follows)");
        lex();
        if (nextToken == LEFT_PAREN)
            function_call_rest();
        break;
    case LEFT_PAREN:
        choose("LPAREN expression RPAREN");
        /* Sebesta: call lex to pass over the left parenthesis, call the
           expression parser, and check for the right parenthesis */
        lex();
        expression();
        expect(RIGHT_PAREN, "')' to close the parenthesis");
        break;
    default:
        error("an expression (a number, string, totoo/palso, name, function "
              "call, list/dictionary/set, or '(')");
        break;
    }
    leave("primary_atom");
}

/* the rest of function_call, after the name:
   LPAREN [ expression_list ] RPAREN */
void function_call_rest(void)
{
    enter("function_call");
    expect(LEFT_PAREN, "'(' after the function name");
    if (nextToken != RIGHT_PAREN)
        expression_list();
    expect(RIGHT_PAREN, "')' to close the argument list");
    leave("function_call");
}

/* expression_list ::= expression { COMMA expression } */
void expression_list(void)
{
    enter("expression_list");
    expression();
    while (nextToken == COMMA)
    {
        lex();
        expression();
    }
    leave("expression_list");
}

/******************************************************/
/* ===== COLLECTIONS =====
   collection_literal ::= list_literal | dict_literal | set_literal       */
void collection_literal(void)
{
    enter("collection_literal");
    if (nextToken == LEFT_BRACKET)
    {
        choose("list_literal");
        list_literal();
    }
    else
    {
        choose("dict_literal / set_literal");
        dict_or_set_literal();
    }
    leave("collection_literal");
}

/* list_literal ::= LBRACKET [ expression_list ] RBRACKET */
void list_literal(void)
{
    enter("list_literal");
    expect(LEFT_BRACKET, "'['");
    if (nextToken != RIGHT_BRACKET)
        expression_list();
    expect(RIGHT_BRACKET, "']' to close the list");
    leave("list_literal");
}

/* dict_literal ::= LBRACE [ key_value_pair { COMMA key_value_pair } ] RBRACE
   set_literal  ::= LBRACE expression_list RBRACE
   Both start with LBRACE (left-factored): "{}" is an empty dictionary;
   otherwise the first expression is parsed, and a COLON after it means the
   braces hold a dictionary, no COLON means a set. */
void dict_or_set_literal(void)
{
    enter("dict_literal / set_literal");
    expect(LEFT_BRACE, "'{'");
    if (nextToken != RIGHT_BRACE)
    {
        expression();
        if (nextToken == COLON) /* dictionary: key ':' value pairs */
        {
            choose("dict_literal: key COLON value { COMMA key_value_pair }");
            lex();
            expression();
            while (nextToken == COMMA)
            {
                lex();
                key_value_pair();
            }
        }
        else /* set: expression { COMMA expression } */
        {
            choose("set_literal: expression { COMMA expression }");
            while (nextToken == COMMA)
            {
                lex();
                expression();
            }
        }
    }
    else
        choose("empty dictionary {}");
    expect(RIGHT_BRACE, "'}' to close the dictionary or set");
    leave("dict_literal / set_literal");
}

/* key_value_pair ::= expression COLON expression */
void key_value_pair(void)
{
    enter("key_value_pair");
    expression();
    expect(COLON, "':' between a dictionary key and its value");
    expression();
    leave("key_value_pair");
}

/******************************************************/
/* openSymbolTable - open the file and skip its two header rows
   ("TOKEN TYPE   LEXEME" and the dashes) */
int openSymbolTable(const char *path)
{
    static const char header[] = "TOKEN TYPE";
    char line[ROW_MAX];
    int i;

    if ((in_fp = fopen(path, "r")) == NULL)
    {
        fprintf(stderr, "ERROR - cannot open %s (run the lexical analyzer "
                        "first: ./front yourfile.tagpy)\n", path);
        return 0;
    }
    if (fgets(line, sizeof line, in_fp) == NULL)
    {
        fprintf(stderr, "ERROR - %s is empty\n", path);
        return 0;
    }
    for (i = 0; header[i] != '\0'; i++)
        if (line[i] != header[i])
        {
            fprintf(stderr, "ERROR - %s is not a Symbol Table written by "
                            "front.c\n", path);
            return 0;
        }
    if (fgets(line, sizeof line, in_fp) == NULL) /* the "------" row */
        return 1;
    return 1;
}

/******************************************************/
/* writeSummary - the end of the Parsing Table file */
void writeSummary(void)
{
    int i;

    fprintf(table_fp, "\n==== SUMMARY ====\n");
    fprintf(table_fp, "Input           : %s\n", SYMBOL_TABLE_FILE);
    fprintf(table_fp, "Tokens read     : %d\n", tokenNo);
    fprintf(table_fp, "Table rows      : %d\n", step);
    fprintf(table_fp, "Syntax errors   : %d\n", errorCount);
    fprintf(table_fp, "Result          : %s\n",
            errorCount == 0 ? "the program is syntactically correct"
                            : "the program has syntax errors");
    if (errorCount > 0)
    {
        fprintf(table_fp, "\nSyntax errors (error recovery method: panic mode):\n");
        for (i = 0; i < loggedErrors; i++)
            fprintf(table_fp, "  %d. %s\n", i + 1, errLog[i]);
        if (errorCount > loggedErrors)
            fprintf(table_fp, "  ... and %d more\n", errorCount - loggedErrors);
    }
}

/******************************************************/
/* main driver (Sebesta: lex() once to prime nextToken, then call the
   start rule) */
int main(int argc, char *argv[])
{
    const char *path = SYMBOL_TABLE_FILE;
    int i;

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'q' && argv[i][2] == '\0')
            trace = 0;
        else
            path = argv[i];
    }

    if (!openSymbolTable(path))
        return 1;
    if ((table_fp = fopen(PARSING_TABLE_FILE, "w")) == NULL)
    {
        fprintf(stderr, "ERROR - cannot create %s\n", PARSING_TABLE_FILE);
        fclose(in_fp);
        return 1;
    }
    fprintf(table_fp, "TAGPY SYNTAX ANALYZER - PARSING TABLE\n");
    fprintf(table_fp, "Input: %s\n\n", path);
    fprintf(table_fp, "%-6s %-5s %-26s %-15s %-21s %s\n", "STEP", "LINE",
            "NON-TERMINAL", "LOOKAHEAD", "LEXEME", "ACTION");
    fprintf(table_fp, "%-6s %-5s %-26s %-15s %-21s %s\n", "----", "----",
            "------------", "---------", "------", "------");

    lex();     /* read the first token from the Symbol Table */
    program(); /* program ::= { statement } */
    logRow("program", nextToken, lexeme,
           errorCount == 0 ? "ACCEPT: end of input, no syntax errors"
                           : "END OF INPUT: syntax errors were found");
    writeSummary();
    fclose(table_fp);
    fclose(in_fp);

    printf("\nSyntax analysis of \"%s\": %d token(s) read, %d syntax error(s).\n",
           path, tokenNo, errorCount);
    printf("Parsing table written to \"%s\" (%d rows).\n", PARSING_TABLE_FILE,
           step);
    if (errorCount == 0)
        printf("The program is syntactically correct.\n");
    else
        printf("The program has syntax errors.\n");
    return (errorCount == 0) ? 0 : 2;
}

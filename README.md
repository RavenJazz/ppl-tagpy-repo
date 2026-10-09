# ppl-tagpy-repo

Repository for lexical analyzer in ppl:

How to use:
  -Copy the files to your pc
  -Change 'front.in' file to your desired equation
    example: x = 20 + 3
  -Compile front.c
  -Run front.exe
  -Output should be:
    Next token is: 11, Next lexeme is x
    Next token is: 20, Next lexeme is =
    Next token is: 10, Next lexeme is 20
    Next token is: 21, Next lexeme is +
    Next token is: 10, Next lexeme is 3
    Next token is: -1, Next lexeme is EOF

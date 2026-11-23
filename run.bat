@echo off
rem Compile and run the whole Tagpy front end with ONE command:
rem   lexical analyzer (front.c)  ->  "Symbol Table.txt"
rem   syntax analyzer  (syntax_analyzer.c)  ->  "Parsing Table.txt"
rem
rem Usage:  run.bat [file.tagpy] [trace]
rem   file.tagpy  the Tagpy program to check (default: sample.tagpy)
rem   trace       also show the full Enter/Exit parse trace on the screen
rem
rem Run it from the folder that contains front.c and syntax_analyzer.c.

setlocal
set "SRC=%~1"
if "%SRC%"=="" set "SRC=sample.tagpy"
set "MODE=-q"
if /I "%~2"=="trace" set "MODE="

echo [1/4] Compiling the lexical analyzer (front.c)...
gcc front.c -o front.exe
if errorlevel 1 goto :fail

echo [2/4] Compiling the syntax analyzer (syntax_analyzer.c)...
gcc syntax_analyzer.c -o syntax_analyzer.exe
if errorlevel 1 goto :fail

echo [3/4] Lexical analysis of %SRC% ...
front.exe "%SRC%"
if errorlevel 1 goto :fail

echo [4/4] Syntax analysis ...
syntax_analyzer.exe %MODE%
set "RC=%ERRORLEVEL%"

echo.
echo Output files:
echo   Symbol Table.txt   (tokens from the lexical analyzer)
echo   Parsing Table.txt  (parsing table from the syntax analyzer)
exit /b %RC%

:fail
echo.
echo Build or lexical analysis failed.
exit /b 1

@echo off
echo Building Vanilla Calculator...
g++ -Iinclude src/Lexer.cpp src/Parser.cpp src/GUI.cpp -o vanilla_calculator.exe -lgdi32 -luser32 -mwindows -municode -lgdiplus -lcomctl32 -lmsimg32 -luxtheme -ldwmapi -lole32 -lshell32 -luuid
if %ERRORLEVEL% EQU 0 (
    echo Build Successful! Run vanilla_calculator.exe
) else (
    echo Build Failed.
)

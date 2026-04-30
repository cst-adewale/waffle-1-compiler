@echo off
echo Building Waffle Studio...
g++ -Iinclude src/Lexer.cpp src/Parser.cpp src/GUI.cpp -o waffle_studio.exe -lgdi32 -luser32 -mwindows -municode -lgdiplus -lcomctl32 -lmsimg32 -luxtheme -ldwmapi
if %ERRORLEVEL% EQU 0 (
    echo Build Successful! Run waffle_studio.exe
) else (
    echo Build Failed.
)

@echo off
REM ==========================================================================
REM  build.bat - build/run without make (Windows, needs gcc in PATH)
REM
REM    build.bat          build tests + simulator
REM    build.bat test     build and run the 64 automated test cases
REM    build.bat sim      build and start the keyboard simulator
REM    build.bat report   run tests, save result to rpt\test_report.txt
REM ==========================================================================
setlocal
cd /d "%~dp0"
set CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -O2 -Isrc
set CORE=src\wm_fsm.c src\wm_debounce.c src\wm_app.c

if not exist build mkdir build

echo [build] test_wm.exe
gcc %CFLAGS% %CORE% testbench\test_wm.c -o build\test_wm.exe || goto :fail
echo [build] interactive_sim.exe
gcc %CFLAGS% %CORE% testbench\interactive_sim.c -o build\interactive_sim.exe || goto :fail

if /i "%1"=="test"   build\test_wm.exe
if /i "%1"=="sim"    build\interactive_sim.exe
if /i "%1"=="report" build\test_wm.exe --no-color > rpt\test_report.txt && echo Report saved to rpt\test_report.txt
goto :eof

:fail
echo BUILD FAILED
exit /b 1

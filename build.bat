@echo off
rem build.bat - compile the car simulator with MSVC
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /nologo /W4 /utf-8 /Fe:car.exe main.c car.c

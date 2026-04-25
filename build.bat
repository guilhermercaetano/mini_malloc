@echo off

REM Run standalone
REM Ideal compilation for running tests.
cl /D"RUN_STANDALONE" /Fd"mini_malloc" /Zi /Tc mini_malloc.c /link kernel32.lib

REM Just compile
REM This compilation mode is ideal if you want to use mini_malloc as a lib
REM cl /Fd"mini_malloc" /Zi /Tc mini_malloc.c /link kernel32.lib

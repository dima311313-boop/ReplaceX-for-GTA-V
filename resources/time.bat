@echo off
title GTA 5 Payza
echo Ожидание запуска игры...
:loop
tasklist /fi "imagename eq GTA5.exe" | find /i "GTA5.exe" > nul
if %errorlevel% equ 0 (
    echo Игра найдена. Заморозка...
    PsSuspend64.exe GTA5.exe
    timeout /t 20 /nobreak > nul
    PsSuspend64.exe -r GTA5.exe
    echo Игра разморожена. Запуск продолжается.
    exit
)
timeout /t 20 /nobreak > nul
goto loop
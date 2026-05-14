@echo off

cd %~dp0..\..\
powershell.exe -ExecutionPolicy Bypass -NoProfile -NoLogo -NonInteractive -File %~dp0run-all.ps1

(((echo."%cmdcmdline%")|find /I "%~0")>nul)
if %errorlevel% equ 0 pause

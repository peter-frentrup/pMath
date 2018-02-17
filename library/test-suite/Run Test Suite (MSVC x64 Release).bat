@echo off

rem %~dp0..\test\bin\windows\msvc-release-x64\test.exe -q -l run-all.pmath
%~dp0..\console-win\bin\windows\msvc-release-x64\console-win.exe --quit --load run-all.pmath

pause

@echo off

setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

set "searchPath=%ProgramFiles%"

if defined ProgramFiles(x86) (
	set "searchPath=%searchPath%;%ProgramFiles(x86)%"
)

for %%X in (CodeBlocks\codeblocks.exe) do set codeblocks.exe="%%~$searchPath:X"

if not defined codeblocks.exe (
	echo "CodeBlocks not found"
	pause
	goto end
)

pushd %~dp0

%codeblocks.exe% /na /nd /ns --debug-log --no-batch-window-close --target=All-Windows --build  pmath.cbp

popd

:end
endlocal

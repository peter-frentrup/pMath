@echo off

setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

set "searchPath=%ProgramFiles%"

if defined ProgramFiles(x86) (
	set "searchPath=%searchPath%;%ProgramFiles(x86)%"
)

for %%X in (CodeBlocks\codeblocks.exe) do set codeblocks.exe="%%~$searchPath:X"

if not defined codeblocks.exe (
	echo "CodeBlocks not found"
	goto end
)

pushd %~dp0

for %%T in (MSVC  GCC) do (
	for %%A in (x64  x86) do (
		for %%D in (Debug  Release) do (
			set target=Windows-%%T-%%D-%%A
			<NUL set /p ="Build target !target! ... "
			
			%codeblocks.exe% /na /nd /ns --debug-log --target=!target! --build  pmath.cbp
			if errorlevel 1 (
				echo failed
			) else (
				echo done
			)
		)
	)
)

popd

:end
pause
endlocal

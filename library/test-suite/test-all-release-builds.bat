@echo off

for /D %%I in (%~dp0..\test\bin\windows\*release*) do (
	echo "%%~fI ..."
	"%%I\test.exe" -q -l run-all.pmath
)

pause

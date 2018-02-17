@echo off

for /D %%I in (%~dp0..\console-win\bin\windows\*release*) do (
	echo "%%~fI ..."
	"%%I\console-win.exe" -q -l run-all.pmath
)

pause

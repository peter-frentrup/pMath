@echo off

setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

if "%PROCESSOR_ARCHITECTURE%" == "x86" (
	set "JAVA_REG_KEY=HKLM\Software\JavaSoft\Java Runtime Environment"
) else (
	set "JAVA_REG_KEY=HKLM\Software\Wow6432Node\JavaSoft\Java Runtime Environment"
)

for /F "usebackq skip=2 tokens=2*" %%A in (`reg.exe query "%JAVA_REG_KEY%" /v CurrentVersion 2^>NUL`) DO (
	set "JRE_VERSION=%%B"
)
if not defined JRE_VERSION (
	echo "Java Runtime Environment (32-bit) not found."
	goto end
)

for /F "usebackq skip=2 tokens=2*" %%A in (`reg.exe query "%JAVA_REG_KEY%\%JRE_VERSION%" /v JavaHome 2^>NUL`) DO (
	set "JRE_HOME=%%B"
)

"%JRE_HOME%\bin\java.exe" -d32 %*


:end
endlocal

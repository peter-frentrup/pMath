@echo off

setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

for %%X in (javac.exe) do (
	set javac.exe="%%~$PATH:X"
)

if defined javac.exe (
	goto found_javac
)

rem set "JAVA_REG_KEY=HKLM\Software\JavaSoft\Java Runtime Environment"
set "JAVA_REG_KEY=HKLM\Software\JavaSoft\Java Development Kit"

for /F "usebackq skip=2 tokens=2*" %%A in (`reg.exe query "%JAVA_REG_KEY%" /v CurrentVersion 2^>NUL`) do (
	set "JDK_VERSION=%%B"
)
if not defined JDK_VERSION (
	echo Java Development Kit not found.
	goto end
)

for /F "usebackq skip=2 tokens=2*" %%A in (`reg.exe query "%JAVA_REG_KEY%\%JDK_VERSION%" /v JavaHome 2^>NUL`) do (
	set "JDK_HOME=%%B"
)
if not defined JDK_HOME (
	echo Java Development Kit %JDK_VERSION% not found.
	goto end
)

set javac.exe="%JDK_HOME%\bin\javac.exe"

:found_javac

pushd %~dp0

%javac.exe% -source 7 -target 7 classpath\pmath\*.java
%javac.exe% -source 7 -target 7 classpath\pmath\util\*.java

%javac.exe% -source 7 -target 7 -cp classpath example\App\*.java
%javac.exe% -source 7 -target 7 -cp classpath example\Interrupt\*.java

popd

:end
pause
endlocal

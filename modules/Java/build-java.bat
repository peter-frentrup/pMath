@echo off

setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

for %%X in (javac.exe) do (
	set javac.exe="%%~$PATH:X"
)

for %%X in (javadoc.exe) do (
	set javadoc.exe="%%~$PATH:X"
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

if not defined javadoc.exe (
	set javadoc.exe="%JDK_HOME%\bin\javadoc.exe"
)


:found_javac

pushd %~dp0

%javac.exe% -source 7 -target 7 .\classpath\pmath\*.java  .\classpath\pmath\util\*.java .\example\App\*.java .\example\Interrupt\*.java

%javadoc.exe% -encoding UTF-8 -charset UTF-8 -docencoding UTF-8 -noqualifier java.lang -link https://docs.oracle.com/en/java/javase/14/docs/api -d .\doc -sourcepath .\classpath pmath pmath.util

popd

:end
pause
endlocal

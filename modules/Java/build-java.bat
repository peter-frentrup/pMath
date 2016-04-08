@echo off

setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

rem set "JAVA_REG_KEY=HKLM\Software\JavaSoft\Java Runtime Environment"
set "JAVA_REG_KEY=HKLM\Software\JavaSoft\Java Development Kit"

for /F "usebackq skip=2 tokens=2*" %%A in (`reg.exe query "%JAVA_REG_KEY%" /v CurrentVersion 2^>NUL`) DO (
	set "JDK_VERSION=%%B"
)
if not defined JDK_VERSION (
	echo Java Development Kit not found.
	goto end
)

for /F "usebackq skip=2 tokens=2*" %%A in (`reg.exe query "%JAVA_REG_KEY%\%JDK_VERSION%" /v JavaHome 2^>NUL`) DO (
	set "JDK_HOME=%%B"
)
if not defined JDK_HOME (
	echo Java Development Kit %JDK_VERSION% not found.
	goto end
)

set javac.exe="%JDK_HOME%\bin\javac.exe"

pushd %~dp0

%javac.exe% -source 6 -target 6 classpath\pmath\*.java
%javac.exe% -source 6 -target 6 classpath\pmath\util\*.java

%javac.exe% -source 6 -target 6 -cp classpath example\App\*.java
%javac.exe% -source 6 -target 6 -cp classpath example\Interrupt\*.java

popd

:end
pause
endlocal

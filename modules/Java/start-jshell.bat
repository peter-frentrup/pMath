@echo off


setlocal ENABLEEXTENSIONS  ENABLEDELAYEDEXPANSION

set PMATH_JAVA_DIR=%~dp0
set PMATH_JAVA_CLASSPATH=%PMATH_JAVA_DIR%\classpath
set PMATH_JAVA_BINDING_DLL=%PMATH_JAVA_DIR%\bin\Windows\x86-64\pmath-java.dll
set PMATH_CORE_BASE_DIR=%PMATH_JAVA_DIR%\..\..\library\scripts
set PMATH_CORE_DLL_DIR=%PMATH_JAVA_DIR%\..\..\library\bin\windows\msvc-debug-x64
set PMATH_CORE_DLL=%PMATH_CORE_DLL_DIR%\pmath.dll

for %%X in (jshell.exe) do (
	set jshell_exe="%%~$PATH:X"
)

if not defined jshell_exe (
	echo jshell.exe not found.
	goto end
)

set PATH=%PMATH_CORE_DLL_DIR%;%PATH%

rem TODO: also set pmath.core.base_directory or %PMATH_BASEDIRECTORY%
%jshell_exe% --class-path "%PMATH_JAVA_CLASSPATH%" "-R-Dpmath.core.dll=%PMATH_CORE_DLL%" "-R-Dpmath.core.base_directory=%PMATH_CORE_BASE_DIR%" "-R-Dpmath.binding.dll=%PMATH_JAVA_BINDING_DLL%"


:end
rem (((echo."%cmdcmdline%")|find /I "%~0")>nul)
rem if %errorlevel% equ 0 pause

endlocal

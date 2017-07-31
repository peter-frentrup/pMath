REM Parameter %1 is the target output directory, relative to the project directory (where this 
REM              file batch lives). 
REM              It is assumed to be a path of *three* directories, so "%1\..\..\.." is equivalent 
REM              to ".". This is needed for maininit.5.txt to refer to the correct file.
REM Parameter %2 is the target architecture. It can be x86 or x64.

IF not exist dependencies\windows\%2\ (
	MKDIR dependencies\windows\%2
)
IF not exist console\dependencies\windows\%2\ (
	MKDIR console\dependencies\windows\%2
)

MOVE /Y %1\libpmath.dll.def  %1\pmath.def

MKDIR test\%1
COPY /Y dependencies\windows\%2\*        test\%1\*
COPY /Y %1\*.dll                         test\%1\*
COPY /Y %1\*.pdb                         test\%1\*
COPY /Y scripts\maininit.5.txt           test\%1\maininit.pmath

MKDIR console\%1
COPY /Y console\dependencies\windows\%2\*   console\%1\*
COPY /Y dependencies\windows\%2\*           console\%1\*
COPY /Y %1\*.dll                            console\%1\*
COPY /Y %1\*.pdb                            console\%1\*
COPY /Y scripts\maininit.5.txt              console\%1\maininit.pmath

MKDIR console-win\%1
COPY /Y %1\*.dll                            console-win\%1\*
COPY /Y %1\*.pdb                            console-win\%1\*
COPY /Y scripts\maininit.5.txt              console-win\%1\maininit.pmath

rem Only generate pmath.lib if we did a gcc build:
IF EXIST %1\pmath.def  LIB /DEF:%1\pmath.def /OUT:%1\pmath.lib

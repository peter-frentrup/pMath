MKDIR test\bin\windows\%1
COPY /Y depencies\windows\%2\*           test\bin\windows\%1\*
MOVE /Y bin\windows\%1\libpmath.dll.def  bin\windows\%1\pmath.def
COPY /Y bin\windows\%1\pmath*.dll        test\bin\windows\%1\*
COPY /Y bin\windows\%1\pmath*.pdb        test\bin\windows\%1\*
COPY /Y scripts\maininit.5.txt           test\bin\windows\%1\maininit.pmath

MKDIR console\bin\windows\%1
COPY /Y depencies\windows\%2\*           console\bin\windows\%1\*
COPY /Y bin\windows\%1\pmath*.dll        console\bin\windows\%1\*
COPY /Y bin\windows\%1\pmath*.pdb        console\bin\windows\%1\*
COPY /Y scripts\maininit.5.txt           console\bin\windows\%1\maininit.pmath

rem Only generate pmath.lib if we did a gcc build:
IF EXIST bin\windows\%1\pmath.def  LIB /DEF:bin\windows\%1\pmath.def /OUT:bin\windows\%1\pmath.lib

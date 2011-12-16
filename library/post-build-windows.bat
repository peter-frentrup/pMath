MKDIR test\bin\windows\%1
COPY /Y depencies\windows\*              test\bin\windows\%1\*
MOVE /Y bin\windows\%1\libpmath.dll.def  bin\windows\%1\pmath.def
COPY /Y bin\windows\%1\pmath*.dll        test\bin\windows\%1\*
COPY /Y scripts\maininit.5.txt           test\bin\windows\%1\maininit.pmath

rem Only generate pmath.lib if we did a gcc build:
IF EXIST bin\windows\%1\libpmath.dll.a  LIB /DEF:bin\windows\%1\pmath.def /OUT:bin\windows\%1\pmath.lib
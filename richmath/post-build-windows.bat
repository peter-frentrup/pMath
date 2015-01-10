
REM  argument %1 is pmath library build name ( ..\library\bin\windows\%1\* )
REM  argument %2 is dependency architecture  ( dependencies\windows\%2\* )
REM  argument %3 is target build name        ( bin\windows\%3\* )


COPY /Y ..\library\bin\windows\%1\pmath*.dll bin\windows\%3\*
COPY /Y dependencies\windows\%2\*            bin\windows\%3\*

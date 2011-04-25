COPY /Y ..\library\bin\windows\%1\pmath*.dll bin\windows\%1\*
COPY /Y depencies\windows\*                  bin\windows\%1\*
ECHO Get(ToFileName({DirectoryName($Input, 5), "library", "scripts"}, "maininit.pmath")) > bin\windows\%1\maininit.pmath
ECHO Get(ToFileName({DirectoryName($Input, 4), "scripts"}, "frontinit.pmath")) >> bin\windows\%1\maininit.pmath
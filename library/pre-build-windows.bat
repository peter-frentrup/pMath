
PUSHD %~dp0

>src\pmath-util\version.g.in.c  ECHO // git log -1
>>src\pmath-util\version.g.in.c git log -1 --format="#define GIT_COMMIT_SHA %%x22%%H%%x22%%n#define GIT_COMMIT_DATE_ISO %%x22%%cI%%x22%%n#define GIT_COMMIT_DATE_UNIX %%ct"

>>src\pmath-util\version.g.in.c ECHO // git rev-list master --count
>>src\pmath-util\version.g.in.c ECHO #define GIT_COMMITS_MASTER \
>>src\pmath-util\version.g.in.c git rev-list master --count

>>src\pmath-util\version.g.in.c ECHO // git rev-list master..HEAD --count
>>src\pmath-util\version.g.in.c ECHO #define GIT_COMMITS_SINCE_MASTER \
>>src\pmath-util\version.g.in.c git rev-list master..HEAD --count

CD src\pmath-util

>NUL 2>&1 FC/B version.g.in.c version.g.c

IF ERRORLEVEL 1 COPY/B version.g.in.c version.g.c

DEL version.g.in.c

POPD

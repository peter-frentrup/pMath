#!/bin/bash -x


>src/pmath-util/version.g.in.c  echo "// git log -1"
>>src/pmath-util/version.g.in.c git log -1 --format="#define GIT_COMMIT_SHA %x22%H%x22%n#define GIT_COMMIT_DATE_ISO %x22%cI%x22%n#define GIT_COMMIT_DATE_UNIX %ct"

>>src/pmath-util/version.g.in.c echo "// git rev-list master --count"
>>src/pmath-util/version.g.in.c echo "#define GIT_COMMITS_MASTER \\"
>>src/pmath-util/version.g.in.c git rev-list master --count

>>src/pmath-util/version.g.in.c echo "// git rev-list master..HEAD --count"
>>src/pmath-util/version.g.in.c echo "#define GIT_COMMITS_SINCE_MASTER \\"
>>src/pmath-util/version.g.in.c git rev-list master..HEAD --count

pushd src/pmath-util/

if cmp --quiet ./version.g.in.c ./version.g.c; then
	rm ./version.g.in.c
else
	mv --force ./version.g.in.c ./version.g.c
fi

popd

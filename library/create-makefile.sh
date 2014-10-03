#!/bin/bash

CFLIB="-DBUILDING_PMATH -DPMATH_USE_PTHREAD -fPIC"
CFLAGS="$CFLAGS -Iinclude -Wall"
LDFLAGS="$LDFLAGS"
SOFLAGS="-shared"
LIBS="-lm -lpthread -lgmp -lmpfr -liconv -lpcre16 -ldl -lz"
MAYOR="0"
MINOR="1"
SYSTEM="$(uname -s)"
MACHINE="$(uname -m)"
SRCDIR="src"
OBJDIR="obj/$SYSTEM/$MACHINE"
BINDIR="bin/$SYSTEM/$MACHINE"
TARGET="debug"
SHORTNAME="pmath"
RESULT=libpmath.so
RESULTM=$RESULT.$MAYOR
RESULTMM=$RESULTM.$MINOR

rm -r tmp
mkdir tmp

cat > tmp/nop.c <<-_EOF_
	int main(int argc, char **argv){
		return 0;
	}
	_EOF_


function printinfo {
	printf "$1" >&2
}

printinfo "CFLAGS: $CFLAGS\n"


printinfo "checking for -m64 option... "
rm -f  tmp/nop  tmp/nop.o
gcc $CFLAGS  -m64 -o tmp/nop.o -c tmp/nop.c 2>/dev/null && \
gcc $LDFLAGS -m64 -o tmp/nop      tmp/nop.o 2>/dev/null
if [ -f tmp/nop ]; then
	 CFLAGS="$CFLAGS -m64"
	LDFLAGS="$LDFLAGS -m64"
	printinfo "ok\n"
else
	printinfo "failed\n"
fi


printinfo "checking for -mptr64 option... "
rm -f  tmp/nop  tmp/nop.o
gcc $CFLAGS  -mptr64 -o tmp/nop.o -c tmp/nop.c 2>/dev/null && \
gcc $LDFLAGS -mptr64 -o tmp/nop      tmp/nop.o 2>/dev/null
if [ -f tmp/nop ]; then
	 CFLAGS="$CFLAGS -mptr64"
	LDFLAGS="$LDFLAGS -mptr64"
	printinfo "ok\n"
else
	printinfo "failed\n"
fi



printinfo "checking for -march=native option... "
rm -f  tmp/nop  tmp/nop.o
gcc $CFLAGS  -march=native -o tmp/nop.o -c tmp/nop.c 2>/dev/null && \
gcc $LDFLAGS -march=native -o tmp/nop      tmp/nop.o 2>/dev/null
if [ -f tmp/nop ]; then
	 CFLAGS="$CFLAGS -march=native"
	LDFLAGS="$LDFLAGS -march=native"
	printinfo "ok\n"
else
	printinfo "failed\n"
fi



printinfo "checking for -mfpmath=sse option... "
rm -f  tmp/nop  tmp/nop.o
gcc $CFLAGS  -mfpmath=sse -o tmp/nop.o -c tmp/nop.c 2>/dev/null && \
gcc $LDFLAGS -mfpmath=sse -o tmp/nop      tmp/nop.o 2>/dev/null
if [ -f tmp/nop ]; then
	 CFLAGS="$CFLAGS -mfpmath=sse"
	LDFLAGS="$LDFLAGS -mfpmath=sse"
	printinfo "ok\n"
else
	printinfo "failed\n"
fi



cat > tmp/nested_functions.c <<-_EOF_
	int main(int argc, char **argv){
		int x;
		int f(int y){
			return x + y;
		};
		
		x = 9;
		return f(1);
	}
	_EOF_
printinfo "checking for -fnested-functions option... "
rm -f  tmp/nested_functions  tmp/nested_functions.o
gcc $CFLAGS  -fnested-functions -o tmp/nested_functions.o -c tmp/nested_functions.c 2>/dev/null && \
gcc $LDFLAGS -o tmp/nested_functions      tmp/nested_functions.o 2>/dev/null
if [ -f tmp/nested_functions ]; then
	 CFLAGS="$CFLAGS -fnested-functions"
	LDFLAGS="$LDFLAGS"
	printinfo "ok\n"
else
	printinfo "failed\n"
fi



cat > tmp/malloc_usable_size.c <<-_EOF_
	#include <malloc.h>
	int main(int argc, char **argv){
		return (int)malloc_usable_size(NULL);
	}
	_EOF_
printinfo "checking for malloc_usable_size function... "
rm -f  tmp/malloc_usable_size  tmp/malloc_usable_size.o
gcc $CFLAGS  -mfpmath=sse -o tmp/malloc_usable_size.o -c tmp/malloc_usable_size.c 2>/dev/null && \
gcc $LDFLAGS -mfpmath=sse -o tmp/malloc_usable_size      tmp/malloc_usable_size.o 2>/dev/null
if [ -f tmp/malloc_usable_size ]; then
	printinfo "ok\n"
else
	CFLAGS="$CFLAGS -DPMATH_USE_DLMALLOC"
	printinfo "failed\n"
fi



#case "$MACHINE" in
#	"sun4u" )
#		CFLAGS="$CFLAGS -pthreads"
#		LDFLAGS="$LDFLAGS -pthreads"
#		;;
#	
#	* )
#		;;
#esac

case "$SYSTEM" in
	"Linux" )
		LIBS="$LIBS -lrt"
		CFLAGS="$CFLAGS -D_XOPEN_SOURCE=600 -std=c99"
		LDFLAGS="$LDFLAGS -pthread"
		SOFLAGS="$SOFLAGS -Wl,-soname,$""(LIBNAME).$""(MAYOR)"
		;;
		
	"SunOS" )
		LIBS="$LIBS -lrt"
		CFLAGS="$CFLAGS -pthreads"
		SOFLAGS="$SOFLAGS -Wl,-h,-soname,$""(LIBNAME).$""(MAYOR)"
		;;
	
	"Darwin" )
		CFLAGS="$CFLAGS -pthreads"
		
		# -install_name /usr/local/lib/lib$""(SHORTLIBNAME).$""(MAYOR).dylib
		# -Wl,-h -Wl,lib$""(SHORTLIBNAME).$""(MAYOR).dylib
		SOFLAGS="$SOFLAGS -framework CoreServices -dynamiclib -current_version $""(MAYOR).$""(MINOR)"
		RESULT=libpmath.dylib
		RESULTM=libpmath.$MAYOR.dylib
		RESULTMM=libpmath.$MAYOR.$MINOR.dylib
		;;
		
	* )
		;;
esac

if [ "$1" != "" ]; then
	TARGET="$1"
fi

case "$TARGET" in
	"debug" )
		CFLAGS="$CFLAGS -g -DPMATH_DEBUG_LOG -DPMATH_DEBUG_MEMORY -DPMATH_DEBUG_TESTS -DPMATH_DEBUG_NO_FASTREF"
		;;
		
	"release" )
		CFLAGS="$CFLAGS -s -O3 -DNDEBUG"
		;;
		
	* )
		echo "unknown Target type $TARGET" >&2
		exit 1
		;;
esac

OBJDIR="$OBJDIR/$TARGET"
BINDIR="$BINDIR/$TARGET"

printinfo "binaries go to $BINDIR\n"
printinfo "building depency list... "

echo "# Makefile created by $0 at $(date)"
echo "# System: $SYSTEM"
echo "# Machine: $MACHINE"
echo "# Target: $TARGET"
echo "CC = gcc"
echo "LINK = g++"
#echo "SYMLINK = ln -fs"
echo "CP = cp -f"
echo "RM = rm -f"
echo "MKDIR = mkdir -p"
echo "MAYOR = $MAYOR"
echo "MINOR = $MINOR"
echo "TESTNAME = pmath-test"
echo "SHORTLIBNAME = $SHORTNAME"
echo "LIBNAME = lib$""(SHORTLIBNAME).so"
echo "CFLAGS = $CFLAGS $CFLIB"
echo "LDFLAGS = $LDFLAGS $SOFLAGS"
echo "LIBS = $LIBS"
echo "TESTCFLAGS = $CFLAGS"
echo "TESTLDFLAGS = $LDFLAGS -Wl,-rpath,?ORIGIN"
echo "CHRPATH = chrpath -r '$""$""ORIGIN'"
echo "TESTLIBS = -l$""(SHORTLIBNAME) -lrt $""(LIBS)"
echo "RESULTDIR = $BINDIR"
echo "RESULT = $""(RESULTDIR)/$RESULT"
echo "RESULTM = $""(RESULTDIR)/$RESULTM"
echo "RESULTMM = $""(RESULTDIR)/$RESULTMM"
echo "TEST = $""(RESULTDIR)/$""(TESTNAME)"
echo
echo ".PHONY: all clean"
echo
echo "all: $""(TEST)"
echo

ALLOBJS=""

for srcfile in $( find src -type f -name *.c ); do
	objfile="$OBJDIR/$srcfile.o"
	dep="$(gcc -MM -MG -MT $objfile $CFLAGS $CFLIB $srcfile)"
	if  [ "$?" == "0" ]; then
		ALLOBJS="$ALLOBJS $objfile"
		echo "$dep"
		echo -e "\t$""(MKDIR) $( dirname $objfile )  &&  $""(CC) $""(CFLAGS) -o $objfile -c $srcfile"
		echo
	fi
done

echo "$""(RESULTMM): $ALLOBJS"
echo -e "\t$""(MKDIR) $""(RESULTDIR) && $""(LINK) $""(LDFLAGS) $""(LIBS) -o $""(RESULTMM) $ALLOBJS"
echo
echo "$""(RESULTM): $""(RESULTMM)"
echo -e "\t$""(RM) $""(RESULTM)"
#echo -e "\t$""(SYMLINK) $""(RESULTMM) $""(RESULTM) || $""(CP) $""(RESULTMM) $""(RESULTM)"
echo -e "\t$""(CP) $""(RESULTMM) $""(RESULTM)"
echo
echo "$""(RESULT): $""(RESULTM)"
echo -e "\t$""(RM) $""(RESULT)"
#echo -e "\t$""(SYMLINK) $""(RESULTM) $""(RESULT) || $""(CP) $""(RESULTM) $""(RESULT)"
echo -e "\t$""(CP) $""(RESULTM) $""(RESULT)"
echo
echo "$OBJDIR/test/main.o: test/main.c"
echo -e "\t$""(MKDIR) $OBJDIR/test && $""(CC) $""(TESTCFLAGS) -o $OBJDIR/test/main.o -c test/main.c"
echo 
echo "$""(RESULTDIR)/maininit.pmath: scripts/maininit.5.txt"
echo -e "\t$""(CP) scripts/maininit.5.txt $""(RESULTDIR)/maininit.pmath"
echo
echo "$""(TEST): $""(RESULT) $OBJDIR/test/main.o $""(RESULTDIR)/maininit.pmath"
echo -e "\t$""(LINK) $""(TESTLDFLAGS) $""(TESTLIBS) -L$""(RESULTDIR) -l$""(SHORTLIBNAME) -o $""(TEST) $OBJDIR/test/main.o"
echo -e "\t$""(CHRPATH) $""(TEST)"
echo
echo "clean:"
echo -e "\t$""(RM) -r $BINDIR $OBJDIR\n"
echo

printinfo "ok\n"

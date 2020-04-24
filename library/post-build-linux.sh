#!/bin/bash -x

function ln-copy {
	ln -sf "$1" "$2"
	if [ "$?" != "0" ]; then
		cp "$1" "$2"
	fi
}

mkdir -p dependencies/linux/$2
mkdir -p console/dependencies/linux/$2

mkdir -p test/$1
cp -r dependencies/linux/$2/.           test/$1/
cp    scripts/maininit.5.txt            test/$1/maininit.pmath
#echo "Get(ToFileName({DirectoryName($""Input, 5), \"scripts\"}, \"maininit.pmath\"))" > test/$1/maininit.pmath
cp    $1/libpmath.so.0.1                test/$1/

mkdir -p console/$1
cp -r console/dependencies/linux/$2/.   console/$1/
cp    scripts/maininit.5.txt            console/$1/maininit.pmath
cp    $1/libpmath.so.0.1                console/$1/

function ln-copy-all {
	ln-copy "$(pwd)/libpmath.so.0.1" libpmath.so.0
	ln-copy "$(pwd)/libpmath.so.0"   libpmath.so
}

pushd $1
ln-copy-all
popd

pushd test/$1
ln-copy-all
popd

pushd console/$1
ln-copy-all
popd

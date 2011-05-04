#!/bin/bash -x

function ln-copy {
	ln -sf "$1" "$2"
	if [ "$?" != "0" ]; then
		cp "$1" "$2"
	fi
}

mkdir -p test/bin/linux/$1
cp depencies/linux/*              test/bin/linux/$1/*
cp scripts/maininit.5.txt         test/bin/linux/$1/maininit.pmath
#echo "Get(ToFileName({DirectoryName($""Input, 5), \"scripts\"}, \"maininit.pmath\"))" > test/bin/linux/$1/maininit.pmath
cp bin/linux/$1/libpmath.so.0.1   test/bin/linux/$1/*

function ln-copy-all {
	ln-copy $(pwd)/libpmath.so.0.1 libpmath.so.0
	ln-copy $(pwd)/libpmath.so.0   libpmath.so
}

pushd bin/linux/$1
ln-copy-all
popd

pushd test/bin/linux/$1
ln-copy-all
popd

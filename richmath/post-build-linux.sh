#!/bin/bash -x

function ln-copy {
	ln -sf "$1" "$2"
	if [ "$?" != "0" ]; then
		cp "$1" "$2"
	fi
}

cp ../library/bin/linux/$1/*.so.0.1  bin/linux/$1/
cp depencies/linux/*                 bin/linux/$1/

function ln-copy-all {
	ln-copy $(pwd)/libpmath.so.0.1 libpmath.so.0
	ln-copy $(pwd)/libpmath.so.0   libpmath.so
}

pushd bin/linux/$1
ln-copy-all
popd


#!/bin/bash -x

function ln-copy {
	ln -sf "$1" "$2"
	if [ "$?" != "0" ]; then
		cp "$1" "$2"
	fi
}

cp ../library/bin/linux/$1/*.so.0.1  bin/linux/$2/
cp depencies/linux/*                 bin/linux/$2/

function ln-copy-all {
	ln-copy $(pwd)/libpmath.so.0.1 libpmath.so.0
	ln-copy $(pwd)/libpmath.so.0   libpmath.so
}

pushd bin/linux/$2
ln-copy-all

# add $ORIGIN to the rpath. Due to the $ sign, this cannot reliably be done through Code::Blocks IDE
chrpath -r '$ORIGIN' ./richmath

popd


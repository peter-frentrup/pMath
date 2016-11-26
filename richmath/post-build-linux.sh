#!/bin/bash -x
#
# argument $1 is pmath library build name ( ../library/bin/linux/$1/* )
# argument $2 is dependency architecture  ( dependencies/linux/$2/* )
# argument $3 is target build name        ( bin/linux/$3/* )



function ln-copy {
	ln -sf "$1" "$2"
	if [ "$?" != "0" ]; then
		cp "$1" "$2"
	fi
}

cp ../library/bin/linux/$1/*.so.0.1  bin/linux/$3/
cp dependencies/default/*            bin/linux/$3/
cp dependencies/linux/$2/*           bin/linux/$3/

function ln-copy-all {
	ln-copy $(pwd)/libpmath.so.0.1 libpmath.so.0
	ln-copy $(pwd)/libpmath.so.0   libpmath.so
}

pushd bin/linux/$3
ln-copy-all

# add $ORIGIN to the rpath. Due to the $ sign, this cannot reliably be done through Code::Blocks IDE
chrpath -r '$ORIGIN' ./richmath

popd


#!/bin/bash -x


pushd $1

# add $ORIGIN to the rpath. Due to the $ sign, this cannot be done through any more  Code::Blocks IDE with "-Wl,-rpath,\\$$ORIGIN"
chrpath -r '$ORIGIN' ./pmathc

popd


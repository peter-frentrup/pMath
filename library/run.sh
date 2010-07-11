#!/bin/bash

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$(pwd)/$(dirname $1)"
export PMATH_BASEDIRECTORY="$(pwd)/$(dirname $1)"

./$@

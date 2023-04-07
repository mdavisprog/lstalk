#!/bin/bash

pushd "$(dirname "${BASH_SOURCE[0]}")"

# If source path is not found, then the 'defines.sh' may not have been run. Run it here.
if [[ -z $SOURCE_PATH ]] ; then
    source defines.sh
fi

if [[ ! -z $GENERATOR ]]; then
    CMAKE_OPTIONS="-G $GENERATOR"
fi

CMAKE_OPTIONS="$CMAKE_OPTIONS -S .. -B $BUILD_PATH -DCMAKE_BUILD_TYPE=$CONFIGURATION"
CMAKE_OPTIONS="$CMAKE_OPTIONS -DWITH_TESTS=$WITH_TESTS -DBUILD_SHARED_LIBS=$BUILD_SHARED -DNO_LIB=$NO_LIB -DWITH_EXAMPLES=$WITH_EXAMPLES"

cmake $CMAKE_OPTIONS

popd

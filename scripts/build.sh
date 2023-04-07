#!/bin/bash

pushd "$(dirname "${BASH_SOURCE[0]}")"

source defines.sh
source generate.sh

if [ "$GENERATOR" = "Ninja" ] ; then
    NINJA_VERSION=$(ninja --version)
    echo "Using ninja version $NINJA_VERSION"
    ninja -C $BUILD_PATH
elif [ "$GENERATOR" = "Xcode" ] ; then
    xcodebuild -configuration $CONFIGURATION -scheme ALL_BUILD -project "$BUILD_PATH/OctaneGUI.xcodeproj"
else
    make -C $BUILD_PATH
fi

if [ "$INSTALL" = true ] ; then
    cmake --install ../builds
fi

popd

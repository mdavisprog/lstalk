#!/bin/bash

BUILD_SHARED=OFF
CONFIGURATION=Debug
GENERATOR=
HELP=false
INSTALL=false
NO_LIB=OFF
XCODE=false
WITH_EXAMPLES=OFF
WITH_TESTS=OFF

BIN_PATH=../bin
BUILD_PATH=../build
LIB_PATH=../lib

for Var in "$@"
do
    Var=$(echo $Var | tr '[:upper:]' '[:lower:]')
    case ${Var} in
        examples) WITH_EXAMPLES=ON ;;
        help) HELP=true ;;
        install) INSTALL=true ;;
        ninja) GENERATOR=Ninja ;;
        nolib) NO_LIB=ON ;;
        release) CONFIGURATION=Release ;;
        shared) BUILD_SHARED=ON ;;
        tests) WITH_TESTS=ON ;;
        xcode) GENERATOR=Xcode ;;
        *) break
    esac
done

if [[ $OSTYPE == "darwin"* ]]; then
    if [[ "$XCODE" = true ]] ; then
        GENERATOR=Xcode
    fi
fi

if [ "$HELP" = true ] ; then
    echo "This is a list of arguments and a description for each:"
    echo "release        Sets the configuration to Release. The default is Debug."
    echo "ninja          Use the ninja build system instead of the default (make). The path"
    echo "               to the ninja build system must be added to the PATH environment variable."
    echo "shared         Compiles the library as shared libraries."
    echo "install        Install built libraries."
    echo "nolib          Compiles the library directly into all of the programs. By default, programs"
    echo "               link the library statically."
    echo "examples       Compiles the example programs."
    echo "tests          Compiles the testing framework."
    echo "help           Displays this help message."
    exit 0
fi

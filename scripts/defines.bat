@ECHO OFF

SET BIN_PATH=..\bin
SET BUILD_PATH=..\build
SET BUILD_SHARED=OFF
SET CONFIGURATION=Debug
SET GENERATOR=
SET HELP=FALSE
SET INSTALL=FALSE
SET LIB_PATH=..\lib
SET NINJA=FALSE
SET NO_LIB=OFF
SET WITH_EXAMPLES=OFF
SET WITH_TESTS=OFF

:PARSE_ARGS
IF NOT "%1" == "" (
    IF /I "%1" == "Shared" SET BUILD_SHARED=ON
    IF /I "%1" == "Release" SET CONFIGURATION=Release
    IF /I "%1" == "Help" SET HELP=TRUE
    IF /I "%1" == "Install" SET INSTALL=TRUE
    IF /I "%1" == "Ninja" SET GENERATOR=Ninja
    IF /I "%1" == "NoLib" SET NO_LIB=ON
    IF /I "%1" == "Examples" SET WITH_EXAMPLES=ON
    IF /I "%1" == "Tests" SET WITH_TESTS=ON
    SHIFT
    GOTO :PARSE_ARGS
)

IF "%HELP%" == "TRUE" (
    ECHO This is a list of arguments and a description for each:
    ECHO release        Sets the configuration to Release. The default is Debug.
    ECHO ninja          Use the ninja build system instead of the default (msbuild^). The path
    ECHO                to the ninja build system must be added to the PATH environment variable.
    ECHO shared         Compiles the library as shared libraries.
    ECHO install        Install built libraries.
    ECHO nolib          Compiles the library directly into all of the programs. By default, programs
    ECHO                link the library statically.
    ECHO examples       Compiles the example programs.
    ECHO tests          Compiles the testing framework.
    ECHO help           Displays this help message.
    EXIT 0
)

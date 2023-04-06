@ECHO OFF

SETLOCAL ENABLEDELAYEDEXPANSION
PUSHD "%~dp0"

CALL defines.bat %*
CALL vcvars.bat %*

IF NOT EXIST "%VCVARS%" (
    ECHO VCVars batch file "%VCVARS%" does not exist!. A valid Visual Studio installation was not found. Please verify a valid Visual Studio install exists before attempting to call the VCVars batch file.
    EXIT -1
)

CALL "%VCVARS%"
CALL generate.bat

IF "%GENERATOR%" == "Ninja" (
    ninja --version
    ninja -C %BUILD_PATH%
) ELSE (
    msbuild %BUILD_PATH%\lstalk.sln /p:Configuration=%CONFIGURATION%
)

IF "%INSTALL%" == "TRUE" (
    cmake --install %BUILD_PATH%
)

POPD
ENDLOCAL

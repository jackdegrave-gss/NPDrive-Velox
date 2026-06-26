@echo off
REM Build the NP-Drive / Velox driver project with the installed VS Build Tools.
REM Uses the MSVC compiler plus the CMake and Ninja bundled with Build Tools,
REM so no separately-installed CMake is required.

setlocal
set "VS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
set "VCVARS=%VS%\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

set "ROOT=%~dp0.."
set "BUILD=%ROOT%\build"

call "%VCVARS%" || exit /b 1
"%CMAKE%" -S "%ROOT%" -B "%BUILD%" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" || exit /b 1
"%CMAKE%" --build "%BUILD%" || exit /b 1
echo.
echo Build complete. Run the smoke test with:
echo   "%BUILD%\npdrive_smoke.exe"
endlocal

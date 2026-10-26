@echo off
REM Requires: Visual Studio 2022 (Desktop development with C++), CMake, git.
cmake -B build -G "Visual Studio 17 2022" -A x64 -DAURORA_COPY_AFTER_BUILD=OFF
cmake --build build --config Release --parallel
echo.
echo Done. Plugin: build\Aurora_artefacts\Release\VST3\
echo Copy the .vst3 folder to C:\Program Files\Common Files\VST3\
pause

#!/bin/bash
# Requires: Xcode command line tools (xcode-select --install), CMake (brew install cmake), git.
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DAURORA_COPY_AFTER_BUILD=OFF
cmake --build build --config Release --parallel
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build/Aurora_artefacts/Release/VST3/*.vst3 ~/Library/Audio/Plug-Ins/VST3/
xattr -cr ~/Library/Audio/Plug-Ins/VST3/*.vst3 || true   # unsigned plugin: drop the quarantine flag
echo "Installed to ~/Library/Audio/Plug-Ins/VST3 - rescan plugins in your DAW."

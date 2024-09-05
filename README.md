# FeatherPad (for Windows)

## Overview

[FeatherPad](https://github.com/tsujan/FeatherPad) (by Pedram Pourang, a.k.a. Tsu Jan <tsujan2000@gmail.com>) is a lightweight Qt plain-text editor for Linux. 

The only purpose of this fork is to add the ability to compile it for Windows, using MSYS2 and MINGW64.

## Requirements for Windows
* MSYS2 with MINGW64
* $ pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-svg mingw-w64-x86_64-hunspell mingw-w64-x86_64-qt6-tools pkg-config
* CMake installed natively at default location C:\Program Files\CMake\bin (the MSYS2 cmake version didn't work for me)
* Optional: 7-zip installed at default location C:\Program Files\7-Zip
* Optional: NSIS installed at default location C:\Program Files (x86)\NSIS

## Compilation (in a MSYS2 MINGW64 shell)
After cloning the repo, cd into its folder and run:
```
mkdir build
cd build  
"C:\Program Files\CMake\bin\cmake.exe" .. -G "MSYS Makefiles"
make
make install
```
If everything worked as expected, there will be a new directory `build\dist\FeatherPad` containing the compiled .exe and all files it depends on. If 7z.exe was found at the 7-zip default location, there will also be an archive of this folder at `build\dist\FeatherPad-x64-portable.7z`. If makensis.exe was found at the NSIS default location, there will also be an installer for FeatherPad at `build\dist\FeatherPad-x64-setup.exe`.

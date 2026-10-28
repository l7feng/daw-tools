@echo off
cd /d "%~dp0.."

"D:\Ai-Files\Tools\BuildEnv\py312\python.exe" scripts\prepare-validation.py
if errorlevel 1 exit /b 1
call "D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 exit /b 1

"D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build/engine -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTUDIO_BUILD_ENGINE=ON -DCMAKE_MAKE_PROGRAM="D:/Ai-Files/Tools/BuildEnv/vs2022-cpp/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"

if errorlevel 1 exit /b 1

"D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build/engine --target TestRunner -j 4


exit /b %errorlevel%

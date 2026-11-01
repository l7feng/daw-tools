@echo off
chcp 65001 >nul
cd /d "%~dp0.."
call "D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "BACKEND_CMAKE=D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%BACKEND_CMAKE%" -S tests/backend -B build/backend -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="D:/Ai-Files/Tools/BuildEnv/vs2022-cpp/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
if errorlevel 1 exit /b 1
"%BACKEND_CMAKE%" --build build/backend --target BackendProbe StudioTestDelay_VST3 -j 4
exit /b %errorlevel%

@echo off
chcp 65001 >nul
cd /d "%~dp0.."
call "D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "VERIFIED_CMAKE=D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%VERIFIED_CMAKE%" -S . -B build/editor -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTUDIO_BUILD_ENGINE=ON -DCMAKE_MAKE_PROGRAM="D:/Ai-Files/Tools/BuildEnv/vs2022-cpp/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
if errorlevel 1 exit /b 1
"D:\Ai-Files\Tools\BuildEnv\py312\python.exe" -c "from pathlib import Path; p=Path('build/editor/build.ninja'); p.with_name('build-verified.ninja').write_text(p.read_text(encoding='utf-8').replace('Studio Workbench.exe','Studio Workbench Verified.exe'),encoding='utf-8')"
if errorlevel 1 exit /b 1
"D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -C build/editor -f build-verified.ninja StudioWorkbench StudioTestGain_VST3 -j 4
exit /b %errorlevel%

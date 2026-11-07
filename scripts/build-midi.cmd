@echo off
chcp 65001 >nul
cd /d "%~dp0.."
call "D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "MIDI_CMAKE=D:\Ai-Files\Tools\BuildEnv\vs2022-cpp\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%MIDI_CMAKE%" -S tests/backend -B build/backend -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="D:/Ai-Files/Tools/BuildEnv/vs2022-cpp/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
if errorlevel 1 exit /b 1
"%MIDI_CMAKE%" --build build/backend --target StudioMidiFixture_VST3 MidiVerify -j 4
exit /b %errorlevel%

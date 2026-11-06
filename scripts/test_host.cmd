@echo off
setlocal
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not "%errorlevel%"=="0" exit /b 1
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /EHsc /W4 /Iinclude tests\study_policy_test.cpp /Fobuild\study_policy_test.obj /Febuild\study_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\study_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /Itests\stubs /Iinclude tests\study_store_test.cpp src\study_store.cpp /Fobuild\ /Febuild\study_store_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\study_store_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /Iinclude tests\audio_policy_test.cpp /Fobuild\audio_policy_test.obj /Febuild\audio_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\audio_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /utf-8 /Iinclude tests\weather_policy_test.cpp /Fobuild\weather_policy_test.obj /Febuild\weather_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\weather_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /Iinclude tests\network_policy_test.cpp /Fobuild\network_policy_test.obj /Febuild\network_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\network_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /Iinclude tests\prefetch_policy_test.cpp /Fobuild\prefetch_policy_test.obj /Febuild\prefetch_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\prefetch_policy_test.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /utf-8 /Iinclude tests\today_review_test.cpp /Fobuild\today_review_test.obj /Febuild\today_review_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\today_review_test.exe

if not "%errorlevel%"=="0" exit /b 1
cl /nologo /EHsc /W4 /utf-8 /Itests\stubs /Iinclude tests\quiz_game_test.cpp src\quiz_game.cpp src\study_store.cpp /Fobuild\ /Febuild\quiz_game_test.exe
if not "%errorlevel%"=="0" exit /b 1
build\quiz_game_test.exe

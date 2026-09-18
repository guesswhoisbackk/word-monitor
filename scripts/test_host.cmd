@echo off
setlocal
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /EHsc /W4 /Iinclude tests\study_policy_test.cpp /Fobuild\study_policy_test.obj /Febuild\study_policy_test.exe
if errorlevel 1 exit /b 1
build\study_policy_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /Itests\stubs /Iinclude tests\study_store_test.cpp src\study_store.cpp /Fobuild\ /Febuild\study_store_test.exe
if errorlevel 1 exit /b 1
build\study_store_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /Iinclude tests\audio_policy_test.cpp /Fobuild\audio_policy_test.obj /Febuild\audio_policy_test.exe
if errorlevel 1 exit /b 1
build\audio_policy_test.exe

@echo off
setlocal

rem ============================================================
rem BUILD AND RUN THE DEMASTER'S TESTS (X86, NO GL, NO GAME)
rem The pure logic always; the extension API of the built DLL when .build\eden\out\FFVIII_EDEN_DEMASTER.dll exists
rem (tools\build_dll.cmd). Output and scratch files stay under .build\tests of this repository.
rem ============================================================
set "REPOSITORY=%~dp0.."
set "OUTPUT=%~dp0..\.build\tests"
set "DLL=%~dp0..\.build\eden\out\FFVIII_EDEN_DEMASTER.dll"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_vswhere
for /f "usebackq delims=" %%P in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%P"
if not defined VSPATH goto no_vswhere
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x86 -no_logo >nul
if errorlevel 1 goto no_vswhere
if not exist "%OUTPUT%" mkdir "%OUTPUT%"

cl /nologo /std:c++20 /EHsc /W4 /permissive- /MT /Fo"%OUTPUT%\\" /Fe"%OUTPUT%\demaster_logic_tests.exe" "%REPOSITORY%\tests\demaster_logic_tests.cpp" >"%OUTPUT%\build.log" 2>&1
if errorlevel 1 goto build_failed
"%OUTPUT%\demaster_logic_tests.exe" "%OUTPUT%\scratch"
if errorlevel 1 exit /b 1

if not exist "%DLL%" goto no_dll
cl /nologo /std:c++20 /EHsc /W4 /permissive- /MT /Fo"%OUTPUT%\\" /Fe"%OUTPUT%\demaster_extension_tests.exe" "%REPOSITORY%\tests\demaster_extension_tests.cpp" >"%OUTPUT%\build-extension.log" 2>&1
if errorlevel 1 goto build_extension_failed
"%OUTPUT%\demaster_extension_tests.exe" "%DLL%"
exit /b %errorlevel%

:no_dll
echo SKIP extension API: no built DLL at .build\eden\out (tools\build_dll.cmd)
exit /b 0

:build_failed
type "%OUTPUT%\build.log"
echo Build of the logic tests failed.
exit /b 1

:build_extension_failed
type "%OUTPUT%\build-extension.log"
echo Build of the extension API test failed.
exit /b 1

:no_vswhere
echo Visual Studio with the x86 toolchain was not found.
exit /b 1

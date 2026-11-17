@echo off
setlocal

rem ============================================================
rem BUILD THE DEMASTER DLL INTO .build\eden (NEVER INTO THE GAME)
rem Release|Win32 by default; "build_dll.cmd debug" builds Debug|Win32, as the Lunar Cry 4 and standard DLLs were built
rem (docs/LUNARCRY4_DEMASTER_DIFF.md 2.1). Release links vendor\lib\bgfx\*Release.lib (tools\build_bgfx_release.py).
rem The dependencies come from .build\vcpkg_installed through .build\local-build.props; the
rem vcxproj post-build event breaks on the "(x86)" of the path and is skipped.
rem ============================================================
set "REPOSITORY=%~dp0.."
set "CONFIGURATION=Release"
if /I "%~1"=="debug" set "CONFIGURATION=Debug"
set "OUTPUT=%~dp0..\.build\eden"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_msbuild
for /f "usebackq delims=" %%P in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%P"
if not defined MSBUILD goto no_msbuild

"%MSBUILD%" "%REPOSITORY%\ff8_demaster\ff8_demaster.vcxproj" /nologo /m /v:minimal /p:Configuration=%CONFIGURATION% /p:Platform=Win32 ^
  /p:OutDir="%OUTPUT%\out\\" /p:IntDir="%OUTPUT%\obj\\" /p:SolutionDir="%REPOSITORY%\\" ^
  /p:ForceImportBeforeCppTargets="%REPOSITORY%\.build\local-build.props" /p:VcpkgEnableManifest=false /p:VcpkgEnabled=false ^
  /p:PostBuildEventUseInBuild=false
exit /b %errorlevel%

:no_msbuild
echo MSBuild was not found.
exit /b 1

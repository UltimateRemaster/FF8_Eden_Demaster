@echo off
setlocal

rem ============================================================
rem LOCATE THE BOOTSTRAP SOURCES AND LOCAL WINDOWS BUILD TOOLS
rem Compile the standalone entry point without the legacy manager UI.
rem ============================================================
set "BOOTSTRAP_SOURCE_DIRECTORY=%~dp0..\viiidem_customlauncher"
set "BOOTSTRAP_OUTPUT_DIRECTORY=%~dp0..\.dist\EdenBootstrap"
set "BOOTSTRAP_COMPILER=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe"
set "BOOTSTRAP_FRAMEWORK_REFERENCES=%ProgramFiles(x86)%\Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8"

rem ============================================================
rem VALIDATE THE COMPILER AND FRAMEWORK BEFORE CREATING OUTPUT
rem All generated files stay in this development repository.
rem ============================================================
if not exist "%BOOTSTRAP_COMPILER%" (
  echo C# compiler not found: %BOOTSTRAP_COMPILER%
  exit /b 1
)
if not exist "%BOOTSTRAP_FRAMEWORK_REFERENCES%\mscorlib.dll" (
  echo .NET Framework 4.8 reference assemblies not found.
  exit /b 1
)
if not exist "%BOOTSTRAP_OUTPUT_DIRECTORY%" mkdir "%BOOTSTRAP_OUTPUT_DIRECTORY%"
if not exist "%BOOTSTRAP_OUTPUT_DIRECTORY%" exit /b 1

rem ============================================================
rem BUILD THE EXISTING ANYCPU BOOTSTRAP WITH ITS UPDATED IDENTITY
rem The output name supplies the canonical original filename metadata.
rem ============================================================
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 >nul
if errorlevel 1 exit /b 1
rc /nologo /i "%BOOTSTRAP_SOURCE_DIRECTORY%\Properties" /fo "%BOOTSTRAP_OUTPUT_DIRECTORY%\EdenBootstrap.res" "%BOOTSTRAP_SOURCE_DIRECTORY%\Properties\EdenBootstrap.rc"
if errorlevel 1 exit /b 1
"%BOOTSTRAP_COMPILER%" /nologo /noconfig /nostdlib+ /target:winexe /platform:anycpu /optimize+ /deterministic+ /langversion:7.3 /utf8output ^
  /win32res:"%BOOTSTRAP_OUTPUT_DIRECTORY%\EdenBootstrap.res" ^
  /reference:"%BOOTSTRAP_FRAMEWORK_REFERENCES%\mscorlib.dll" ^
  /reference:"%BOOTSTRAP_FRAMEWORK_REFERENCES%\System.dll" ^
  /reference:"%BOOTSTRAP_FRAMEWORK_REFERENCES%\System.Core.dll" ^
  /out:"%BOOTSTRAP_OUTPUT_DIRECTORY%\FFVIII_EDEN_DEMASTER.exe" ^
  "%BOOTSTRAP_SOURCE_DIRECTORY%\Program.cs" ^
  "%BOOTSTRAP_SOURCE_DIRECTORY%\Properties\AssemblyInfo.cs"
if errorlevel 1 exit /b 1

rem ============================================================
rem COPY THE FRAMEWORK CONTRACT BESIDE THE DEVELOPMENT BUILD
rem This script does not deploy into the live game or release directory.
rem ============================================================
copy /Y "%BOOTSTRAP_SOURCE_DIRECTORY%\App.config" "%BOOTSTRAP_OUTPUT_DIRECTORY%\FFVIII_EDEN_DEMASTER.exe.config" >nul
if errorlevel 1 exit /b 1
echo Bootstrap completed: %BOOTSTRAP_OUTPUT_DIRECTORY%\FFVIII_EDEN_DEMASTER.exe
exit /b 0

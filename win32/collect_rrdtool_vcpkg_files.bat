@ echo off
REM This script collects the built .exe and .dll files required for running RRDtool.
REM It is supposed to be run after an MSVC build using nmake and libraries from vcpkg.
REM Wolfgang Stöggl <c72578@yahoo.de>, 2017-2022.

REM Run the batch file with command line parameter x64 or x86
if "%1"=="" (
  echo Command line parameter required: x64 or x86
  echo e.g.: %~nx0 x64
  exit /b
)
echo configuration: %1

REM The script is located in the subdirectory win32
echo %~dp0
pushd %~dp0
SET base_dir=..

REM Read current version of RRDtool
SET /p version=<%base_dir%\VERSION
echo RRDtool version: %version%

SET release_dir=%base_dir%\win32\nmake_release_%1_vcpkg\rrdtool-%version%-%1_vcpkg\
echo release_dir: %release_dir%

if exist %base_dir%\win32\nmake_release_%1_vcpkg rmdir %base_dir%\win32\nmake_release_%1_vcpkg /s /q
mkdir %release_dir%

REM use xcopy instead of copy. xcopy creates directories if necessary and outputs the copied file
REM /Y Suppresses prompting to confirm that you want to overwrite an existing destination file.
REM /D xcopy copies all Source files that are newer than existing Destination files

xcopy /Y /D %base_dir%\win32\librrd-8.dll %release_dir%
xcopy /Y /D %base_dir%\win32\rrdcgi.exe %release_dir%
xcopy /Y /D %base_dir%\win32\rrdtool.exe %release_dir%
xcopy /Y /D %base_dir%\win32\rrdupdate.exe %release_dir%

REM The following part needs to be checked and maintained after an update to a new vcpkg version
REM Names of dlls can change over time, which has happened in the past
REM e.g. glib-2.dll -> glib-2.0-0.dll, expat.dll -> libexpat.dll
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\brotlicommon.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\brotlidec.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\bz2.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\cairo-2.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\libexpat.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\libffi.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\fontconfig-1.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\freetype.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\fribidi-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\gio-2.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\glib-2.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\gmodule-2.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\gobject-2.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\harfbuzz.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\iconv-2.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\intl-8.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\libpng16.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\libxml2.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\liblzma.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\pango-1.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\pangocairo-1.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\pangoft2-1.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\pangowin32-1.0-0.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\pcre.dll %release_dir%
xcopy /Y /D %base_dir%\vcpkg\installed\%1-windows\bin\zlib1.dll %release_dir%

popd

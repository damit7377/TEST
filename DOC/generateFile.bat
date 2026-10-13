@echo on
setlocal

cd /d "%~dp0"

set "SREC_CAT=%CD%\srec_cat.exe"
set "BOOT_BIN=%CD%\..\STM32CubeIDE\Boot\Debug\H7R_70D5_Boot.bin"
set "APPLI_HEX=%CD%\..\STM32CubeIDE\Appli\Debug\H7R_70D5_Appli.hex"
set "APPLI_BIN=%CD%\APPLI.BIN"
set "RESORCE_BIN=%CD%\RESORCE.BIN"
set "OUTPUT_HEX=%CD%\H7R_70D5_All.hex"

set "BOOT_BASE=0x08000000"
set "APPLI_BASE=0x70000000"
set "APPLI_END=0x70100000"
set "RESORCE_BASE=0x70100000"
set "RESORCE_END=0x72000000"

if not exist "%SREC_CAT%" (
  echo ERROR: srec_cat.exe not found: "%SREC_CAT%"
  exit /b 1
)

if not exist "%BOOT_BIN%" (
  echo ERROR: Boot binary not found: "%BOOT_BIN%"
  echo Please build the Boot project first.
  exit /b 1
)

if not exist "%APPLI_HEX%" (
  echo ERROR: Appli hex not found: "%APPLI_HEX%"
  echo Please build the Appli project first.
  exit /b 1
)

if exist "%APPLI_BIN%" del "%APPLI_BIN%"
if exist "%RESORCE_BIN%" del "%RESORCE_BIN%"
if exist "%OUTPUT_HEX%" del "%OUTPUT_HEX%"

"%SREC_CAT%" "%APPLI_HEX%" -Intel ^
  -crop %APPLI_BASE% %APPLI_END% ^
  -offset -%APPLI_BASE% ^
  -o "%APPLI_BIN%" -binary
if errorlevel 1 exit /b 1

"%SREC_CAT%" "%APPLI_HEX%" -Intel ^
  -crop %RESORCE_BASE% %RESORCE_END% ^
  -offset -%RESORCE_BASE% ^
  -o "%RESORCE_BIN%" -binary
if errorlevel 1 exit /b 1

"%SREC_CAT%" "%BOOT_BIN%" -binary -offset %BOOT_BASE% ^
  "%APPLI_BIN%" -binary -offset %APPLI_BASE% ^
  "%RESORCE_BIN%" -binary -offset %RESORCE_BASE% ^
  -o "%OUTPUT_HEX%" -Intel
if errorlevel 1 exit /b 1

echo Generated files:
echo   "%APPLI_BIN%"
echo   "%RESORCE_BIN%"
echo   "%OUTPUT_HEX%"

endlocal

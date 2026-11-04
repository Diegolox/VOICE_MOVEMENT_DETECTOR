@echo off
setlocal
set "ARDUINO_TOOLS=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources"
if not exist "%ARDUINO_TOOLS%\arduino-cli.exe" (
    echo No se encuentra Arduino CLI dentro de Arduino IDE.
    pause
    exit /b 1
)
set "PATH=%ARDUINO_TOOLS%;%PATH%"
echo Cierra Arduino IDE y las conexiones USB de Edge Impulse.
echo Conecta la placa y pulsa RESET dos veces antes de continuar.
pause
call "%~dp0flash_windows.bat"
set "FLASH_RESULT=%ERRORLEVEL%"
echo.
echo Resultado del cargador: %FLASH_RESULT%
pause
exit /b %FLASH_RESULT%

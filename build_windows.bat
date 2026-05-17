@echo off
setlocal

set BUILD_DIR=build

cmake -S . -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build %BUILD_DIR% --config Release
if errorlevel 1 exit /b 1

echo.
echo Built executable:
echo   %BUILD_DIR%\dist\checkpoint.exe

@echo off
:: ============================================================
:: AdaSpace3D - Build Server Launcher
:: ============================================================

title AdaSpace3D Build Server

cd /d "%~dp0"

:: Check if Node.js is installed
where node >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ============================================================
    echo ERROR: Node.js not found!
    echo ============================================================
    echo.
    echo Node.js is required to run the build server.
    echo.
    echo Please install Node.js from: https://nodejs.org
    echo.
    echo Recommended: Download the LTS version
    echo.
    pause
    exit /b 1
)

echo ============================================================
echo AdaSpace3D Build Server Setup
echo ============================================================
echo.

:: Check if node_modules exists
if not exist "node_modules\" (
    echo Installing dependencies...
    echo This may take a minute on first run.
    echo.
    call npm install
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo ============================================================
        echo ERROR: Failed to install dependencies
        echo ============================================================
        pause
        exit /b 1
    )
    echo.
    echo Dependencies installed successfully!
    echo.
)

:: Start the server
echo ============================================================
echo Starting Build Server...
echo ============================================================
echo.
node build-server.js

:: If server exits, pause
echo.
echo ============================================================
echo Server stopped
echo ============================================================
pause

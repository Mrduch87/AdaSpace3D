@echo off
setlocal
title AdaSpace3D Launcher

echo ============================================================
echo   AdaSpace3D - One-Click Launcher
echo ============================================================
echo.

:: 1. Check Node.js
where node >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [WARN] Node.js is NOT installed!
    echo.
    echo [INFO] Downloading Node.js 18 LTS installer...
    
    :: Download MSI using PowerShell
    powershell -Command "Invoke-WebRequest -Uri 'https://nodejs.org/dist/v18.17.1/node-v18.17.1-x64.msi' -OutFile 'node_installer.msi'"
    
    if exist node_installer.msi (
        echo [INFO] Installing Node.js...
        echo        (Please click "Yes" if asked for Admin permission)
        
        :: Run Installer as Admin
        powershell -Command "Start-Process msiexec.exe -ArgumentList '/i node_installer.msi /passive /norestart' -Verb RunAs -Wait"
        
        echo.
        echo [SUCCESS] Node.js installed!
        echo.
        echo *** IMPORTANT ***
        echo Please CLOSE this window and run RUN_ME.bat again to finish setup.
        echo (This is needed to update system paths)
        echo.
        del node_installer.msi
        pause
        exit /b 0
    ) else (
        echo [ERROR] Failed to download installer.
        echo Please install Node.js manually from: https://nodejs.org/
        pause
        exit /b 1
    )
)

:: 2. Check Dependencies
if not exist "node_modules\" (
    echo [INFO] First run detected. Installing libraries...
    call npm install
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Install failed. Check internet connection.
        pause
        exit /b 1
    )
    echo [OK] Libraries installed.
)

:: 3. Start Server (Minimize window to background)
echo [INFO] Starting Local Build Server...
start "AdaSpace3D Server" /MIN cmd /c "node build-server.js"

:: 4. Wait for server to be ready (approx 2 sec)
echo [INFO] Waiting for server...
timeout /t 2 /nobreak >nul

:: 5. Open Dashboard
echo [INFO] Opening Configurator...
start configurator.html

echo.
echo [SUCCESS] System is running!
echo.
echo  - The Dashboard is open in your browser.
echo  - The Server is running in the background (minimized).
echo.
echo To close everything, just close the browser and the server window.
echo.
pause

@echo off
echo Building ECDIS AUV with Visualization Components...
echo.

REM Check if qmake exists
where qmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: qmake not found. Please ensure Qt is installed and in PATH.
    pause
    exit /b 1
)

REM Clean previous builds
echo Cleaning previous builds...
if exist "Makefile" del Makefile
if exist "*.obj" del *.obj
if exist "debug" rmdir /s /q debug
if exist "release" rmdir /s /q release

REM Generate Makefile
echo Generating Makefile...
qmake ecdis.pro CONFIG+=debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to generate Makefile.
    pause
    exit /b 1
)

REM Build the project
echo Building project...
nmake
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed.
    echo.
    echo Common issues:
    echo 1. Missing Qt development files
    echo 2. Missing SevenCs EC2007 SDK
    echo 3. Path issues with headers/libraries
    echo.
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.
echo Run debug\ecdis.exe to test the visualization features.
echo.
echo New Features Added:
echo - Ocean Current Arrows (View menu)
echo - Tide Rectangle Visualization (View menu)
echo - Visualization Control Panel (View -> UI Panels)
echo.
pause
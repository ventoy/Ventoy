@echo off
chcp 65001
echo ==============================================
echo          VS Build Script
echo ==============================================

:: ==========================================================
set "VS_DEV_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
set "SLN_FILE=Ventoy2Disk.sln"
:: ==========================================================

echo Initialize environment ...
call "%VS_DEV_CMD%" -no_logo
if %errorlevel% neq 0 (
    echo Error: VS env error.
    pause
    exit /b 1
)


:: ============== Build Release Win32 (x86) ==============
echo.
echo Building: Release  Win32
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=Win32 /m
if %errorlevel% neq 0 (
    echo Build Release Win32 failed!
    pause
    exit /b 1
)


:: ============== Build Release x64 ==============
echo.
echo Building: Release  x64
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=x64 /m
if %errorlevel% neq 0 (
    echo Build Release x64 failed!
    pause
    exit /b 1
)


:: ============== Build Release ARM ==============
echo.
echo Building: Release  ARM
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=ARM /m
if %errorlevel% neq 0 (
    echo Build Release ARM failed!
    pause
    exit /b 1
)


:: ============== Build Release ARM64 ==============
echo.
echo Building: Release  ARM64
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=ARM64 /m
if %errorlevel% neq 0 (
    echo Build Release ARM64 failed!
    pause
    exit /b 1
)


echo.
echo ==============================================
echo              ✅ Build Success
echo ==============================================
pause

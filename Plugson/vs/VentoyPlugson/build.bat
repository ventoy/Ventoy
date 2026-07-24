@echo off
echo ==============================================
echo          VS Build Script
echo ==============================================

set "VS_DEV_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
set "SLN_FILE=VentoyPlugson.sln"


echo Init VS environment ...
call "%VS_DEV_CMD%" -no_logo
if %errorlevel% neq 0 (
    echo Error: VS environment init failed, please check.
    pause
    exit /b 1
)

:: ============== Build Release Win32 (x86) ==============
echo.
echo Build: Release  Win32
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=Win32 /m
if %errorlevel% neq 0 (
    echo Build Release Win32 Failed!
    pause
    exit /b 1
)


:: ============== Build Release x64 ==============
echo.
echo Build: Release  x64
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=x64 /m
if %errorlevel% neq 0 (
    echo Build Release x64 Failed!
    pause
    exit /b 1
)

echo.
echo ==============================================
echo              Build Success
echo ==============================================
pause

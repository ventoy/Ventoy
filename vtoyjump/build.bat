@echo off
chcp 65001
echo ==============================================
echo          VS 命令行双架构编译脚本
echo ==============================================

:: ====================== 【必须改这里】======================
:: 1. 你的 VS 版本对应的 VsDevCmd.bat 路径（看下面的路径对照表）
set "VS_DEV_CMD=%VS120COMNTOOLS%VsDevCmd.bat"
:: 2. 你的 VS 解决方案文件名（.sln）
set "SLN_FILE=vtoyjump.sln"
:: ==========================================================

:: 初始化VS编译环境（必须第一步）
echo 正在初始化 VS 编译环境...
call "%VS_DEV_CMD%" -no_logo
if %errorlevel% neq 0 (
    echo 错误：VS环境初始化失败！检查路径是否正确
    pause
    exit /b 1
)

:: ============== 编译 Release Win32 (x86) ==============
echo.
echo 正在编译：Release  Win32
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=Win32 /m
if %errorlevel% neq 0 (
    echo 编译 Release Win32 失败！
    pause
    exit /b 1
)


:: ============== 编译 Release x64 ==============
echo.
echo 正在编译：Release  x64
MSBuild "%SLN_FILE%" /t:Build /p:Configuration=Release;Platform=x64 /m
if %errorlevel% neq 0 (
    echo 编译 Release x64 失败！
    pause
    exit /b 1
)

del ..\INSTALL\ventoy\vtoyjump32.exe
del ..\INSTALL\ventoy\vtoyjump64.exe

copy Release\vtoyjump32.exe         ..\INSTALL\ventoy\vtoyjump32.exe
copy x64\Release\vtoyjump64.exe     ..\INSTALL\ventoy\vtoyjump64.exe


echo.
echo ==============================================
echo              ✅ 2架构编译完成！
echo ==============================================
echo.
pause



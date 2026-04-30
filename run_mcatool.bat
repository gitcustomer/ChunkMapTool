@echo off
cd /d "%~dp0"
echo Starting MCATool...
echo.

REM 检查可执行文件是否存在
if not exist "cmake-build-debug\bin\Debug\mcatool.exe" (
    echo Error: mcatool.exe not found!
    echo Please build the project first.
    pause
    exit /b 1
)

REM 复制必需的 DLL 文件
echo Copying required DLL files...
xcopy /Y /Q "C:\dev\vcpkg\installed\x64-windows\bin\*.dll" "cmake-build-debug\bin\Debug\" >nul 2>&1
xcopy /Y /Q "C:\dev\vcpkg\installed\x64-windows\debug\bin\*.dll" "cmake-build-debug\bin\Debug\" >nul 2>&1

REM 启动程序
echo Starting MCATool...
start "" "cmake-build-debug\bin\Debug\mcatool.exe"
echo MCATool started successfully!
echo.

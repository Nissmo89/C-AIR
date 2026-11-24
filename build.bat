@echo off
setlocal
set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%"
if errorlevel 1 exit /b 1

set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "RAVEN_SOURCE_DIR=%PROJECT_ROOT%third_party\Raven"
set "RAVEN_BUILD_DIR=%RAVEN_SOURCE_DIR%\build-cair"
set "RAVEN_INSTALL_DIR=%PROJECT_ROOT%build\raven-install"
if not exist "%RAVEN_SOURCE_DIR%\CMakeLists.txt" (
	echo Raven source checkout was not found at "%RAVEN_SOURCE_DIR%".
	exit /b 1
)

rem Always produce a single, predictable executable: build\bin\C-AIR.exe.
set "CAIR_OUTPUT_NAME=C-AIR"

rem Remove everything under .\build except the Raven install so stale binaries
rem from earlier builds can never be picked up by mistake. The executables are
rem deleted first so a locked binary is reported by name instead of hiding behind
rem the directory it lives in. A running C-AIR.exe keeps its file locked.
powershell -NoProfile -Command "$locked = @(); if (Test-Path -LiteralPath '.\build\bin') { Get-ChildItem -LiteralPath '.\build\bin' -Force -File | ForEach-Object { $path = $_.FullName; try { Remove-Item -LiteralPath $path -Force -ErrorAction Stop } catch { $locked += $path } } }; if ($locked.Count -gt 0) { $locked | ForEach-Object { Write-Host ('Could not delete ' + $_) }; exit 1 }"
if errorlevel 1 (
	echo Close every running C-AIR window and retry.
	exit /b 1
)

powershell -NoProfile -Command "$locked = @(); if (Test-Path -LiteralPath '.\build') { Get-ChildItem -LiteralPath '.\build' -Force | Where-Object { $_.Name -ne 'raven-install' } | ForEach-Object { $path = $_.FullName; try { Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop } catch { $locked += $path } } }; if ($locked.Count -gt 0) { $locked | ForEach-Object { Write-Host ('Could not delete ' + $_) }; exit 1 }"
if errorlevel 1 (
	echo Could not clean .\build\. Close every running C-AIR window and retry.
	exit /b 1
)

cmake -S "%RAVEN_SOURCE_DIR%" -B "%RAVEN_BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%RAVEN_INSTALL_DIR%" -DRAVEN_BUILD_EXAMPLES=OFF -DRAVEN_BUILD_TESTS=OFF -DRAVEN_BUILD_TOOLS=OFF -DRAVEN_BUILD_WEB=OFF -DRAVEN_BUILD_INSTALLER=OFF
if errorlevel 1 exit /b 1

cmake --build "%RAVEN_BUILD_DIR%" --target raven --parallel
if errorlevel 1 exit /b 1

cmake --install "%RAVEN_BUILD_DIR%" --prefix "%RAVEN_INSTALL_DIR%"
if errorlevel 1 exit /b 1

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%RAVEN_INSTALL_DIR%" -DCAIR_OUTPUT_NAME=%CAIR_OUTPUT_NAME%
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

ctest --test-dir build --output-on-failure
if errorlevel 1 exit /b 1

echo Build and tests succeeded! Application: build\bin\%CAIR_OUTPUT_NAME%.exe

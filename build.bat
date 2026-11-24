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

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss-fff"') do set "CAIR_OUTPUT_NAME=cair-%%I"
if not defined CAIR_OUTPUT_NAME exit /b 1

powershell -NoProfile -Command "if (Test-Path -LiteralPath '.\build') { Get-ChildItem -Path '.\build' -Exclude 'raven-install' | ForEach-Object { try { Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction Stop } catch {} } }"
if errorlevel 1 (
	echo Could not remove .\build\. Close any running C-AIR editor and retry.
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

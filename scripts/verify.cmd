@echo off
rem ---------------------------------------------------------------------------
rem NxtCut one-command check. Run from the "x64 Native Tools Command Prompt for VS":
rem     scripts\verify.cmd
rem Runs configure, build, tests, architecture check and the clang-format check
rem (the same format check CI runs). Logs go to the git-ignored build folder.
rem ---------------------------------------------------------------------------
setlocal
cd /d "%~dp0.."
if not exist build mkdir build

echo [1/5] Configure
cmake --preset windows-debug > build\configure_log.txt 2>&1
if errorlevel 1 (
    echo CONFIGURE FAILED - see build\configure_log.txt
    exit /b 1
)

echo [2/5] Build
cmake --build --preset windows-debug -- -k 0 > build\build_log.txt 2>&1
set BUILD_RC=%errorlevel%

echo [3/5] Tests
ctest --preset windows-debug --output-on-failure > build\test_log.txt 2>&1
set TEST_RC=%errorlevel%

echo [4/5] Architecture check
python scripts\check_architecture.py > build\arch_log.txt 2>&1
set ARCH_RC=%errorlevel%

echo [5/5] Format check
powershell -command "git ls-files --cached --others --exclude-standard '*.cpp' '*.hpp' | ForEach-Object { clang-format --dry-run -Werror $_ }" > build\format_log.txt 2>&1
set FMT_SIZE=0
for %%A in (build\format_log.txt) do set FMT_SIZE=%%~zA

echo.
echo ======================= SUMMARY =======================
if "%BUILD_RC%"=="0" (echo Build:        OK) else (echo Build:        FAILED)
if "%TEST_RC%"=="0" (echo Tests:        OK) else (echo Tests:        FAILED)
if "%ARCH_RC%"=="0" (echo Architecture: OK) else (echo Architecture: FAILED)
if "%FMT_SIZE%"=="0" (echo Format:       OK) else (echo Format:       FAILED - run clang-format, see build\format_log.txt)
echo -------------------------------------------------------
powershell -command "Get-Content build\test_log.txt | Select-String -Pattern 'tests passed|tests failed' | ForEach-Object { $_.Line }"
echo First build errors, if any:
powershell -command "Select-String -Path build\build_log.txt -Pattern 'error C[0-9]+|error:|FAILED:|warning C[0-9]+|LNK[0-9]+' | Select-Object -First 10 | ForEach-Object { $_.Line }"
echo =======================================================

if not "%BUILD_RC%"=="0" exit /b 1
if not "%TEST_RC%"=="0" exit /b 1
if not "%ARCH_RC%"=="0" exit /b 1
if not "%FMT_SIZE%"=="0" exit /b 1
exit /b 0

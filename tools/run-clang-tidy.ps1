Param(
    [switch]$Fix,
    [string]$BuildDir = "build/clang-tidy",
    [int]$Jobs = 4
)

# Simple helper to run clang-tidy on the engine/ folder on Windows
# Usage:
#   .\tools\run-clang-tidy.ps1          # dry-run (no --fix)
#   .\tools\run-clang-tidy.ps1 -Fix     # apply fixes where clang-tidy deems safe
# Notes:
# - Requires clang-tidy in PATH.
# - Attempts to generate a compile_commands.json by configuring CMake with the Ninja generator
#   if one is not present. On Windows with MSVC generator CMake does not produce a compile
#   commands database.

function ErrorExit($msg) {
    Write-Error $msg
    exit 1
}

# Check for clang-tidy
if (-not (Get-Command clang-tidy -ErrorAction SilentlyContinue)) {
    ErrorExit "clang-tidy not found in PATH. Install LLVM/Clang and ensure clang-tidy is on PATH.
See https://llvm.org/builds/ or use Visual Studio's LLVM component."
}

# Ensure build dir exists
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$compileDb = Join-Path (Resolve-Path $BuildDir) "compile_commands.json"

if (-not (Test-Path $compileDb)) {
    Write-Host "compile_commands.json not found in $BuildDir. Attempting to generate using Ninja..."
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        ErrorExit "cmake is not available on PATH. Please install CMake."
    }
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        ErrorExit "Ninja is not available. Install Ninja or run CMake with a generator that exports compile_commands.json.\nTry: choco install ninja or install from https://ninja-build.org/."
    }

    Write-Host "Configuring CMake (Ninja) to export compile_commands.json into $BuildDir..."
    & cmake -S . -B $BuildDir -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if ($LASTEXITCODE -ne 0) {
        ErrorExit "CMake configuration failed (exit $LASTEXITCODE)."
    }

    if (-not (Test-Path $compileDb)) {
        ErrorExit "Failed to generate compile_commands.json in $BuildDir."
    }
}

# Find source files under engine/
$files = Get-ChildItem -Path "engine" -Recurse -Include *.cpp,*.c,*.h | ForEach-Object { $_.FullName }
if ($files.Count -eq 0) { ErrorExit "No source files found under engine/." }

# Build clang-tidy args (omit -j on Windows clang-tidy builds that don't support it)
$tidyArgs = @("-p", (Resolve-Path $BuildDir).Path)
if ($Fix.IsPresent) { $tidyArgs += "--fix" }

# Add files
$tidyArgs += $files

Write-Host "Running clang-tidy with: clang-tidy $($tidyArgs -join ' ')"
& clang-tidy @tidyArgs

if ($LASTEXITCODE -ne 0) {
    Write-Warning "clang-tidy returned exit code $LASTEXITCODE. Check output above for details."
} else {
    Write-Host "clang-tidy finished successfully."
}

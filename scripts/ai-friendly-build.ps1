# AI-Friendly Build Script
# Provides immediate feedback and clear status messages for AI assistants

Write-Host "=== AI-Friendly Build Script ===" -ForegroundColor Cyan
Write-Host "Starting build process..." -ForegroundColor Yellow

# Check if build directory exists
if (-not (Test-Path "build")) {
    Write-Host "Build directory not found, running configure first..." -ForegroundColor Yellow
    cmake --preset default
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CONFIGURE FAILED!" -ForegroundColor Red
        exit 1
    }
    Write-Host "Configure completed successfully!" -ForegroundColor Green
}

# Start build with progress indication
Write-Host "Building project..." -ForegroundColor Yellow
$buildStart = Get-Date

# Run the build
cmake --build build --config Debug --verbose

# Check result
if ($LASTEXITCODE -eq 0) {
    $buildEnd = Get-Date
    $duration = $buildEnd - $buildStart
    Write-Host "BUILD SUCCESS!" -ForegroundColor Green
    Write-Host "Build completed in $($duration.TotalSeconds.ToString('F1')) seconds" -ForegroundColor Green
    
    # Check if executable exists
    if (Test-Path "build\bin\MMORPGEngine.exe") {
        Write-Host "Executable created: build\bin\MMORPGEngine.exe" -ForegroundColor Green
    } else {
        Write-Host "WARNING: Executable not found!" -ForegroundColor Yellow
    }
} else {
    Write-Host "BUILD FAILED!" -ForegroundColor Red
    Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "=== Build Process Complete ===" -ForegroundColor Cyan

# Force rebuild - kills all instances and does a clean build
Write-Host "=== Force Rebuild ===" -ForegroundColor Cyan

# Kill ALL instances
Write-Host "Killing all MMORPGEngine instances..." -ForegroundColor Yellow
Get-Process -Name "MMORPGEngine" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Clean build directory
if (Test-Path "build/bin/MMORPGEngine.exe") {
    Write-Host "Removing old executable..." -ForegroundColor Yellow
    Remove-Item "build/bin/MMORPGEngine.exe" -Force -ErrorAction SilentlyContinue
}

# Wait for file system
Start-Sleep -Milliseconds 500

# Build
Write-Host "Building..." -ForegroundColor Yellow
cmake --build build --config Debug

if ($LASTEXITCODE -eq 0) {
    Write-Host "BUILD SUCCESS!" -ForegroundColor Green
    Write-Host "Ready to run" -ForegroundColor Green
} else {
    Write-Host "BUILD FAILED!" -ForegroundColor Red
    exit 1
}

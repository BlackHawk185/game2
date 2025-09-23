# Complete MMORPG Engine Setup + Build Script
# Run this after installing development tools

param(
    [string]$RepoUrl = "https://github.com/your-username/game2.git",
    [string]$ProjectDir = "game2"
)

Write-Host "=== Complete MMORPG Engine Setup ===" -ForegroundColor Cyan

# Step 1: Clone repository
if (-not (Test-Path $ProjectDir)) {
    Write-Host "📥 Cloning repository..." -ForegroundColor Green
    git clone $RepoUrl $ProjectDir
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Failed to clone repository" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "📁 Project directory already exists" -ForegroundColor Yellow
}

Set-Location $ProjectDir

# Step 2: Initialize submodules
Write-Host "📦 Initializing submodules..." -ForegroundColor Green
git submodule update --init --recursive

# Step 3: Configure project
Write-Host "⚙️  Configuring project..." -ForegroundColor Green
cmake --preset default
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ CMake configuration failed" -ForegroundColor Red
    exit 1
}

# Step 4: Build project
Write-Host "🔨 Building project..." -ForegroundColor Green
cmake --build build
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed" -ForegroundColor Red
    exit 1
}

# Step 5: Test run
Write-Host "🚀 Testing executable..." -ForegroundColor Green
if (Test-Path "build/bin/MMORPGEngine.exe") {
    Write-Host "✅ Build successful! Executable created." -ForegroundColor Green
    Write-Host "🎮 To run: .\build\bin\MMORPGEngine.exe" -ForegroundColor Cyan
} else {
    Write-Host "❌ Executable not found" -ForegroundColor Red
    exit 1
}

Write-Host "`n🎉 Setup complete! Ready for development." -ForegroundColor Green
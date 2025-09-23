# MMORPG Engine Development Setup Script
# Run this script as Administrator for best results

Write-Host "=== MMORPG Engine Development Setup ===" -ForegroundColor Cyan
Write-Host "Installing required development tools..." -ForegroundColor Yellow

# Check if running as admin (recommended for winget)
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
if (-not $isAdmin) {
    Write-Host "WARNING: Not running as Administrator. Some installations may fail." -ForegroundColor Red
    Write-Host "Consider running: PowerShell -Command 'Start-Process PowerShell -Verb RunAs'" -ForegroundColor Yellow
}

# Install core development tools
$tools = @(
    @{Name = "Git.Git"; Description = "Git version control"},
    @{Name = "Kitware.CMake"; Description = "CMake build system"},
    @{Name = "Ninja-build.Ninja"; Description = "Ninja build tool"},
    @{Name = "LLVM.LLVM"; Description = "LLVM/Clang compiler"}
)

foreach ($tool in $tools) {
    Write-Host "Installing $($tool.Description)..." -ForegroundColor Green
    try {
        winget install $tool.Name --accept-package-agreements --accept-source-agreements
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ $($tool.Description) installed successfully" -ForegroundColor Green
        } else {
            Write-Host "⚠️  $($tool.Description) installation may have failed (exit code: $LASTEXITCODE)" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "❌ Failed to install $($tool.Description): $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Optional: Visual Studio Code
Write-Host "`nOptional: Installing Visual Studio Code..." -ForegroundColor Cyan
try {
    winget install Microsoft.VisualStudioCode --accept-package-agreements --accept-source-agreements
} catch {
    Write-Host "VS Code installation failed, but this is optional" -ForegroundColor Yellow
}

Write-Host "`n=== Installation Complete ===" -ForegroundColor Cyan
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Restart your terminal/PowerShell" -ForegroundColor White
Write-Host "2. Clone the repository: git clone <repository-url>" -ForegroundColor White
Write-Host "3. cd game2" -ForegroundColor White
Write-Host "4. git submodule update --init --recursive" -ForegroundColor White
Write-Host "5. cmake --preset default" -ForegroundColor White
Write-Host "6. cmake --build build" -ForegroundColor White
Write-Host "7. .\build\bin\MMORPGEngine.exe" -ForegroundColor White

Write-Host "`nIf you encounter issues, verify tools are in PATH by restarting terminal" -ForegroundColor Yellow
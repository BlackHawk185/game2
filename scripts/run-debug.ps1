$ErrorActionPreference='Stop'
$exe = 'build/review/bin/Debug/MMORPGEngine.exe'
if (!(Test-Path $exe)) { & "$PSScriptRoot/build-debug.ps1" }
Start-Process -FilePath $exe

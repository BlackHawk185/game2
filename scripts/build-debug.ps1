param([switch]$Run)
$ErrorActionPreference='Stop'
$buildDir = 'build/review'
if (!(Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
cmake -S . -B $buildDir -DCMAKE_BUILD_TYPE=Debug
cmake --build $buildDir --config Debug -j 10
if ($Run) { Start-Process -FilePath "$buildDir/bin/Debug/MMORPGEngine.exe" }

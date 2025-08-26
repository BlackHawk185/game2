Running clang-tidy on Windows (quick guide)

1) Install LLVM/Clang (which includes clang-tidy):
   - Download official LLVM installer for Windows: https://github.com/llvm/llvm-project/releases
   - Or use Chocolatey: choco install llvm
   - Ensure the LLVM bin directory (which contains clang-tidy.exe) is on your PATH.

2) Install Ninja (optional, recommended to generate a compile database):
   - choco install ninja

3) Run the helper script from the repo root (PowerShell):
   - .\tools\run-clang-tidy.ps1        # run checks only
   - .\tools\run-clang-tidy.ps1 -Fix   # apply automatic fixes where clang-tidy deems safe

Notes:
- The script will attempt to configure CMake with the Ninja generator to produce a compile_commands.json
  if one is not already present in the provided build directory.
- On Windows with MSVC and the Visual Studio generator, CMake does not export a compile database by
  default; using the Ninja generator with clang toolchain is the simplest way to get a usable
  compile_commands.json for clang-tidy.

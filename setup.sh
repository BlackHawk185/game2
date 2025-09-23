#!/bin/bash
# MMORPG Engine Development Setup Script
# Works on Linux, macOS, and Windows (with Git Bash/WSL)

set -e

echo "=== MMORPG Engine Development Setup ==="
echo "Detecting platform and installing required tools..."

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="windows"
else
    echo "❌ Unsupported platform: $OSTYPE"
    exit 1
fi

echo "📋 Detected platform: $PLATFORM"

# Install tools based on platform
case $PLATFORM in
    "linux")
        echo "🐧 Installing on Linux..."
        # Detect package manager
        if command -v apt &> /dev/null; then
            sudo apt update
            sudo apt install -y cmake ninja-build clang git build-essential
        elif command -v yum &> /dev/null; then
            sudo yum install -y cmake ninja-build clang git gcc-c++
        elif command -v pacman &> /dev/null; then
            sudo pacman -S --noconfirm cmake ninja clang git base-devel
        else
            echo "❌ Unsupported Linux package manager"
            exit 1
        fi
        ;;
    "macos")
        echo "🍎 Installing on macOS..."
        # Check if Homebrew is installed
        if ! command -v brew &> /dev/null; then
            echo "Installing Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        brew install cmake ninja llvm git
        ;;
    "windows")
        echo "🪟 On Windows - please use the PowerShell script instead:"
        echo "   PowerShell -ExecutionPolicy Bypass -File setup.ps1"
        echo ""
        echo "Or install manually via winget:"
        echo "   winget install Git.Git Kitware.CMake Ninja-build.Ninja LLVM.LLVM"
        exit 1
        ;;
esac

echo ""
echo "✅ Installation complete!"
echo ""
echo "📋 Next steps:"
echo "1. Clone the repository: git clone <repository-url>"
echo "2. cd game2"
echo "3. git submodule update --init --recursive"
echo "4. cmake --preset default"
echo "5. cmake --build build"
echo "6. ./build/bin/MMORPGEngine"
echo ""
echo "🔧 For VS Code users, install these extensions:"
echo "   - C/C++ (ms-vscode.cpptools)"
echo "   - CMake Tools (ms-vscode.cmake-tools)"
echo "   - CMake (twxs.cmake)"
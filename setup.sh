#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "======================================================"
echo "          gem5 Dependency Installation Script         "
echo "======================================================"

# 1. Check for root privileges
if [ "$EUID" -ne 0 ]; then
  echo "❌ Error: Please run this script as root or using sudo."
  echo "Usage: sudo ./setup.sh"
  exit 1
fi

# 2. Detect OS and Version
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
else
    echo "❌ Error: Cannot detect OS. This script requires Ubuntu 22.04 or 24.04."
    exit 1
fi

if [[ "$OS" != "Ubuntu" ]]; then
    echo "❌ Error: This script is designed for Ubuntu. Detected: $OS"
    exit 1
fi

echo "✅ Detected OS: $OS $VER"
echo "🔄 Updating package lists..."
apt-get update -yqq

# 3. Install packages based on Ubuntu Version
if [[ "$VER" == "24.04" ]]; then
    echo "📦 Installing dependencies for Ubuntu 24.04..."
    apt-get install -y build-essential scons python3-dev git pre-commit zlib1g zlib1g-dev \
        libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
        libboost-all-dev libhdf5-serial-dev python3-pydot python3-venv python3-tk mypy \
        m4 libcapstone-dev libpng-dev libelf-dev pkg-config wget cmake doxygen clang-format

elif [[ "$VER" == "22.04" ]]; then
    echo "📦 Installing dependencies for Ubuntu 22.04..."
    apt-get install -y build-essential git m4 scons zlib1g zlib1g-dev \
        libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
        python3-dev libboost-all-dev pkg-config python3-tk clang-format-15

    echo "⚙️ Configuring clang-format-15 as the system default..."
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-15 150 \
        --slave /usr/bin/clang-format-diff clang-format-diff /usr/bin/clang-format-diff-15 \
        --slave /usr/bin/git-clang-format git-clang-format /usr/bin/git-clang-format-15
        
    # Set it to auto mode so it defaults to the one we just installed
    update-alternatives --auto clang-format
else
    echo "⚠️ Warning: Detected Ubuntu version $VER. gem5 officially tests on 22.04 and 24.04."
    echo "Attempting to install 22.04 baseline dependencies as a fallback..."
    
    apt-get install -y build-essential git m4 scons zlib1g zlib1g-dev \
        libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
        python3-dev libboost-all-dev pkg-config python3-tk
fi

# 4. Verification Check
echo ""
echo "======================================================"
echo "✅ Installation Complete! Verifying core dependencies:"
echo "======================================================"

# Helper function to check if command exists and print version
check_version() {
    if command -v $1 &> /dev/null; then
        echo -n "🟢 $1: "
        $1 --version | head -n 1
    else
        echo "🔴 $1: NOT FOUND"
    fi
}

check_version gcc
check_version g++
check_version python3
check_version scons
check_version git

echo "======================================================"
echo "You are now ready to build gem5!"
echo "Example build command:"
echo "  scons build/X86/gem5.opt -j \$(nproc)"
echo "======================================================"
#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "==> CONAN 2 DEPENDENCY INSTALLATION <=="
echo ""

# Detect default profile if not exists
if ! conan profile show &>/dev/null; then
    echo "Detecting Conan profile..."
    conan profile detect --force
fi

# Clean previous build artifacts
mkdir -p build

# Install dependencies with Conan 2
# Use Debug by default, pass 'Release' as argument for release build
BUILD_TYPE="${1:-Debug}"
echo "Installing dependencies for $BUILD_TYPE build..."

conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type="$BUILD_TYPE"

echo ""
echo "==> DEPENDENCIES INSTALLED <=="
echo ""
echo "Next steps:"
echo "  cmake --preset conan-debug"
echo "  cmake --build build"

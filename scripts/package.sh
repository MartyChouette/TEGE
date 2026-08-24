#!/bin/bash
set -e
echo "=== Enjin Engine Binary Distribution Builder ==="

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$SOURCE_DIR/build-dist"

mkdir -p "$BUILD_DIR"

echo "[1/3] Configuring CMake..."
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install"

echo "[2/3] Building..."
cmake --build "$BUILD_DIR" --config Release --parallel

echo "[3/3] Packaging..."
cd "$BUILD_DIR" && cpack -C Release

echo "Done! Package is in $BUILD_DIR"

#!/bin/bash
set -e

# ----------------------------------------
# 설정
# ----------------------------------------
WORK_DIR="$PWD/external_skia"
SKIA_DIR="$WORK_DIR/skia"
OUT_DIR="$WORK_DIR/out/vulkan"
DEPOT_TOOLS="$WORK_DIR/depot_tools"

# 필요한 패키지 설치 (Ubuntu 기준)
# sudo apt update
# sudo apt install -y python3 git curl build-essential cmake ninja-build libfreetype6-dev libfontconfig1-dev libvulkan-dev
# # 기본 X11 헤더
# sudo apt install -y libx11-dev libx11-xcb-dev libxcb1-dev libxcb-xinerama0-dev libxrandr-dev libxinerama-dev libxcursor-dev
# # GLU
# sudo apt install -y libglu1-mesa-dev freeglut3-dev mesa-common-dev
# # GLEW
# sudo apt install -y libglew-dev

# ----------------------------------------
# depot_tools 설치
# ----------------------------------------
if [ ! -d "$DEPOT_TOOLS" ]; then
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS"
fi
export PATH="$DEPOT_TOOLS:$PATH"

# ----------------------------------------
# Skia 소스 가져오기
# ----------------------------------------
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

if [ ! -d "$SKIA_DIR" ]; then
    fetch skia
    cd "$SKIA_DIR"
    python3 tools/git-sync-deps
fi

cd "$SKIA_DIR"

# ----------------------------------------
# GN 빌드 설정
# ----------------------------------------
mkdir -p "$OUT_DIR"
bin/gn gen "$OUT_DIR" --args="is_official_build=false is_debug=true skia_use_vulkan=true skia_use_gl=true skia_use_metal=false skia_use_direct3d=false skia_use_fontconfig=true skia_use_freetype=true skia_use_system_freetype2=false target_cpu=\"x64\""

# ----------------------------------------
# Ninja 빌드
# ----------------------------------------
ninja -C "$OUT_DIR"

echo "Skia build completed successfully!"
echo "Headers: $SKIA_DIR/include"
echo "Library: $OUT_DIR/libskia.a"
echo "Generated headers: $OUT_DIR/gen"

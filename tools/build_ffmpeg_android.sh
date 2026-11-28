#!/usr/bin/env bash
set -euo pipefail

# Build a small static FFmpeg toolchain for the Android app.
# Required: ANDROID_NDK_HOME, ABI (arm64-v8a or armeabi-v7a), glslangValidator,
# make, git, pkg-config.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/android_ndk_paths.sh"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
ABI="${ABI:-arm64-v8a}"
API="${ANDROID_API:-26}"
VERSION="${FFMPEG_VERSION:-n8.0}"
VULKAN_HEADERS_TAG="${VULKAN_HEADERS_TAG:-v1.4.317}"
VULKAN_HEADERS_VERSION="${VULKAN_HEADERS_TAG#v}"
HOST_TAG="linux-x86_64"

if [[ -z "$NDK" || ! -d "$NDK" ]]; then
  echo "ANDROID_NDK_HOME (ou ANDROID_NDK_ROOT) não aponta para um NDK válido." >&2
  exit 1
fi

case "$ABI" in
  arm64-v8a)
    ARCH="aarch64"
    CPU="armv8-a"
    TRIPLE="aarch64-linux-android"
    ;;
  armeabi-v7a)
    ARCH="arm"
    CPU="armv7-a"
    TRIPLE="armv7a-linux-androideabi"
    ;;
  *)
    echo "ABI não suportada: $ABI" >&2
    exit 1
    ;;
esac

TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG"
SYSROOT="$TOOLCHAIN/sysroot"
CC="$TOOLCHAIN/bin/${TRIPLE}${API}-clang"
CXX="$TOOLCHAIN/bin/${TRIPLE}${API}-clang++"
LIB_DIR="$(android_ndk_vulkan_lib_dir "$SYSROOT" "$ABI" "$API")"

for tool in "$CC" "$CXX" "$TOOLCHAIN/bin/llvm-ar" "$LIB_DIR/libvulkan.so"; do
  [[ -e "$tool" ]] || { echo "Ferramenta/arquivo ausente: $tool" >&2; exit 1; }
done
command -v glslangValidator >/dev/null
command -v pkg-config >/dev/null

WORK="$ROOT/build/ffmpeg/$ABI"
SOURCE="$WORK/source"
BUILD="$WORK/build"
OUT="$ROOT/ffmpeg-android/$ABI"
VULKAN_HEADERS_DIR="$ROOT/build/vulkan-headers/$VULKAN_HEADERS_TAG"
mkdir -p "$WORK" "$BUILD" "$OUT/include" "$OUT/lib" "$WORK/pkgconfig" "$(dirname "$VULKAN_HEADERS_DIR")"

if [[ ! -f "$SOURCE/configure" ]]; then
  rm -rf "$SOURCE"
  git clone --depth 1 --branch "$VERSION" https://github.com/FFmpeg/FFmpeg.git "$SOURCE"
fi

# FFmpeg n8.0 omits VKAPI_ATTR on this callback. On Android ARMv7,
# VKAPI_ATTR expands to the Vulkan-required aapcs-vfp calling convention.
CALLBACK_SOURCE="$SOURCE/libavutil/hwcontext_vulkan.c"
CALLBACK_PATCH="$ROOT/patches/ffmpeg-vulkan-armv7-callback.patch"
if grep -Fq 'static VkBool32 VKAPI_CALL vk_dbg_callback' "$CALLBACK_SOURCE"; then
  git -C "$SOURCE" apply "$CALLBACK_PATCH"
elif grep -Fq 'static VKAPI_ATTR' "$CALLBACK_SOURCE" &&
     grep -Fq 'VkBool32 VKAPI_CALL vk_dbg_callback' "$CALLBACK_SOURCE"; then
  echo "Callback Vulkan já contém VKAPI_ATTR."
else
  echo "Não foi possível reconhecer a assinatura vk_dbg_callback em $CALLBACK_SOURCE." >&2
  exit 1
fi

if [[ ! -f "$VULKAN_HEADERS_DIR/include/vulkan/vulkan_core.h" ]]; then
  rm -rf "$VULKAN_HEADERS_DIR"
  git clone --depth 1 --branch "$VULKAN_HEADERS_TAG" \
    https://github.com/KhronosGroup/Vulkan-Headers.git "$VULKAN_HEADERS_DIR"
fi

VULKAN_HEADER_VERSION="$(awk '$1 == "#define" && $2 == "VK_HEADER_VERSION" { print $3; exit }' \
  "$VULKAN_HEADERS_DIR/include/vulkan/vulkan_core.h")"
if [[ ! "$VULKAN_HEADER_VERSION" =~ ^[0-9]+$ ]] || (( VULKAN_HEADER_VERSION < 277 )); then
  echo "Vulkan-Headers insuficientes: VK_HEADER_VERSION=$VULKAN_HEADER_VERSION; mínimo exigido: 277." >&2
  exit 1
fi

cat > "$WORK/pkgconfig/vulkan.pc" <<EOF
prefix=$SYSROOT/usr
libdir=$LIB_DIR
includedir=$VULKAN_HEADERS_DIR/include

Name: Vulkan
Description: Khronos Vulkan-Headers with Android NDK loader
Version: $VULKAN_HEADERS_VERSION
Libs: -L\${libdir} -lvulkan
Cflags: -I\${includedir}
EOF

export PKG_CONFIG_PATH="$WORK/pkgconfig"
export PKG_CONFIG_LIBDIR="$WORK/pkgconfig"
echo "Vulkan-Headers $VULKAN_HEADERS_VERSION (VK_HEADER_VERSION=$VULKAN_HEADER_VERSION)"
echo "Vulkan loader NDK: $LIB_DIR/libvulkan.so"

cd "$SOURCE"
./configure \
  --target-os=android \
  --arch="$ARCH" \
  --cpu="$CPU" \
  --enable-cross-compile \
  --sysroot="$SYSROOT" \
  --cc="$CC" \
  --cxx="$CXX" \
  --ar="$TOOLCHAIN/bin/llvm-ar" \
  --ranlib="$TOOLCHAIN/bin/llvm-ranlib" \
  --nm="$TOOLCHAIN/bin/llvm-nm" \
  --strip="$TOOLCHAIN/bin/llvm-strip" \
  --enable-pic \
  --enable-static \
  --disable-shared \
  --disable-programs \
  --disable-doc \
  --disable-debug \
  --disable-network \
  --disable-autodetect \
  --disable-asm \
  --disable-avdevice \
  --disable-everything \
  --enable-avcodec \
  --enable-avformat \
  --enable-avfilter \
  --enable-avutil \
  --enable-swscale \
  --enable-swresample \
  --enable-vulkan \
  --enable-protocol=file \
  --enable-demuxer=mov,matroska,avi,flv,mpegts \
  --enable-decoder=h264,hevc,vp8,vp9,mjpeg,png,webp \
  --enable-parser=h264,hevc,vp8,vp9,mjpeg \
  --enable-encoder=mpeg4,png \
  --enable-muxer=mp4,image2 \
  --enable-filter=buffer,buffersink,format,hwupload,hwdownload,scale_vulkan,scale,crop,fps,transpose \
  --extra-cflags="-fPIC -I$VULKAN_HEADERS_DIR/include" \
  --extra-ldflags="-L$LIB_DIR" \
  --pkg-config=pkg-config

make -j"${BUILD_JOBS:-2}"
make install DESTDIR="$WORK/stage" prefix=/usr

STAGE="$WORK/stage/usr"
cp -a "$STAGE/include/." "$OUT/include/"
cp -a "$STAGE/lib/"*.a "$OUT/lib/"

for library in avutil avcodec avformat avfilter swscale swresample; do
  [[ -s "$OUT/lib/lib${library}.a" ]] || {
    echo "Biblioteca FFmpeg não foi gerada: lib${library}.a" >&2
    exit 1
  }
done

echo "FFmpeg $VERSION compilado para $ABI com Vulkan."
echo "Headers: $OUT/include"
echo "Bibliotecas: $OUT/lib"

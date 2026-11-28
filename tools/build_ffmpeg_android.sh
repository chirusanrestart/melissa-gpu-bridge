#!/usr/bin/env bash
set -euo pipefail

# Build a small static FFmpeg toolchain for the Android app.
# Required: ANDROID_NDK_HOME, ABI (arm64-v8a or armeabi-v7a), glslangValidator,
# make, git, pkg-config.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
ABI="${ABI:-arm64-v8a}"
API="${ANDROID_API:-26}"
VERSION="${FFMPEG_VERSION:-n8.0}"
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
LIB_DIR="$SYSROOT/usr/lib/$TRIPLE/$API"
VULKAN_INCLUDE_DIR="$SYSROOT/usr/include"

for tool in "$CC" "$CXX" "$TOOLCHAIN/bin/llvm-ar" "$LIB_DIR/libvulkan.so" "$VULKAN_INCLUDE_DIR/vulkan/vulkan.h"; do
  [[ -e "$tool" ]] || { echo "Ferramenta/arquivo ausente: $tool" >&2; exit 1; }
done
command -v glslangValidator >/dev/null
command -v pkg-config >/dev/null

WORK="$ROOT/build/ffmpeg/$ABI"
SOURCE="$WORK/source"
BUILD="$WORK/build"
OUT="$ROOT/ffmpeg-android/$ABI"
mkdir -p "$WORK" "$BUILD" "$OUT/include" "$OUT/lib" "$WORK/pkgconfig"

if [[ ! -f "$SOURCE/configure" ]]; then
  rm -rf "$SOURCE"
  git clone --depth 1 --branch "$VERSION" https://github.com/FFmpeg/FFmpeg.git "$SOURCE"
fi

# Use absolute paths for pkg-config. FFmpeg configure checks both Vulkan headers
# and linker availability; relying on SYSROOT_DIR here duplicates the NDK prefix.
cat > "$WORK/pkgconfig/vulkan.pc" <<EOF
prefix=$SYSROOT/usr
libdir=$LIB_DIR
includedir=$VULKAN_INCLUDE_DIR

Name: vulkan
Description: Android NDK Vulkan loader
Version: 1.3.275
Libs: -L\${libdir} -lvulkan
Cflags: -I\${includedir}
EOF

export PKG_CONFIG_PATH="$WORK/pkgconfig"
export PKG_CONFIG_LIBDIR="$WORK/pkgconfig"
unset PKG_CONFIG_SYSROOT_DIR

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
  --enable-encoder=png \
  --enable-muxer=image2 \
  --enable-filter=buffer,buffersink,format,hwupload,hwdownload,scale_vulkan,scale,crop,fps,transpose \
  --extra-cflags="-fPIC -I$VULKAN_INCLUDE_DIR" \
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

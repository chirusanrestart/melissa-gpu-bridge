#!/usr/bin/env bash
set -euo pipefail

# Build the Android NDK's bundled Shaderc sources for one target ABI.
# Output: shaderc-android/<ABI>/{include,lib,VERSION}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
ABI="${ABI:-arm64-v8a}"
API="${ANDROID_API:-26}"
JOBS="${BUILD_JOBS:-2}"

if [[ -z "$NDK" || ! -d "$NDK" ]]; then
  echo "ANDROID_NDK_HOME (ou ANDROID_NDK_ROOT) não aponta para um NDK válido." >&2
  exit 1
fi

case "$ABI" in
  arm64-v8a|armeabi-v7a) ;;
  *)
    echo "ABI não suportada pelo Shaderc: $ABI" >&2
    exit 1
    ;;
esac

NDK_BUILD="$NDK/ndk-build"
SOURCE="$NDK/sources/third_party/shaderc"
if [[ ! -x "$NDK_BUILD" ]]; then
  echo "ndk-build ausente: $NDK_BUILD" >&2
  exit 1
fi
if [[ ! -f "$SOURCE/Android.mk" || ! -f "$SOURCE/CHANGES" ]]; then
  echo "O NDK instalado não contém sources/third_party/shaderc (Android.mk/CHANGES)." >&2
  echo "NDK: $NDK" >&2
  exit 1
fi

VERSION_LINE="$(grep -m1 -E '^[[:space:]]*v[0-9]+\.[0-9]+' "$SOURCE/CHANGES" || true)"
SHADERC_VERSION="${VERSION_LINE#*v}"
SHADERC_VERSION="${SHADERC_VERSION%%[[:space:]]*}"
SHADERC_VERSION="${SHADERC_VERSION%-dev}"
if [[ ! "$SHADERC_VERSION" =~ ^[0-9]+\.[0-9]+([.][0-9]+)?$ ]]; then
  echo "Não foi possível extrair uma versão estável de Shaderc de $SOURCE/CHANGES." >&2
  echo "Linha encontrada: $VERSION_LINE" >&2
  exit 1
fi

(
  cd "$SOURCE"
  "$NDK_BUILD" \
    NDK_PROJECT_PATH=. \
    APP_BUILD_SCRIPT=Android.mk \
    APP_STL=c++_static \
    APP_ABI="$ABI" \
    APP_PLATFORM="android-$API" \
    libshaderc_combined \
    -j"$JOBS"
)

BUILT_LIBRARY="$SOURCE/libs/c++_static/$ABI/libshaderc.a"
BUILT_HEADERS="$SOURCE/include/shaderc"
if [[ ! -s "$BUILT_LIBRARY" ]]; then
  echo "Biblioteca Shaderc não foi gerada: $BUILT_LIBRARY" >&2
  exit 1
fi
if [[ ! -s "$BUILT_HEADERS/shaderc.h" ]]; then
  echo "Headers Shaderc não foram gerados: $BUILT_HEADERS" >&2
  exit 1
fi

OUT="$ROOT/shaderc-android/$ABI"
mkdir -p "$OUT/lib" "$OUT/include"
install -m 644 "$BUILT_LIBRARY" "$OUT/lib/libshaderc.a"
rm -rf "$OUT/include/shaderc"
cp -a "$BUILT_HEADERS" "$OUT/include/shaderc"
printf '%s\n' "$SHADERC_VERSION" > "$OUT/VERSION"

echo "Shaderc $SHADERC_VERSION compilado para $ABI (Android API $API)."
echo "Biblioteca: $OUT/lib/libshaderc.a"

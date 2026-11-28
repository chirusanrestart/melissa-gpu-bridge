#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VALIDATOR="$ROOT/tools/verify_ffmpeg_android_config.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fail() {
  printf 'FAIL: %s\\n' "$1" >&2
  exit 1
}

mkdir -p "$TMP/enabled/ffbuild" "$TMP/no-filter/ffbuild" "$TMP/no-shaderc/ffbuild"
printf '#define CONFIG_LIBSHADERC 1\\n' > "$TMP/enabled/config.h"
printf '#define CONFIG_SCALE_VULKAN_FILTER 1\\n' > "$TMP/enabled/config_components.h"
printf 'CONFIG_LIBSHADERC=yes\\n' > "$TMP/enabled/ffbuild/config.mak"

printf '#define CONFIG_SCALE_VULKAN_FILTER 0\\n' > "$TMP/no-filter/config_components.h"
printf 'CONFIG_LIBSHADERC=yes\\n' > "$TMP/no-filter/ffbuild/config.mak"

printf '#define CONFIG_SCALE_VULKAN_FILTER 1\\n' > "$TMP/no-shaderc/config_components.h"
printf 'CONFIG_LIBSHADERC=no\\n' > "$TMP/no-shaderc/ffbuild/config.mak"

"$VALIDATOR" "$TMP/enabled/config_components.h" "$TMP/enabled/ffbuild/config.mak" arm64-v8a >/dev/null \
  || fail 'valid Vulkan + Shaderc configuration was rejected'

if "$VALIDATOR" "$TMP/no-filter/config_components.h" "$TMP/no-filter/ffbuild/config.mak" arm64-v8a >/dev/null 2>&1; then
  fail 'configuration without scale_vulkan was accepted'
fi

if "$VALIDATOR" "$TMP/no-shaderc/config_components.h" "$TMP/no-shaderc/ffbuild/config.mak" arm64-v8a >/dev/null 2>&1; then
  fail 'configuration without Shaderc was accepted'
fi

printf 'PASS: FFmpeg config validation accepts enabled scale_vulkan + Shaderc and rejects either missing feature.\\n'

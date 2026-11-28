#!/usr/bin/env bash
set -euo pipefail

COMPONENTS_HEADER="${1:?FFmpeg config_components.h path is required}"
CONFIG_MAK="${2:?FFmpeg ffbuild/config.mak path is required}"
ABI="${3:?Android ABI is required}"

if ! grep -q '^#define CONFIG_SCALE_VULKAN_FILTER 1$' "$COMPONENTS_HEADER"; then
  echo "FFmpeg não habilitou CONFIG_SCALE_VULKAN_FILTER para $ABI." >&2
  grep -E '^#define CONFIG_SCALE_VULKAN_FILTER ' "$COMPONENTS_HEADER" >&2 || true
  exit 1
fi

if ! grep -q '^CONFIG_LIBSHADERC=yes$' "$CONFIG_MAK"; then
  echo "FFmpeg não confirmou CONFIG_LIBSHADERC para $ABI." >&2
  grep -E '^CONFIG_LIBSHADERC=' "$CONFIG_MAK" >&2 || true
  exit 1
fi

printf 'Verificado: libshaderc e scale_vulkan habilitados para %s.\\n' "$ABI"

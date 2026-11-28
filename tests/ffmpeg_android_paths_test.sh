#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/android_ndk_paths.sh"

SYSROOT="$(mktemp -d)"
trap 'rm -rf "$SYSROOT"' EXIT

mkdir -p "$SYSROOT/usr/lib/aarch64-linux-android/26"
mkdir -p "$SYSROOT/usr/lib/arm-linux-androideabi/26"
touch "$SYSROOT/usr/lib/aarch64-linux-android/26/libvulkan.so"
touch "$SYSROOT/usr/lib/arm-linux-androideabi/26/libvulkan.so"

assert_vulkan_library_path() {
    local abi="$1"
    local expected="$2"
    local actual

    actual="$(android_ndk_vulkan_lib_dir "$SYSROOT" "$abi" 26)"
    [[ "$actual" == "$expected" ]] || {
        printf 'FAIL: %s path: expected %s, got %s\\n' "$abi" "$expected" "$actual" >&2
        return 1
    }
    [[ -f "$actual/libvulkan.so" ]] || {
        printf 'FAIL: %s Vulkan loader not found at %s\\n' "$abi" "$actual" >&2
        return 1
    }
}

assert_vulkan_library_path arm64-v8a "$SYSROOT/usr/lib/aarch64-linux-android/26"
assert_vulkan_library_path armeabi-v7a "$SYSROOT/usr/lib/arm-linux-androideabi/26"

if android_ndk_vulkan_lib_dir "$SYSROOT" x86 26 >/dev/null 2>&1; then
    echo 'FAIL: unsupported ABI was accepted' >&2
    exit 1
fi

echo 'PASS: NDK Vulkan loader paths resolve for ARM64 and ARMv7; unsupported ABI is rejected.'

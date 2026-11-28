android_ndk_vulkan_lib_dir() {
    local sysroot="${1:?Android NDK sysroot is required}"
    local abi="${2:?Android ABI is required}"
    local api="${3:?Android API level is required}"
    local sysroot_triple

    case "$abi" in
        arm64-v8a) sysroot_triple='aarch64-linux-android' ;;
        armeabi-v7a) sysroot_triple='arm-linux-androideabi' ;;
        *)
            printf 'ABI Android não suportada: %s\\n' "$abi" >&2
            return 1
            ;;
    esac

    printf '%s/usr/lib/%s/%s\\n' "${sysroot%/}" "$sysroot_triple" "$api"
}

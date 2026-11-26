#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

if [[ -z "${PREFIX:-}" || ! -d "$PREFIX/bin" ]]; then
    printf '%s\n' "Execute este instalador dentro do Termux." >&2
    exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ARCH="$(dpkg --print-architecture 2>/dev/null || uname -m)"
case "$ARCH" in
    aarch64)
        BINARY="mgpu-aarch64"
        ;;
    arm|armv7l|armv8l)
        BINARY="mgpu-armv7"
        ;;
    *)
        printf 'Arquitetura não suportada pelo pacote: %s\n' "$ARCH" >&2
        exit 1
        ;;
esac

SOURCE="$SCRIPT_DIR/$BINARY"
if [[ ! -f "$SOURCE" ]]; then
    printf 'Binário não encontrado: %s\n' "$SOURCE" >&2
    printf '%s\n' "Extraia o ZIP completo antes de executar install.sh." >&2
    exit 1
fi

install -m 755 "$SOURCE" "$PREFIX/bin/mgpu"
printf 'Instalado: %s/bin/mgpu\n' "$PREFIX"

if [[ ! -e "$PREFIX/lib/libvulkan.so" ]]; then
    printf '%s\n' "Aviso: loader Vulkan do Termux não encontrado."
    printf '%s\n' "Instale com: pkg install vulkan-loader-android"
fi

printf '%s\n' "Teste com: mgpu info"

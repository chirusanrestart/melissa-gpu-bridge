#!/usr/bin/env python3
import pathlib
import struct
import sys

if len(sys.argv) != 3:
    raise SystemExit("uso: embed_spirv.py entrada.spv saida.h")
data = pathlib.Path(sys.argv[1]).read_bytes()
if not data or len(data) % 4:
    raise SystemExit("SPIR-V inválido: tamanho vazio ou não divisível por 4")
words = struct.unpack("<" + "I" * (len(data) // 4), data)
lines = ["#pragma once", "#include <cstdint>", "", "static constexpr uint32_t kVectorShaderSpv[] = {"]
for offset in range(0, len(words), 8):
    lines.append("    " + ", ".join(f"0x{word:08x}u" for word in words[offset:offset + 8]) + ",")
lines.extend(["};", ""])
target = pathlib.Path(sys.argv[2])
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text("\n".join(lines), encoding="utf-8")
print(f"Gerado {target}: {len(data)} bytes")

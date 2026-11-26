#!/usr/bin/env python3
import json
import subprocess
import sys

binary = sys.argv[1]

info_result = subprocess.run(
    [binary, "info", "--json"],
    check=True,
    capture_output=True,
    text=True,
)
info = json.loads(info_result.stdout)
assert info["ok"] is True
assert info["command"] == "info"
assert info["element_count"] is None
assert info["output"] == (
    'Vulkan: disponível\n'
    'Dispositivos: 1\n'
    'GPU: "Teste" \\\\ caminho\tcoluna\r\n'
    'controle:\x01'
)

compute_result = subprocess.run(
    [binary, "test", "4096", "--json"],
    check=True,
    capture_output=True,
    text=True,
)
compute = json.loads(compute_result.stdout)
assert compute["ok"] is True
assert compute["command"] == "test"
assert compute["element_count"] == 4096
assert "Elementos processados: 4096" in compute["output"]

print("CLI JSON tests passed")

#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

installer = Path(__file__).resolve().parents[1] / "termux" / "install.sh"


def run_install(architecture: str):
    with tempfile.TemporaryDirectory(prefix="mgpu-install-test-") as temporary:
        root = Path(temporary)
        prefix = root / "prefix"
        package = root / "package"
        fake_bin = root / "fake-bin"
        (prefix / "bin").mkdir(parents=True)
        (prefix / "lib").mkdir()
        package.mkdir()
        fake_bin.mkdir()

        (package / "mgpu-aarch64").write_text("arm64 binary\n", encoding="utf-8")
        (package / "mgpu-armv7").write_text("armv7 binary\n", encoding="utf-8")
        dpkg = fake_bin / "dpkg"
        dpkg.write_text('#!/bin/sh\nprintf "%s\\n" "$FAKE_ARCH"\n', encoding="utf-8")
        dpkg.chmod(0o755)

        packaged_installer = package / "install.sh"
        packaged_installer.write_text(installer.read_text(encoding="utf-8"), encoding="utf-8")
        environment = os.environ.copy()
        environment["PREFIX"] = str(prefix)
        environment["FAKE_ARCH"] = architecture
        environment["PATH"] = str(fake_bin) + os.pathsep + environment.get("PATH", "")

        result = subprocess.run(
            ["bash", str(packaged_installer)],
            cwd=package,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )
        return result, prefix


def main():
    arm64, prefix64 = run_install("aarch64")
    assert arm64.returncode == 0, arm64.stderr
    assert (prefix64 / "bin" / "mgpu").read_text(encoding="utf-8") == "arm64 binary\n"
    assert "Instalado:" in arm64.stdout

    armv7, prefix32 = run_install("arm")
    assert armv7.returncode == 0, armv7.stderr
    assert (prefix32 / "bin" / "mgpu").read_text(encoding="utf-8") == "armv7 binary\n"

    unsupported, _ = run_install("x86_64")
    assert unsupported.returncode == 1
    assert "Arquitetura não suportada" in unsupported.stderr

    print("Termux installer tests passed")


if __name__ == "__main__":
    main()

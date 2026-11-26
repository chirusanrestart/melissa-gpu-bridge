#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

installer = Path(__file__).resolve().parents[1] / "termux" / "install.sh"


def run_install(architecture: str, expected_binary: str | None = None):
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

        if expected_binary is not None:
            assert result.returncode == 0, result.stderr
            installed = prefix / "bin" / "mgpu"
            assert installed.read_text(encoding="utf-8") == expected_binary
            assert installed.stat().st_mode & 0o111
            assert "Instalado:" in result.stdout
        return result


def main():
    run_install("aarch64", "arm64 binary\n")
    run_install("arm", "armv7 binary\n")
    unsupported = run_install("x86_64")
    assert unsupported.returncode == 1
    assert "Arquitetura não suportada" in unsupported.stderr
    print("Termux installer tests passed")


if __name__ == "__main__":
    main()

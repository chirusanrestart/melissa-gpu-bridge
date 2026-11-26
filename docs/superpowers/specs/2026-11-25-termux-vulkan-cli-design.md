# Melissa GPU Bridge Termux CLI Design

**Date:** 2026-11-25

## Intent

Add a native command-line interface to Melissa GPU Bridge so Termux can query Android Vulkan devices and run the same validated compute workload without keeping the Android UI open. Preserve the existing Android app and its selectable workload size.

## Current state

The open PR #1, `feature/vulkan-compute-test`, currently contains the Android Vulkan device-discovery path, the compute shader (`out = in * 2 + 1`), the selectable workload (1 through 1,048,576 float elements), and the SPIR-V embedding step in GitHub Actions. The compute implementation is currently embedded in JNI-specific `native-lib.cpp`, so a terminal executable cannot call it without a boundary extraction.

## Goals

- Extract Vulkan discovery and compute execution into a C++ core with no JNI dependency.
- Keep the Android app's JNI methods and visible output compatible.
- Build Termux executables for Android ARM64 (`aarch64`) and ARMv7 (`arm`).
- Provide `mgpu info`, `mgpu devices`, `mgpu test [elements]`, and `mgpu bench`.
- Provide machine-readable JSON output for the commands.
- Validate command parsing and input bounds on the host with CTest.
- Publish a ZIP containing both Termux binaries and an installer as a GitHub Actions artifact.

## Non-goals

- No root, privileged service, remote server, or arbitrary shell-command execution.
- No vendor GPU driver bundled with the project.
- No automatic installation from an unverified package source.
- No changes to the Vulkan shader operation or the app's default test size.

## Approaches considered

1. **Direct Termux Vulkan executable (chosen):** build a native Android executable using the NDK and link the Android Vulkan loader. Termux installs `vulkan-loader-android`, which exposes Android's system loader to Termux. This lets `mgpu` run independently from the app process and keeps one shared C++ compute implementation.
2. **Android app + local socket service:** keep Vulkan in the app and forward commands over a local socket. This adds lifecycle, authentication, and cross-app socket policy complexity; Android 9+ imposes SELinux restrictions on cross-app abstract Unix sockets for apps targeting API 28.
3. **Shell wrapper around the Android app:** cannot perform synchronous command-line benchmarks cleanly and would depend on launching UI activities.

The direct executable is the smallest path that matches the requested Termux workflow. Runtime access to the device's vendor driver still needs confirmation on both the M8 4G and A10s.

## Architecture

- `app/src/main/cpp/gpu_core.h/.cpp`: shared Vulkan device discovery and compute-test implementation; returns ordinary C++ strings.
- `app/src/main/cpp/native-lib.cpp`: thin JNI adapters calling the shared core.
- `termux/src/cli_args.h/.cpp`: deterministic command parsing and validation, independent of Vulkan.
- `termux/src/main.cpp`: CLI dispatch, text/JSON formatting, and benchmark presets.
- `termux/CMakeLists.txt`: Android NDK executable target; host-only parser tests remain buildable without Vulkan.
- `termux/install.sh`: installs the architecture-matched binary into `$PREFIX/bin`.
- `.github/workflows/android.yml`: run host parser tests, preserve the APK build, cross-compile ARMv7/ARM64 CLI binaries, and upload the Termux ZIP artifact.

## Command contract

- `mgpu info [--json]`: print Vulkan availability and device properties.
- `mgpu devices [--json]`: device-discovery alias for `info`.
- `mgpu test [1..1048576] [--json]`: run the same `output = input * 2 + 1` compute shader; default is 256 elements.
- `mgpu bench [--json]`: run the fixed presets 256, 4K, 64K, 256K, and 1M in order.
- `mgpu help`: print usage.

Invalid values, overflow, unknown commands, and unsupported options must fail before initializing Vulkan. A failed Vulkan run must return a non-zero process exit status. JSON output must be valid and escape control characters.

## Runtime requirements

- Termux on Android with an ABI matching the downloaded executable.
- Termux package `vulkan-loader-android` installed so `libvulkan.so` resolves to the Android system loader.
- A device exposing Vulkan and a compute-capable queue.
- No app UI process is required for the CLI.

## Verification

- Host CTest covers command defaults, valid bounds, overflow, invalid values, unknown commands/options, and JSON argument selection.
- GitHub Actions builds the existing debug APK and both NDK CLI ABIs.
- Hardware validation remains a device-side step: run `mgpu info`, `mgpu test 1048576`, and `mgpu test 1048576 --json` on the M8 4G and A10s.

# Termux Vulkan CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a standalone ARMv7/ARM64 Termux CLI that uses the same Vulkan discovery and compute implementation as the Android app.

**Architecture:** Extract the current JNI-owned Vulkan routines into a plain C++ core. Keep JNI as a thin adapter, compile a separate NDK executable for Termux, and share the current generated SPIR-V header with both targets.

**Tech Stack:** C++17, Android NDK 27.2.12479018, CMake 3.22.1, Vulkan 1.0 API, GLSL/SPIR-V, Python 3, CTest, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-25-termux-vulkan-cli-design.md`

## Global Constraints

- Preserve the Android app's current UI and JNI method signatures.
- Preserve the compute operation `output = input * 2 + 1`.
- Accepted element count: 1 through 1,048,576; omitted count defaults to 256.
- Keep C++ standard at C++17.
- Build Android ABIs `armeabi-v7a` and `arm64-v8a`.
- Do not bundle or replace Android vendor Vulkan drivers.
- Do not require root or keep the Android UI running for Termux CLI execution.
- Invalid CLI input must be rejected before Vulkan initialization.
- The Termux CLI must not execute arbitrary shell input.

## Files and Responsibilities

- Create `app/src/main/cpp/gpu_core.h`: shared API declarations.
- Create `app/src/main/cpp/gpu_core.cpp`: moved Vulkan info and compute implementations.
- Modify `app/src/main/cpp/native-lib.cpp`: retain only JNI adapters.
- Modify `app/src/main/cpp/CMakeLists.txt`: compile the shared core into the Android library.
- Create `termux/src/cli_args.h` and `termux/src/cli_args.cpp`: command model and argument parsing.
- Create `termux/src/main.cpp`: CLI output, dispatch, JSON escaping, and benchmark presets.
- Create `termux/CMakeLists.txt`: Android executable plus host CTest target.
- Create `tests/cli_args_test.cpp`: parser behavior tests.
- Create `termux/install.sh`: architecture-aware local installer.
- Modify `.github/workflows/android.yml`: host test, Android APK build, Termux cross-build, and artifact upload.
- Modify `README.md`: actual CLI install/use steps and runtime loader prerequisite.

## Review Focus

- Element-count overflow or zero must be rejected before Vulkan calls.
- An unavailable loader or missing compute queue must return a clear failure and non-zero exit.
- ARMv7 and ARM64 builds must link to the Android Vulkan loader without bundling vendor ICDs.
- JSON output must remain valid with newlines, quotes, and control characters in Vulkan messages.
- `bench` must run the documented presets in order and report each run distinctly.

---

### Task 1: CLI Argument Contract (TDD)

**Files:**
- Create: `tests/cli_args_test.cpp`
- Create: `termux/src/cli_args.h`
- Create: `termux/src/cli_args.cpp`
- Create: `termux/CMakeLists.txt`

**Interfaces:**
- `mgpu::cli::Options parseArgs(int argc, const char* const argv[])`
- `Options` contains command, element count, JSON flag, success flag, and error text.
- Commands: Help, Info, Devices, Test, Bench.

- [ ] **Step 1: Write parser tests first.** Cover no-argument help; info/devices/bench; test default 256; valid bounds 1 and 1,048,576; reject 0, 1,048,577, non-decimal input, integer overflow, unknown commands, extra positional arguments, and unknown options.
- [ ] **Step 2: Run host CTest and confirm the test fails because the parser interface is not implemented.**
- [ ] **Step 3: Implement the smallest parser using C++17 `std::from_chars`; do not initialize Vulkan in the parser.**
- [ ] **Step 4: Run the complete host CTest suite and confirm all parser cases pass.**

### Task 2: Extract the Shared Vulkan Core

**Files:**
- Create: `app/src/main/cpp/gpu_core.h`
- Create: `app/src/main/cpp/gpu_core.cpp`
- Modify: `app/src/main/cpp/native-lib.cpp`
- Modify: `app/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- `std::string mgpu::gpuInfo()`
- `std::string mgpu::computeTest(int64_t requestedCount)`

- [ ] **Step 1: Add declarations and move the existing device-info/compute code from the PR's current JNI implementation into the core without changing shader math, queue selection, bounds, validation, or timing definitions.**
- [ ] **Step 2: Change failure paths to return `std::string` rather than JNI `jstring`; preserve cleanup on every Vulkan error path.**
- [ ] **Step 3: Reduce `native-lib.cpp` to the two existing JNI methods calling the shared core; keep Java-visible names and method signatures unchanged.**
- [ ] **Step 4: Link `gpu_core.cpp` into the existing Android shared library and run the Android Gradle build through CI.**

### Task 3: Termux CLI

**Files:**
- Create: `termux/src/main.cpp`
- Modify: `termux/src/cli_args.cpp` only if tests reveal a parser defect.

**Interfaces:**
- CLI executable name: `mgpu`.
- `info` and `devices` call `mgpu::gpuInfo()`.
- `test` calls `mgpu::computeTest(count)`.
- `bench` executes 256, 4,096, 65,536, 262,144, and 1,048,576 in that order.
- `--json` wraps the textual report in valid JSON with command/status/element count and escaped output.

- [ ] **Step 1: Add CLI dispatch and a JSON string escaper; ensure invalid arguments exit before any core function is called.**
- [ ] **Step 2: Return exit code 0 only for successful compute validation; return non-zero for parser errors or Vulkan failures.**
- [ ] **Step 3: Add the Android NDK executable target linking the NDK Vulkan import library; keep host parser tests independent of Vulkan.**
- [ ] **Step 4: Run host CTest and compile the Termux executable for both Android ABIs.**

### Task 4: Build Artifacts, Installer, and Documentation

**Files:**
- Create: `termux/install.sh`
- Modify: `.github/workflows/android.yml`
- Modify: `README.md`

- [ ] **Step 1: Add an installer that chooses `mgpu-aarch64` or `mgpu-armv7` from `uname -m`, copies it to `$PREFIX/bin/mgpu`, and never uses root.**
- [ ] **Step 2: Update Actions to run host CTest, keep the current shader compilation/APK build, and cross-compile both Termux ABIs with NDK 27.2.12479018.**
- [ ] **Step 3: Package both binaries and the installer into `melissa-gpu-bridge-termux.zip` and upload it as an Actions artifact.**
- [ ] **Step 4: Document `pkg install vulkan-loader-android`, artifact extraction, installation, commands, bounds, and the fact that the system driver must expose a compute queue.**
- [ ] **Step 5: Inspect the full PR diff and verify the CI workflow result before reporting build status.**

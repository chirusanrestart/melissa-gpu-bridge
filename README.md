# Melissa GPU Bridge

Android-native Vulkan GPU diagnostics with a lightweight Termux control channel.

## Planned architecture

- Kotlin Android app for lifecycle and a minimal status screen.
- C++ through Android NDK/JNI for Vulkan device discovery.
- Vulkan loader and driver supplied by Android; no bundled vendor driver.
- Local Unix-domain socket IPC for the Termux CLI (planned next milestone).
- GitHub Actions builds debug APKs for ARMv7 and ARM64.

## Current milestone

The first milestone establishes a reproducible Android/NDK build and reports the actual Vulkan physical-device properties exposed by the device. It does not claim compute performance or benchmark results yet.

## Build locally

Open this repository in Android Studio, or use:

```bash
./gradlew assembleDebug
```

The APK is produced at `app/build/outputs/apk/debug/app-debug.apk`.

## Requirements

- JDK 17
- Android SDK Platform 35
- Android NDK 27.2.12479018
- CMake 3.22.1

## Security

The future local control service must authenticate clients and accept only explicit, allow-listed commands. It must never execute arbitrary shell commands received over IPC.

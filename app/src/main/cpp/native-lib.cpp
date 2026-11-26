#include <jni.h>

#include <cstdint>
#include <string>

#include "gpu_core.h"

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeGpuInfo(JNIEnv* env, jobject) {
    const std::string result = mgpu::gpuInfo();
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeComputeTest(
    JNIEnv* env,
    jobject,
    jint requestedCount
) {
    const std::string result =
        mgpu::computeTest(static_cast<std::int64_t>(requestedCount));
    return env->NewStringUTF(result.c_str());
}

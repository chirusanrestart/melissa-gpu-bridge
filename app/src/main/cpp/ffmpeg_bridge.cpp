#include <jni.h>

#include <string>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeFFmpegVulkanInfo(
    JNIEnv* env,
    jobject
) {
    AVBufferRef* device = nullptr;
    const int result = av_hwdevice_ctx_create(
        &device,
        AV_HWDEVICE_TYPE_VULKAN,
        nullptr,
        nullptr,
        0
    );

    std::string message = "FFmpeg: ";
    message += av_version_info();
    message += "\nBackend: Vulkan";

    if (result < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(result, error, sizeof(error));
        message += "\nInicialização: FALHOU\nErro: ";
        message += error;
    } else {
        message += "\nInicialização: OK";
        av_buffer_unref(&device);
    }

    return env->NewStringUTF(message.c_str());
}

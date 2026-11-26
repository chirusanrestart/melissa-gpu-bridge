#include <jni.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <sstream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
}

namespace {

std::atomic<int> gProgress{0};
std::atomic<std::int64_t> gFrames{0};
std::atomic<std::int64_t> gTotalFrames{0};
std::atomic<double> gFps{0.0};
std::atomic<double> gSpeed{0.0};
std::atomic<double> gElapsed{0.0};
std::atomic<double> gEta{0.0};
std::atomic<bool> gRunning{false};

std::string ffError(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, buffer, sizeof(buffer));
    return std::string(buffer);
}

std::string progressJson() {
    std::ostringstream out;
    out << "{\"running\":" << (gRunning.load() ? "true" : "false")
        << ",\"progress\":" << gProgress.load()
        << ",\"frames\":" << gFrames.load()
        << ",\"totalFrames\":" << gTotalFrames.load()
        << ",\"fps\":" << gFps.load()
        << ",\"speed\":" << gSpeed.load()
        << ",\"elapsed\":" << gElapsed.load()
        << ",\"eta\":" << gEta.load() << "}";
    return out.str();
}

std::string resizeVideo(int inputFd, const std::string& outputPath, int targetHeight) {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();

    AVFormatContext* input = nullptr;
    AVCodecContext* decoder = nullptr;
    AVFormatContext* output = nullptr;
    AVCodecContext* encoder = nullptr;
    AVBufferRef* hwDevice = nullptr;
    AVFilterGraph* graph = nullptr;
    AVFilterContext* source = nullptr;
    AVFilterContext* upload = nullptr;
    AVFilterContext* scale = nullptr;
    AVFilterContext* download = nullptr;
    AVFilterContext* format = nullptr;
    AVFilterContext* sink = nullptr;
    AVPacket* packet = nullptr;
    AVPacket* encoded = nullptr;
    AVFrame* decoded = nullptr;
    AVFrame* filtered = nullptr;
    int videoIndex = -1;
    int result = 0;
    bool headerWritten = false;
    std::int64_t processed = 0;
    double sourceFps = 30.0;
    std::int64_t totalFrames = 0;
    std::string failure;

    auto cleanup = [&]() {
        av_frame_free(&decoded);
        av_frame_free(&filtered);
        av_packet_free(&packet);
        av_packet_free(&encoded);
        avfilter_graph_free(&graph);
        avcodec_free_context(&decoder);
        avcodec_free_context(&encoder);
        av_buffer_unref(&hwDevice);
        if (output) {
            if (output->pb) avio_closep(&output->pb);
            avformat_free_context(output);
            output = nullptr;
        }
        avformat_close_input(&input);
    };
    auto finish = [&](const std::string& message, bool success) -> std::string {
        cleanup();
        gRunning.store(false);
        if (!success) {
            std::remove(outputPath.c_str());
            return "VIDEO BENCHMARK: FALHOU\n" + message;
        }
        gProgress.store(100);
        gEta.store(0.0);
        std::ostringstream summary;
        summary << "VIDEO BENCHMARK: CONCLUÍDO"
                << "\nBackend: Vulkan GPU"
                << "\nOperação: Resize"
                << "\nQuadros processados: " << processed
                << "\nResolução de saída: arquivo MP4"
                << "\nFPS médio: " << gFps.load()
                << "\nVelocidade média: " << gSpeed.load() << "x"
                << "\nTempo total: " << gElapsed.load() << " s"
                << "\nArquivo: " << outputPath;
        return summary.str();
    };

    if (inputFd < 0 || outputPath.empty() || targetHeight < 144 || targetHeight > 2160) {
        return finish("Parâmetros inválidos.", false);
    }

    gProgress.store(0);
    gFrames.store(0);
    gTotalFrames.store(0);
    gFps.store(0.0);
    gSpeed.store(0.0);
    gElapsed.store(0.0);
    gEta.store(0.0);
    gRunning.store(true);

    const std::string inputPath = "/proc/self/fd/" + std::to_string(inputFd);
    result = avformat_open_input(&input, inputPath.c_str(), nullptr, nullptr);
    if (result < 0) return finish("Não foi possível abrir o vídeo: " + ffError(result), false);

    result = avformat_find_stream_info(input, nullptr);
    if (result < 0) return finish("Não foi possível ler os dados do vídeo: " + ffError(result), false);

    const AVCodec* decoderCodec = nullptr;
    result = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, &decoderCodec, 0);
    if (result < 0 || !decoderCodec)
        return finish("Nenhuma faixa de vídeo compatível foi encontrada.", false);
    videoIndex = result;

    decoder = avcodec_alloc_context3(decoderCodec);
    if (!decoder) return finish("Memória insuficiente para o decoder.", false);
    result = avcodec_parameters_to_context(decoder, input->streams[videoIndex]->codecpar);
    if (result < 0) return finish("Falha ao configurar decoder: " + ffError(result), false);
    decoder->thread_count = 0;
    result = avcodec_open2(decoder, decoderCodec, nullptr);
    if (result < 0) return finish("Falha ao abrir decoder: " + ffError(result), false);

    AVStream* inputStream = input->streams[videoIndex];
    AVRational frameRate = inputStream->avg_frame_rate;
    if (frameRate.num <= 0 || frameRate.den <= 0)
        frameRate = av_guess_frame_rate(input, inputStream, nullptr);
    if (frameRate.num > 0 && frameRate.den > 0)
        sourceFps = av_q2d(frameRate);
    if (sourceFps <= 0.0) sourceFps = 30.0;

    std::int64_t durationUs = inputStream->duration > 0
        ? av_rescale_q(inputStream->duration, inputStream->time_base, AV_TIME_BASE_Q)
        : input->duration;
    if (durationUs > 0)
        totalFrames = static_cast<std::int64_t>((durationUs / static_cast<double>(AV_TIME_BASE)) * sourceFps);
    gTotalFrames.store(totalFrames);

    if (decoder->pix_fmt == AV_PIX_FMT_NONE) {
        return finish("O decoder não informou o formato dos frames após a leitura dos metadados.", false);
    }

    const int outputHeight = targetHeight & ~1;
    int outputWidth = static_cast<int>(
        (static_cast<double>(decoder->width) * outputHeight / decoder->height) + 0.5
    );
    outputWidth = (outputWidth + 1) & ~1;
    if (decoder->width <= 0 || decoder->height <= 0 || outputWidth <= 0)
        return finish("Dimensões do vídeo de entrada inválidas.", false);

    result = av_hwdevice_ctx_create(&hwDevice, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
    if (result < 0)
        return finish("FFmpeg não conseguiu inicializar Vulkan: " + ffError(result), false);

    graph = avfilter_graph_alloc();
    if (!graph) return finish("Não foi possível criar o grafo de filtros.", false);

    const AVFilter* bufferFilter = avfilter_get_by_name("buffer");
    const AVFilter* uploadFilter = avfilter_get_by_name("hwupload");
    const AVFilter* scaleFilter = avfilter_get_by_name("scale_vulkan");
    const AVFilter* downloadFilter = avfilter_get_by_name("hwdownload");
    const AVFilter* formatFilter = avfilter_get_by_name("format");
    const AVFilter* sinkFilter = avfilter_get_by_name("buffersink");
    if (!bufferFilter || !uploadFilter || !scaleFilter || !downloadFilter || !formatFilter || !sinkFilter)
        return finish("Filtros necessários ausentes no FFmpeg compilado.", false);

    char sourceArgs[512]{};
    std::snprintf(
        sourceArgs, sizeof(sourceArgs),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        decoder->width, decoder->height, decoder->pix_fmt,
        inputStream->time_base.num, inputStream->time_base.den,
        decoder->sample_aspect_ratio.num > 0 ? decoder->sample_aspect_ratio.num : 1,
        decoder->sample_aspect_ratio.den > 0 ? decoder->sample_aspect_ratio.den : 1
    );
    result = avfilter_graph_create_filter(&source, bufferFilter, "video_input", sourceArgs, nullptr, graph);
    if (result < 0) return finish("Falha ao criar entrada do filtro: " + ffError(result), false);

    result = avfilter_graph_create_filter(&upload, uploadFilter, "vulkan_upload", nullptr, nullptr, graph);
    if (result < 0) return finish("Falha ao criar hwupload: " + ffError(result), false);
    upload->hw_device_ctx = av_buffer_ref(hwDevice);
    if (!upload->hw_device_ctx)
        return finish("Falha ao associar o dispositivo Vulkan ao hwupload.", false);

    char scaleArgs[128]{};
    std::snprintf(scaleArgs, sizeof(scaleArgs), "w=%d:h=%d", outputWidth, outputHeight);
    result = avfilter_graph_create_filter(&scale, scaleFilter, "gpu_resize", scaleArgs, nullptr, graph);
    if (result < 0) return finish("Falha ao criar scale_vulkan: " + ffError(result), false);

    result = avfilter_graph_create_filter(&download, downloadFilter, "vulkan_download", nullptr, nullptr, graph);
    if (result < 0) return finish("Falha ao criar hwdownload: " + ffError(result), false);
    result = avfilter_graph_create_filter(&format, formatFilter, "output_format", "pix_fmts=yuv420p", nullptr, graph);
    if (result < 0) return finish("Falha ao criar conversão de formato: " + ffError(result), false);
    result = avfilter_graph_create_filter(&sink, sinkFilter, "video_output", nullptr, nullptr, graph);
    if (result < 0) return finish("Falha ao criar saída do filtro: " + ffError(result), false);

    if ((result = avfilter_link(source, 0, upload, 0)) < 0 ||
        (result = avfilter_link(upload, 0, scale, 0)) < 0 ||
        (result = avfilter_link(scale, 0, download, 0)) < 0 ||
        (result = avfilter_link(download, 0, format, 0)) < 0 ||
        (result = avfilter_link(format, 0, sink, 0)) < 0)
        return finish("Falha ao conectar filtros: " + ffError(result), false);

    result = avfilter_graph_config(graph, nullptr);
    if (result < 0)
        return finish("Falha ao configurar pipeline Vulkan: " + ffError(result), false);

    result = avformat_alloc_output_context2(&output, nullptr, "mp4", outputPath.c_str());
    if (result < 0 || !output)
        return finish("Não foi possível criar o contêiner MP4: " + ffError(result), false);

    const AVCodec* encoderCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!encoderCodec)
        return finish("Encoder MPEG-4 ausente na compilação FFmpeg.", false);
    encoder = avcodec_alloc_context3(encoderCodec);
    if (!encoder) return finish("Memória insuficiente para o encoder.", false);

    encoder->width = outputWidth;
    encoder->height = outputHeight;
    encoder->pix_fmt = AV_PIX_FMT_YUV420P;
    encoder->time_base = av_inv_q(frameRate);
    if (encoder->time_base.num <= 0 || encoder->time_base.den <= 0)
        encoder->time_base = AVRational{1, 30};
    encoder->framerate = av_inv_q(encoder->time_base);
    encoder->bit_rate = 4'000'000;
    encoder->gop_size = 60;
    encoder->max_b_frames = 0;
    if (output->oformat->flags & AVFMT_GLOBALHEADER)
        encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    result = avcodec_open2(encoder, encoderCodec, nullptr);
    if (result < 0)
        return finish("Falha ao abrir encoder MPEG-4: " + ffError(result), false);

    AVStream* outputStream = avformat_new_stream(output, nullptr);
    if (!outputStream) return finish("Falha ao criar faixa de saída.", false);
    outputStream->time_base = encoder->time_base;
    result = avcodec_parameters_from_context(outputStream->codecpar, encoder);
    if (result < 0)
        return finish("Falha ao configurar faixa MP4: " + ffError(result), false);

    if (!(output->oformat->flags & AVFMT_NOFILE)) {
        result = avio_open(&output->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
        if (result < 0)
            return finish("Não foi possível criar o arquivo de saída: " + ffError(result), false);
    }
    result = avformat_write_header(output, nullptr);
    if (result < 0)
        return finish("Falha ao iniciar arquivo MP4: " + ffError(result), false);
    headerWritten = true;

    packet = av_packet_alloc();
    encoded = av_packet_alloc();
    decoded = av_frame_alloc();
    filtered = av_frame_alloc();
    if (!packet || !encoded || !decoded || !filtered)
        return finish("Memória insuficiente para os buffers de vídeo.", false);

    auto writeEncoded = [&]() -> int {
        while (true) {
            int ret = avcodec_receive_packet(encoder, encoded);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
            if (ret < 0) return ret;
            av_packet_rescale_ts(encoded, encoder->time_base, outputStream->time_base);
            encoded->stream_index = outputStream->index;
            ret = av_interleaved_write_frame(output, encoded);
            av_packet_unref(encoded);
            if (ret < 0) return ret;
        }
    };

    auto submitFiltered = [&]() -> int {
        while (true) {
            int ret = av_buffersink_get_frame(sink, filtered);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
            if (ret < 0) return ret;
            filtered->pts = av_rescale_q(
                filtered->pts,
                av_buffersink_get_time_base(sink),
                encoder->time_base
            );
            ret = avcodec_send_frame(encoder, filtered);
            av_frame_unref(filtered);
            if (ret < 0) return ret;
            ret = writeEncoded();
            if (ret < 0) return ret;

            ++processed;
            const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();
            const double fps = elapsed > 0.0 ? processed / elapsed : 0.0;
            const double progress = totalFrames > 0
                ? std::min(99.0, processed * 100.0 / totalFrames)
                : 0.0;
            gFrames.store(processed);
            gProgress.store(static_cast<int>(progress));
            gElapsed.store(elapsed);
            gFps.store(fps);
            gSpeed.store(sourceFps > 0.0 ? fps / sourceFps : 0.0);
            gEta.store(progress > 0.0 ? elapsed * (100.0 - progress) / progress : 0.0);
        }
    };

    while ((result = av_read_frame(input, packet)) >= 0) {
        if (packet->stream_index == videoIndex) {
            result = avcodec_send_packet(decoder, packet);
            av_packet_unref(packet);
            if (result < 0) return finish("Falha ao enviar pacote ao decoder: " + ffError(result), false);
            while ((result = avcodec_receive_frame(decoder, decoded)) >= 0) {
                if (decoded->pts == AV_NOPTS_VALUE)
                    decoded->pts = decoded->best_effort_timestamp;
                result = av_buffersrc_add_frame_flags(source, decoded, AV_BUFFERSRC_FLAG_KEEP_REF);
                av_frame_unref(decoded);
                if (result < 0) return finish("Falha ao enviar frame ao Vulkan: " + ffError(result), false);
                result = submitFiltered();
                if (result < 0) return finish("Falha no processamento/codificação: " + ffError(result), false);
            }
            if (result != AVERROR(EAGAIN) && result != AVERROR_EOF)
                return finish("Falha ao decodificar frame: " + ffError(result), false);
        } else {
            av_packet_unref(packet);
        }
    }
    if (result != AVERROR_EOF)
        return finish("Falha durante leitura do vídeo: " + ffError(result), false);

    result = avcodec_send_packet(decoder, nullptr);
    if (result < 0 && result != AVERROR_EOF)
        return finish("Falha ao finalizar decoder: " + ffError(result), false);
    while ((result = avcodec_receive_frame(decoder, decoded)) >= 0) {
        if (decoded->pts == AV_NOPTS_VALUE)
            decoded->pts = decoded->best_effort_timestamp;
        result = av_buffersrc_add_frame_flags(source, decoded, AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(decoded);
        if (result < 0) return finish("Falha ao enviar frame final: " + ffError(result), false);
        result = submitFiltered();
        if (result < 0) return finish("Falha ao processar frame final: " + ffError(result), false);
    }
    if (result != AVERROR(EAGAIN) && result != AVERROR_EOF)
        return finish("Falha ao drenar decoder: " + ffError(result), false);

    result = av_buffersrc_add_frame_flags(source, nullptr, 0);
    if (result < 0 && result != AVERROR_EOF)
        return finish("Falha ao finalizar filtros: " + ffError(result), false);
    result = submitFiltered();
    if (result < 0) return finish("Falha ao drenar filtros: " + ffError(result), false);

    result = avcodec_send_frame(encoder, nullptr);
    if (result < 0 && result != AVERROR_EOF)
        return finish("Falha ao finalizar encoder: " + ffError(result), false);
    result = writeEncoded();
    if (result < 0) return finish("Falha ao gravar frames finais: " + ffError(result), false);

    if (headerWritten) {
        result = av_write_trailer(output);
        if (result < 0) return finish("Falha ao finalizar MP4: " + ffError(result), false);
    }

    const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();
    gElapsed.store(elapsed);
    gFps.store(elapsed > 0.0 ? processed / elapsed : 0.0);
    gSpeed.store(sourceFps > 0.0 ? gFps.load() / sourceFps : 0.0);
    gFrames.store(processed);
    return finish("OK", true);
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeFFmpegVulkanInfo(JNIEnv* env, jobject) {
    AVBufferRef* device = nullptr;
    const int result = av_hwdevice_ctx_create(
        &device, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0
    );

    std::string message = "FFmpeg: ";
    message += av_version_info();
    message += "\nBackend: Vulkan";
    if (result < 0) {
        message += "\nInicialização: FALHOU\nErro: " + ffError(result);
    } else {
        message += "\nInicialização: OK";
        av_buffer_unref(&device);
    }
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeVideoResize(
    JNIEnv* env, jobject, jint inputFd, jstring outputPath, jint targetHeight
) {
    const char* path = env->GetStringUTFChars(outputPath, nullptr);
    if (!path) return env->NewStringUTF("VIDEO BENCHMARK: FALHOU\nCaminho de saída inválido.");
    const std::string nativePath(path);
    env->ReleaseStringUTFChars(outputPath, path);
    const std::string result = resizeVideo(inputFd, nativePath, targetHeight);
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeVideoProgress(JNIEnv* env, jobject) {
    const std::string result = progressJson();
    return env->NewStringUTF(result.c_str());
}

#include <jni.h>
#include <vulkan/vulkan.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include "vector_spv.h"

static std::string vkResultName(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        default: return "VkResult(" + std::to_string(static_cast<int>(r)) + ")";
    }
}
static std::string deviceTypeName(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrada";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Dedicada";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU / software";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual";
        default: return "Outro";
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeGpuInfo(JNIEnv* env, jobject) {
    std::ostringstream out;
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Melissa GPU Bridge";
    app.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    app.pEngineName = "Melissa GPU Engine";
    app.engineVersion = VK_MAKE_VERSION(0, 2, 0);
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ci, nullptr, &instance);
    if (r != VK_SUCCESS) {
        const std::string s = "Vulkan indisponível\nResultado: " + vkResultName(r);
        return env->NewStringUTF(s.c_str());
    }
    uint32_t count = 0;
    r = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (r != VK_SUCCESS || count == 0) {
        out << "Instância Vulkan criada, mas nenhuma GPU encontrada.\nResultado: " << vkResultName(r);
    } else {
        std::vector<VkPhysicalDevice> devices(count);
        r = vkEnumeratePhysicalDevices(instance, &count, devices.data());
        if (r != VK_SUCCESS) out << "Falha ao enumerar GPUs: " << vkResultName(r);
        else {
            out << "Vulkan: disponível\nDispositivos: " << count << "\n\n";
            for (uint32_t i = 0; i < count; ++i) {
                VkPhysicalDeviceProperties p{};
                VkPhysicalDeviceMemoryProperties m{};
                vkGetPhysicalDeviceProperties(devices[i], &p);
                vkGetPhysicalDeviceMemoryProperties(devices[i], &m);
                VkDeviceSize local = 0;
                for (uint32_t h = 0; h < m.memoryHeapCount; ++h)
                    if (m.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                        local += m.memoryHeaps[h].size;
                out << "GPU " << i + 1 << ": " << p.deviceName
                    << "\nTipo: " << deviceTypeName(p.deviceType)
                    << "\nAPI: " << VK_VERSION_MAJOR(p.apiVersion) << "."
                    << VK_VERSION_MINOR(p.apiVersion) << "." << VK_VERSION_PATCH(p.apiVersion)
                    << "\nVendor ID: 0x" << std::hex << p.vendorID
                    << "\nDevice ID: 0x" << p.deviceID << std::dec
                    << "\nHeap local anunciado: " << local / (1024 * 1024) << " MiB\n\n";
            }
        }
    }
    vkDestroyInstance(instance, nullptr);
    const std::string s = out.str();
    return env->NewStringUTF(s.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeComputeTest(JNIEnv* env, jobject, jint requestedCount) {
    constexpr uint32_t MAX_ELEMENTS = 1048576;
    if (requestedCount < 1 || static_cast<uint32_t>(requestedCount) > MAX_ELEMENTS) {
        const std::string s = "COMPUTE: PARÂMETRO INVÁLIDO\nInforme entre 1 e 1.048.576 elementos.";
        return env->NewStringUTF(s.c_str());
    }
    const uint32_t N = static_cast<uint32_t>(requestedCount);
    const VkDeviceSize bytes = sizeof(float) * static_cast<VkDeviceSize>(N);
    std::ostringstream out;
    const auto totalStart = std::chrono::steady_clock::now();
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffers[2]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory memories[2]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkShaderModule shader = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    void* mapped[2]{nullptr, nullptr};

    auto cleanup = [&]() {
        if (device) {
            for (int i = 0; i < 2; ++i)
                if (mapped[i]) vkUnmapMemory(device, memories[i]);
            if (fence) vkDestroyFence(device, fence, nullptr);
            if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            if (descriptorLayout) vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
            if (shader) vkDestroyShaderModule(device, shader, nullptr);
            for (int i = 0; i < 2; ++i) {
                if (buffers[i]) vkDestroyBuffer(device, buffers[i], nullptr);
                if (memories[i]) vkFreeMemory(device, memories[i], nullptr);
            }
            vkDestroyDevice(device, nullptr);
        }
        if (instance) vkDestroyInstance(instance, nullptr);
    };
    auto fail = [&](const char* step, VkResult r) -> jstring {
        out << "COMPUTE: FALHOU\nEtapa: " << step << "\nResultado: " << vkResultName(r);
        cleanup();
        const std::string s = out.str();
        return env->NewStringUTF(s.c_str());
    };

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Melissa GPU Bridge Compute";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    VkResult r = vkCreateInstance(&ici, nullptr, &instance);
    if (r != VK_SUCCESS) return fail("vkCreateInstance", r);

    uint32_t physicalCount = 0;
    r = vkEnumeratePhysicalDevices(instance, &physicalCount, nullptr);
    if (r != VK_SUCCESS) return fail("enumerar dispositivos", r);
    if (!physicalCount) return fail("nenhuma GPU física", VK_ERROR_INITIALIZATION_FAILED);
    std::vector<VkPhysicalDevice> physicals(physicalCount);
    r = vkEnumeratePhysicalDevices(instance, &physicalCount, physicals.data());
    if (r != VK_SUCCESS) return fail("listar dispositivos", r);

    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties props{};
    uint32_t familyIndex = UINT32_MAX;
    for (auto candidate : physicals) {
        uint32_t n = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, nullptr);
        std::vector<VkQueueFamilyProperties> families(n);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, families.data());
        for (uint32_t i = 0; i < n; ++i) {
            if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                physical = candidate;
                familyIndex = i;
                vkGetPhysicalDeviceProperties(candidate, &props);
                break;
            }
        }
        if (physical) break;
    }
    if (!physical) return fail("fila COMPUTE não encontrada", VK_ERROR_FEATURE_NOT_PRESENT);

    const uint32_t workgroupCount = (N + 63u) / 64u;
    if (bytes > props.limits.maxStorageBufferRange) {
        out << "COMPUTE: LIMITE EXCEDIDO\nBuffer solicitado: " << bytes
            << " bytes\nLimite Vulkan: " << props.limits.maxStorageBufferRange << " bytes";
        cleanup();
        const std::string s = out.str();
        return env->NewStringUTF(s.c_str());
    }
    if (workgroupCount > props.limits.maxComputeWorkGroupCount[0]) {
        out << "COMPUTE: LIMITE DE DISPATCH EXCEDIDO\nGrupos solicitados: " << workgroupCount
            << "\nLimite da GPU: " << props.limits.maxComputeWorkGroupCount[0];
        cleanup();
        const std::string s = out.str();
        return env->NewStringUTF(s.c_str());
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = familyIndex;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    r = vkCreateDevice(physical, &dci, nullptr, &device);
    if (r != VK_SUCCESS) return fail("vkCreateDevice", r);
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(physical, &mp);
    auto memoryType = [&](uint32_t bits) -> uint32_t {
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((bits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                return i;
        return UINT32_MAX;
    };

    for (int i = 0; i < 2; ++i) {
        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = bytes;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        r = vkCreateBuffer(device, &bci, nullptr, &buffers[i]);
        if (r != VK_SUCCESS) return fail("vkCreateBuffer", r);
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, buffers[i], &req);
        const uint32_t type = memoryType(req.memoryTypeBits);
        if (type == UINT32_MAX) return fail("memória HOST_VISIBLE/COHERENT indisponível", VK_ERROR_FEATURE_NOT_PRESENT);
        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = type;
        r = vkAllocateMemory(device, &mai, nullptr, &memories[i]);
        if (r != VK_SUCCESS) return fail("vkAllocateMemory", r);
        r = vkBindBufferMemory(device, buffers[i], memories[i], 0);
        if (r != VK_SUCCESS) return fail("vkBindBufferMemory", r);
        r = vkMapMemory(device, memories[i], 0, bytes, 0, &mapped[i]);
        if (r != VK_SUCCESS) return fail("vkMapMemory", r);
    }
    auto* input = static_cast<float*>(mapped[0]);
    auto* output = static_cast<float*>(mapped[1]);
    for (uint32_t i = 0; i < N; ++i) {
        input[i] = static_cast<float>(i);
        output[i] = -9999.0f;
    }

    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = sizeof(kVectorShaderSpv);
    smci.pCode = kVectorShaderSpv;
    r = vkCreateShaderModule(device, &smci, nullptr, &shader);
    if (r != VK_SUCCESS) return fail("vkCreateShaderModule", r);

    VkDescriptorSetLayoutBinding bindings[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dlci{};
    dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlci.bindingCount = 2;
    dlci.pBindings = bindings;
    r = vkCreateDescriptorSetLayout(device, &dlci, nullptr, &descriptorLayout);
    if (r != VK_SUCCESS) return fail("vkCreateDescriptorSetLayout", r);

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.size = sizeof(uint32_t);
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &descriptorLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &push;
    r = vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout);
    if (r != VK_SUCCESS) return fail("vkCreatePipelineLayout", r);

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader;
    stage.pName = "main";
    VkComputePipelineCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pci.stage = stage;
    pci.layout = pipelineLayout;
    r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);
    if (r != VK_SUCCESS) return fail("vkCreateComputePipelines", r);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    r = vkCreateDescriptorPool(device, &dpci, nullptr, &descriptorPool);
    if (r != VK_SUCCESS) return fail("vkCreateDescriptorPool", r);
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &descriptorLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    r = vkAllocateDescriptorSets(device, &dsai, &set);
    if (r != VK_SUCCESS) return fail("vkAllocateDescriptorSets", r);

    VkDescriptorBufferInfo dbi[2]{};
    VkWriteDescriptorSet writes[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        dbi[i].buffer = buffers[i];
        dbi[i].range = bytes;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &dbi[i];
    }
    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = familyIndex;
    cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    r = vkCreateCommandPool(device, &cpci, nullptr, &commandPool);
    if (r != VK_SUCCESS) return fail("vkCreateCommandPool", r);
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    r = vkAllocateCommandBuffers(device, &cbai, &command);
    if (r != VK_SUCCESS) return fail("vkAllocateCommandBuffers", r);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    r = vkBeginCommandBuffer(command, &begin);
    if (r != VK_SUCCESS) return fail("vkBeginCommandBuffer", r);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(N), &N);
    vkCmdDispatch(command, (N + 63) / 64, 1, 1);
    r = vkEndCommandBuffer(command);
    if (r != VK_SUCCESS) return fail("vkEndCommandBuffer", r);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    r = vkCreateFence(device, &fci, nullptr, &fence);
    if (r != VK_SUCCESS) return fail("vkCreateFence", r);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    const auto gpuStart = std::chrono::steady_clock::now();
    r = vkQueueSubmit(queue, 1, &submit, fence);
    if (r != VK_SUCCESS) return fail("vkQueueSubmit", r);
    r = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    const auto gpuEnd = std::chrono::steady_clock::now();
    if (r != VK_SUCCESS) return fail("vkWaitForFences", r);

    uint32_t errors = 0;
    for (uint32_t i = 0; i < N; ++i) {
        const float expected = input[i] * 2.0f + 1.0f;
        if (!std::isfinite(output[i]) || std::fabs(output[i] - expected) > 0.0001f) ++errors;
    }
    const auto totalEnd = std::chrono::steady_clock::now();
    const double submitWaitMs = std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count();
    const double totalMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();
    out << (errors == 0 ? "COMPUTE: SUCESSO" : "COMPUTE: RESULTADO INCORRETO")
        << "\nGPU: " << props.deviceName
        << "\nFila: COMPUTE"
        << "\nElementos processados: " << N
        << "\nErros de validação: " << errors
        << "\nAmostra: ";
    const uint32_t sampleCount = N < 3u ? N : 3u;
    for (uint32_t i = 0; i < sampleCount; ++i) {
        if (i) out << ", ";
        out << output[i];
    }
    if (N > 3u) out << ", ... " << output[N - 1];
    out << "\nSubmit + espera da GPU: " << submitWaitMs << " ms"
        << "\nTempo total do teste: " << totalMs << " ms"
        << "\n\nA medição inclui submissão e sincronização, não apenas o tempo puro do shader.";
    cleanup();
    const std::string s = out.str();
    return env->NewStringUTF(s.c_str());
}

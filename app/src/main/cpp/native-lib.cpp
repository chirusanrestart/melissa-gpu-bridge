#include <jni.h>
#include <vulkan/vulkan.h>

#include <sstream>
#include <string>
#include <vector>

static std::string vkResultName(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        default: return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_melissa_gpubridge_MainActivity_nativeGpuInfo(JNIEnv* env, jobject /* thiz */) {
    std::ostringstream out;
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Melissa GPU Bridge";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Melissa GPU Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        out << "Vulkan indisponível\nResultado: " << vkResultName(result)
            << "\n\nVerifique o suporte Vulkan do aparelho e do driver.";
        const std::string value = out.str();
        return env->NewStringUTF(value.c_str());
    }

    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        out << "Instância Vulkan criada, mas nenhuma GPU física foi encontrada.\nResultado: "
            << vkResultName(result);
    } else {
        std::vector<VkPhysicalDevice> devices(count);
        result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
        if (result != VK_SUCCESS) {
            out << "Falha ao enumerar GPUs: " << vkResultName(result);
        } else {
            out << "Vulkan: disponível\nDispositivos: " << count << "\n\n";
            for (uint32_t i = 0; i < count; ++i) {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(devices[i], &props);
                VkPhysicalDeviceMemoryProperties memory{};
                vkGetPhysicalDeviceMemoryProperties(devices[i], &memory);

                VkDeviceSize deviceLocalBytes = 0;
                for (uint32_t h = 0; h < memory.memoryHeapCount; ++h) {
                    if ((memory.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                        deviceLocalBytes += memory.memoryHeaps[h].size;
                    }
                }

                out << "GPU " << (i + 1) << ": " << props.deviceName << "\n";
                out << "Tipo: ";
                switch (props.deviceType) {
                    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: out << "Integrada"; break;
                    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: out << "Dedicada"; break;
                    case VK_PHYSICAL_DEVICE_TYPE_CPU: out << "CPU / software"; break;
                    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: out << "Virtual"; break;
                    default: out << "Outro"; break;
                }
                out << "\nAPI: " << VK_VERSION_MAJOR(props.apiVersion) << "."
                    << VK_VERSION_MINOR(props.apiVersion) << "."
                    << VK_VERSION_PATCH(props.apiVersion) << "\n";
                out << "Vendor ID: 0x" << std::hex << props.vendorID
                    << "\nDevice ID: 0x" << props.deviceID << std::dec << "\n";
                out << "Heap local anunciado: " << (deviceLocalBytes / (1024 * 1024)) << " MiB\n\n";
            }
        }
    }

    vkDestroyInstance(instance, nullptr);
    const std::string value = out.str();
    return env->NewStringUTF(value.c_str());
}

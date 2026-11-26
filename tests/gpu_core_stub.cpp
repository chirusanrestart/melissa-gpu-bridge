#include "gpu_core.h"

namespace mgpu {

std::string gpuInfo() {
    return "Vulkan: disponível\nDispositivos: 1\nGPU: \"Teste\" \\\\ caminho\tcoluna\r\ncontrole:\x01";
}

std::string computeTest(std::int64_t requestedCount) {
    if (requestedCount < 1 || requestedCount > kMaxElements) {
        return "COMPUTE: PARÂMETRO INVÁLIDO";
    }
    return "COMPUTE: SUCESSO\nElementos processados: " +
           std::to_string(requestedCount);
}

}  // namespace mgpu

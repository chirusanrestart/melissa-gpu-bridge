#pragma once

#include <cstdint>
#include <string>

namespace mgpu {

constexpr std::int64_t kMaxElements = 1'048'576;

std::string gpuInfo();
std::string computeTest(std::int64_t requestedCount);

}  // namespace mgpu

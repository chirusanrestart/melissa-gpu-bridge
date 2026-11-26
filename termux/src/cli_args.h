#pragma once

#include <cstdint>
#include <string>

namespace mgpu::cli {

constexpr std::uint32_t kDefaultElementCount = 256u;
constexpr std::uint32_t kMaxElementCount = 1'048'576u;

enum class Command {
    Help,
    Info,
    Devices,
    Test,
    Bench
};

struct Options {
    Command command = Command::Help;
    std::uint32_t elementCount = kDefaultElementCount;
    bool json = false;
    bool ok = true;
    std::string error;
};

Options parseArgs(int argc, const char* const argv[]);

}  // namespace mgpu::cli

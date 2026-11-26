#include "cli_args.h"
#include "gpu_core.h"

#include <array>
#include <iostream>
#include <sstream>
#include <string>

namespace {

constexpr std::array<std::uint32_t, 5> kBenchPresets{
    256u, 4'096u, 65'536u, 262'144u, 1'048'576u
};

const char* commandName(mgpu::cli::Command command) {
    using mgpu::cli::Command;
    switch (command) {
        case Command::Info: return "info";
        case Command::Devices: return "devices";
        case Command::Test: return "test";
        case Command::Bench: return "bench";
        case Command::Help: return "help";
    }
    return "unknown";
}

std::string jsonEscape(const std::string& input) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string output;
    output.reserve(input.size() + 8);

    for (unsigned char character : input) {
        switch (character) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            default:
                if (character < 0x20u) {
                    output += "\\u00";
                    output += hex[(character >> 4u) & 0x0fu];
                    output += hex[character & 0x0fu];
                } else {
                    output += static_cast<char>(character);
                }
        }
    }

    return output;
}

bool isComputeSuccess(const std::string& output) {
    return output.rfind("COMPUTE: SUCESSO", 0) == 0;
}

bool isInfoSuccess(const std::string& output) {
    return output.find("Vulkan: disponível") != std::string::npos;
}

void printUsage(std::ostream& output) {
    output
        << "Melissa GPU Bridge • Termux CLI\n"
        << "Uso:\n"
        << "  mgpu info [--json]\n"
        << "  mgpu devices [--json]\n"
        << "  mgpu test [1..1048576] [--json]\n"
        << "  mgpu bench [--json]\n"
        << "  mgpu help\n";
}

int printJson(
    bool success,
    const char* command,
    bool hasElementCount,
    std::uint32_t elementCount,
    const std::string& output
) {
    std::cout << "{\"ok\":" << (success ? "true" : "false")
              << ",\"command\":\"" << command << "\",\"element_count\":";
    if (hasElementCount) {
        std::cout << elementCount;
    } else {
        std::cout << "null";
    }
    std::cout << ",\"output\":\"" << jsonEscape(output) << "\"}\n";
    return success ? 0 : 1;
}

}  // namespace

int main(int argc, const char* const argv[]) {
    const mgpu::cli::Options options = mgpu::cli::parseArgs(argc, argv);
    if (!options.ok) {
        std::cerr << "Erro: " << options.error << "\n\n";
        printUsage(std::cerr);
        return 2;
    }

    if (options.command == mgpu::cli::Command::Help) {
        printUsage(std::cout);
        return 0;
    }

    std::string report;
    bool success = false;
    bool hasElementCount = false;
    std::uint32_t elementCount = 0;

    switch (options.command) {
        case mgpu::cli::Command::Info:
        case mgpu::cli::Command::Devices:
            report = mgpu::gpuInfo();
            success = isInfoSuccess(report);
            break;

        case mgpu::cli::Command::Test:
            hasElementCount = true;
            elementCount = options.elementCount;
            report = mgpu::computeTest(options.elementCount);
            success = isComputeSuccess(report);
            break;

        case mgpu::cli::Command::Bench: {
            success = true;
            std::ostringstream output;
            for (std::uint32_t count : kBenchPresets) {
                output << "=== " << count << " elementos ===\n";
                const std::string result = mgpu::computeTest(count);
                output << result << "\n\n";
                if (!isComputeSuccess(result)) {
                    success = false;
                    break;
                }
            }
            report = output.str();
            break;
        }

        case mgpu::cli::Command::Help:
            printUsage(std::cout);
            return 0;
    }

    if (options.json) {
        return printJson(success, commandName(options.command),
                         hasElementCount, elementCount, report);
    }

    std::cout << report;
    if (report.empty() || report.back() != '\n') {
        std::cout << '\n';
    }
    return success ? 0 : 1;
}

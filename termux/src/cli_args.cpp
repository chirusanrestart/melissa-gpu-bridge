#include "cli_args.h"

#include <charconv>
#include <string_view>
#include <system_error>
#include <utility>

namespace mgpu::cli {
namespace {

Options invalid(std::string message) {
    Options options;
    options.ok = false;
    options.error = std::move(message);
    return options;
}

bool parseElementCount(std::string_view text, std::uint32_t& value) {
    if (text.empty()) return false;

    std::uint64_t parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end ||
        parsed < 1 || parsed > kMaxElementCount) {
        return false;
    }

    value = static_cast<std::uint32_t>(parsed);
    return true;
}

}  // namespace

Options parseArgs(int argc, const char* const argv[]) {
    if (argc <= 1) return Options{};
    if (argv == nullptr || argv[1] == nullptr) {
        return invalid("Comando ausente.");
    }

    Options options;
    const std::string_view command = argv[1];
    if (command == "help" || command == "--help" || command == "-h") {
        options.command = Command::Help;
        if (argc > 2) return invalid("O comando help não aceita argumentos.");
        return options;
    }
    if (command == "info") options.command = Command::Info;
    else if (command == "devices") options.command = Command::Devices;
    else if (command == "test") options.command = Command::Test;
    else if (command == "bench") options.command = Command::Bench;
    else return invalid("Comando desconhecido: " + std::string(command));

    bool countSeen = false;
    for (int index = 2; index < argc; ++index) {
        if (argv[index] == nullptr) return invalid("Argumento vazio.");
        const std::string_view argument = argv[index];

        if (argument == "--json") {
            options.json = true;
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            return invalid("Opção desconhecida: " + std::string(argument));
        }
        if (options.command != Command::Test || countSeen) {
            return invalid("Argumento posicional inesperado: " + std::string(argument));
        }
        if (!parseElementCount(argument, options.elementCount)) {
            return invalid("Quantidade inválida. Use um inteiro entre 1 e 1.048.576.");
        }
        countSeen = true;
    }

    return options;
}

}  // namespace mgpu::cli

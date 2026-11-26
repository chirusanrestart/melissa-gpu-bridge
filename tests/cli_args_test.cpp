#include "cli_args.h"

#include <cstdlib>
#include <iostream>
#include <initializer_list>
#include <string>
#include <vector>

using mgpu::cli::Command;
using mgpu::cli::Options;

namespace {

Options parse(std::initializer_list<const char*> arguments) {
    std::vector<const char*> argv;
    argv.push_back("mgpu");
    for (const char* argument : arguments) {
        argv.push_back(argument);
    }
    return mgpu::cli::parseArgs(static_cast<int>(argv.size()), argv.data());
}

int failures = 0;

void expect(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "PASS " << name << '\n';
    } else {
        std::cerr << "FAIL " << name << '\n';
        ++failures;
    }
}

void expectInvalid(std::initializer_list<const char*> arguments, const std::string& name) {
    const Options result = parse(arguments);
    expect(!result.ok && !result.error.empty(), name);
}

}  // namespace

int main() {
    const Options empty = parse({});
    expect(empty.ok && empty.command == Command::Help, "no arguments show help");

    const Options info = parse({"info"});
    expect(info.ok && info.command == Command::Info && !info.json, "info command");

    const Options infoJson = parse({"info", "--json"});
    expect(infoJson.ok && infoJson.command == Command::Info && infoJson.json, "info JSON flag");

    const Options devices = parse({"devices"});
    expect(devices.ok && devices.command == Command::Devices, "devices command");

    const Options testDefault = parse({"test"});
    expect(testDefault.ok && testDefault.command == Command::Test &&
           testDefault.elementCount == 256, "test defaults to 256 elements");

    const Options testMinimum = parse({"test", "1"});
    expect(testMinimum.ok && testMinimum.elementCount == 1, "test accepts minimum element count");

    const Options testMaximum = parse({"test", "1048576"});
    expect(testMaximum.ok && testMaximum.elementCount == 1048576,
           "test accepts maximum element count");

    const Options testJson = parse({"test", "4096", "--json"});
    expect(testJson.ok && testJson.elementCount == 4096 && testJson.json,
           "test accepts count and JSON flag");

    const Options bench = parse({"bench"});
    expect(bench.ok && bench.command == Command::Bench, "bench command");

    const Options help = parse({"help"});
    expect(help.ok && help.command == Command::Help, "explicit help command");

    expectInvalid({"test", "0"}, "reject zero elements");
    expectInvalid({"test", "1048577"}, "reject count above maximum");
    expectInvalid({"test", "-1"}, "reject negative count");
    expectInvalid({"test", "1.5"}, "reject non-integer count");
    expectInvalid({"test", "18446744073709551616"}, "reject integer overflow");
    expectInvalid({"test", "12", "13"}, "reject extra positional argument");
    expectInvalid({"info", "--unknown"}, "reject unknown info option");
    expectInvalid({"unknown"}, "reject unknown command");
    expectInvalid({"bench", "extra"}, "reject extra bench argument");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All CLI argument tests passed\n";
    return EXIT_SUCCESS;
}

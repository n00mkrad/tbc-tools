// Copyright 2026
// This file is licensed under the provisions of the GNU General Public License v3.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <ac3/Ac3Decoder.h>

namespace {

enum class InputFormat {
    Auto,
    Raw,
    Ascii,
};

struct Options {
    InputFormat inputFormat = InputFormat::Auto;
    LogPriority logLevel = eInfo;
    std::string inputPath;
    std::string outputPath;
    bool showHelp = false;
};

bool isWhitespace(const uint8_t value)
{
    return value == ' ' || value == '\n' || value == '\r' || value == '\t' || value == '\v' || value == '\f';
}

std::string inputFormatToString(const InputFormat format)
{
    switch (format) {
        case InputFormat::Auto: return "auto";
        case InputFormat::Raw: return "raw";
        case InputFormat::Ascii: return "ascii";
    }
    return "unknown";
}

std::string logPriorityToString(const LogPriority priority)
{
    switch (priority) {
        case eDebug: return "DEBUG";
        case eInfo: return "INFO";
        case eWarn: return "WARN";
        case eError: return "ERROR";
        case eOff: return "OFF";
    }
    return "UNKNOWN";
}

bool parseInputFormat(const std::string &text, InputFormat &format)
{
    if (text == "auto") {
        format = InputFormat::Auto;
        return true;
    }
    if (text == "raw") {
        format = InputFormat::Raw;
        return true;
    }
    if (text == "ascii") {
        format = InputFormat::Ascii;
        return true;
    }
    return false;
}

bool parseLogLevel(const std::string &text, LogPriority &level)
{
    std::string normalized = text;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "debug") {
        level = eDebug;
        return true;
    }
    if (normalized == "info") {
        level = eInfo;
        return true;
    }
    if (normalized == "warn" || normalized == "warning") {
        level = eWarn;
        return true;
    }
    if (normalized == "error") {
        level = eError;
        return true;
    }
    if (normalized == "off") {
        level = eOff;
        return true;
    }
    return false;
}

void printUsage(const char *programName)
{
    std::cerr
        << "Usage: " << programName << " [options] <input_symbols> <output_ac3>\n"
        << "Options:\n"
        << "  -f, --format <auto|raw|ascii>      Input symbol format (default: auto)\n"
        << "  -l, --log-level <debug|info|warn|error|off>\n"
        << "                                     Logging verbosity (default: info)\n"
        << "  -h, --help                         Show this help message\n"
        << "\n"
        << "Input formats:\n"
        << "  raw   : binary bytes where each byte is one symbol value (0..3)\n"
        << "  ascii : text bytes '0'..'3' with optional whitespace separators\n"
        << "  auto  : detects raw/ascii for regular files\n";
}

bool readOptionValue(const std::string &arg, const std::string &prefix, int &index, int argc, char *argv[], std::string &value)
{
    if (arg == prefix) {
        if (index + 1 >= argc) {
            return false;
        }
        value = argv[++index];
        return true;
    }

    const std::string eqPrefix = prefix + "=";
    if (arg.rfind(eqPrefix, 0) == 0) {
        value = arg.substr(eqPrefix.size());
        return true;
    }

    return false;
}

bool parseArguments(int argc, char *argv[], Options &options, std::string &error)
{
    std::vector<std::string> positionals;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
            return true;
        }

        std::string value;
        if (readOptionValue(arg, "-f", i, argc, argv, value) || readOptionValue(arg, "--format", i, argc, argv, value)) {
            if (!parseInputFormat(value, options.inputFormat)) {
                error = "Invalid --format value: " + value;
                return false;
            }
            continue;
        }

        if (readOptionValue(arg, "-l", i, argc, argv, value) || readOptionValue(arg, "--log-level", i, argc, argv, value)) {
            if (!parseLogLevel(value, options.logLevel)) {
                error = "Invalid --log-level value: " + value;
                return false;
            }
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            error = "Unknown option: " + arg;
            return false;
        }

        positionals.push_back(arg);
    }

    if (positionals.size() != 2) {
        error = "Expected exactly 2 positional arguments: <input_symbols> <output_ac3>";
        return false;
    }

    options.inputPath = positionals[0];
    options.outputPath = positionals[1];
    return true;
}

class StderrLogger final : public Logger {
public:
    explicit StderrLogger(const LogPriority minimumPriority)
        : m_minimumPriority(minimumPriority)
    {
    }

    void log(const LogPriority priority, const LogCategoryFlags, const std::string &message) override
    {
        if (!isEnabled(priority, eApplication)) {
            return;
        }
        std::cerr << "[" << logPriorityToString(priority) << "] " << message << "\n";
    }

    [[nodiscard]] bool isEnabled(const LogPriority priority, const LogCategoryFlags) const override
    {
        return priority >= m_minimumPriority;
    }

    void sync() override
    {
        std::cerr << std::flush;
    }

private:
    LogPriority m_minimumPriority;
};

InputFormat detectInputFormat(std::istream &input)
{
    std::array<char, 32768> sampleBuffer{};
    input.read(sampleBuffer.data(), static_cast<std::streamsize>(sampleBuffer.size()));
    const auto sampleSize = input.gcount();

    bool allRaw = sampleSize > 0;
    bool allAscii = sampleSize > 0;
    bool seenAsciiSymbols = false;

    for (std::streamsize i = 0; i < sampleSize; i++) {
        const auto value = static_cast<uint8_t>(sampleBuffer[static_cast<size_t>(i)]);
        if (value > 3) {
            allRaw = false;
        }

        if (value >= '0' && value <= '3') {
            seenAsciiSymbols = true;
        } else if (!isWhitespace(value)) {
            allAscii = false;
        }
    }

    input.clear();
    input.seekg(0, std::ios::beg);

    if (allRaw) {
        return InputFormat::Raw;
    }
    if (allAscii && seenAsciiSymbols) {
        return InputFormat::Ascii;
    }
    return InputFormat::Raw;
}

bool writeDecodedFrames(
    const std::vector<uint8_t> &symbols,
    Ac3Decoder &decoder,
    std::ostream &output,
    uint64_t &framesWritten)
{
    if (symbols.empty()) {
        return true;
    }

    const auto frames = decoder.decodeSymbols(symbols);
    for (const auto &frame : frames) {
        output.write(reinterpret_cast<const char *>(frame.data()), static_cast<std::streamsize>(frame.size()));
        if (!output.good()) {
            return false;
        }
        framesWritten++;
    }
    return true;
}

bool decodeRawSymbols(
    std::istream &input,
    std::ostream &output,
    Ac3Decoder &decoder,
    uint64_t &symbolsSeen,
    uint64_t &framesWritten,
    std::string &error)
{
    std::array<char, 1 << 20> readBuffer{};
    uint64_t byteOffset = 0;

    while (input.good()) {
        input.read(readBuffer.data(), static_cast<std::streamsize>(readBuffer.size()));
        const auto bytesRead = input.gcount();
        if (bytesRead <= 0) {
            break;
        }

        std::vector<uint8_t> symbols;
        symbols.reserve(static_cast<size_t>(bytesRead));
        for (std::streamsize i = 0; i < bytesRead; i++, byteOffset++) {
            const auto value = static_cast<uint8_t>(readBuffer[static_cast<size_t>(i)]);
            if (value > 3) {
                error = "Invalid raw symbol at byte offset " + std::to_string(byteOffset) + ": " + std::to_string(value);
                return false;
            }
            symbols.push_back(value);
        }

        symbolsSeen += symbols.size();
        if (!writeDecodedFrames(symbols, decoder, output, framesWritten)) {
            error = "Failed while writing decoded AC3 frame data";
            return false;
        }
    }

    if (input.bad()) {
        error = "Failed while reading input stream";
        return false;
    }

    return true;
}

bool decodeAsciiSymbols(
    std::istream &input,
    std::ostream &output,
    Ac3Decoder &decoder,
    uint64_t &symbolsSeen,
    uint64_t &framesWritten,
    std::string &error)
{
    std::array<char, 1 << 20> readBuffer{};
    uint64_t byteOffset = 0;

    while (input.good()) {
        input.read(readBuffer.data(), static_cast<std::streamsize>(readBuffer.size()));
        const auto bytesRead = input.gcount();
        if (bytesRead <= 0) {
            break;
        }

        std::vector<uint8_t> symbols;
        symbols.reserve(static_cast<size_t>(bytesRead));
        for (std::streamsize i = 0; i < bytesRead; i++, byteOffset++) {
            const auto value = static_cast<uint8_t>(readBuffer[static_cast<size_t>(i)]);
            if (value >= '0' && value <= '3') {
                symbols.push_back(static_cast<uint8_t>(value - '0'));
                continue;
            }
            if (isWhitespace(value)) {
                continue;
            }

            error = "Invalid ascii symbol byte at offset " + std::to_string(byteOffset) + ": " + std::to_string(value);
            return false;
        }

        symbolsSeen += symbols.size();
        if (!writeDecodedFrames(symbols, decoder, output, framesWritten)) {
            error = "Failed while writing decoded AC3 frame data";
            return false;
        }
    }

    if (input.bad()) {
        error = "Failed while reading input stream";
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    Options options;
    std::string parseError;
    if (!parseArguments(argc, argv, options, parseError)) {
        std::cerr << parseError << "\n";
        printUsage(argv[0]);
        return 1;
    }

    if (options.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    std::ifstream inputFile;
    std::istream *input = &std::cin;
    if (options.inputPath != "-") {
        inputFile.open(options.inputPath, std::ios::binary);
        if (!inputFile.is_open()) {
            std::cerr << "Unable to open input file: " << options.inputPath << "\n";
            return 1;
        }
        input = &inputFile;
    }

    std::ofstream outputFile;
    std::ostream *output = &std::cout;
    if (options.outputPath != "-") {
        outputFile.open(options.outputPath, std::ios::binary | std::ios::trunc);
        if (!outputFile.is_open()) {
            std::cerr << "Unable to open output file: " << options.outputPath << "\n";
            return 1;
        }
        output = &outputFile;
    }

    if (options.inputPath == "-" && options.inputFormat == InputFormat::Auto) {
        std::cerr << "Input format auto-detection is not supported for stdin; use --format raw or --format ascii.\n";
        return 1;
    }

    StderrLogger logger(options.logLevel);
    Ac3Decoder decoder(logger);

    InputFormat effectiveFormat = options.inputFormat;
    if (effectiveFormat == InputFormat::Auto) {
        effectiveFormat = detectInputFormat(*input);
        logger.info(eApplication, "Detected input format: " + inputFormatToString(effectiveFormat));
    }

    uint64_t symbolsSeen = 0;
    uint64_t framesWritten = 0;
    std::string decodeError;
    bool ok;
    if (effectiveFormat == InputFormat::Raw) {
        ok = decodeRawSymbols(*input, *output, decoder, symbolsSeen, framesWritten, decodeError);
    } else {
        ok = decodeAsciiSymbols(*input, *output, decoder, symbolsSeen, framesWritten, decodeError);
    }

    if (!ok) {
        std::cerr << decodeError << "\n";
        return 1;
    }

    output->flush();
    logger.info(eApplication, "Decoded " + std::to_string(symbolsSeen) + " symbols into " + std::to_string(framesWritten) + " AC3 frames");
    logger.info(eApplication, decoder.reedSolomonStatistics());
    logger.sync();
    return 0;
}

#include "pfy/binary.hpp"
#include "pfy/error.hpp"
#include "pfy/parser.hpp"
#include "pfy/runtime.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr const char* kVersion = "1.0.0";

void usage(std::ostream& out) {
    out << "Pfy 1.0\n"
        << "A small typed data and scripting language.\n\n"
        << "Usage:\n"
        << "  pfy run <file.pfy|file.pfyb> [--json]\n"
        << "  pfy check <file.pfy|file.pfyb>\n"
        << "  pfy build <file.pfy> [-o <file.pfyb>]\n"
        << "  pfy version\n\n"
        << "Commands:\n"
        << "  run      Parse/load and execute a program.\n"
        << "  check    Parse a source file or verify a compiled image.\n"
        << "  build    Compile a source file to the PFYB binary format.\n"
        << "  version  Print the compiler/runtime version.\n\n"
        << "Options:\n"
        << "  --json   Print the module object after running the program.\n"
        << "  -o FILE  Write the compiled image to FILE.\n";
}

std::string readFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open '" + path + "'");
    std::string data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (data.size() > 64u * 1024u * 1024u) throw std::runtime_error("file is too large");
    return data;
}

void writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("cannot create '" + path + "'");
    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!stream) throw std::runtime_error("could not write '" + path + "'");
}

bool isBinary(const std::string& data) {
    return data.size() >= 4 && data[0] == 'P' && data[1] == 'F' && data[2] == 'Y' && data[3] == 'B';
}

std::string binaryOutputPath(const std::string& input) {
    if (input.size() >= 4 && input.substr(input.size() - 4) == ".pfy") {
        return input.substr(0, input.size() - 4) + ".pfyb";
    }
    return input + ".pfyb";
}

int reportError(const std::string& path, const std::exception& error, const pfy::Error* located) {
    std::cerr << "pfy: ";
    if (located) {
        std::cerr << path << ":" << located->line() << ":" << located->column() << ": ";
    } else if (!path.empty()) {
        std::cerr << path << ": ";
    }
    std::cerr << error.what() << "\n";
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(std::cerr);
        return 2;
    }

    const std::string command = argv[1];
    if (command == "version" || command == "--version") {
        std::cout << "pfy " << kVersion << " (format 1)\n";
        return 0;
    }
    if (command == "help" || command == "--help" || command == "-h") {
        usage(std::cout);
        return 0;
    }
    if (command != "run" && command != "check" && command != "build") {
        std::cerr << "pfy: unknown command '" << command << "'\n\n";
        usage(std::cerr);
        return 2;
    }
    if (argc < 3) {
        std::cerr << "pfy: " << command << " requires a file\n";
        return 2;
    }

    const std::string path = argv[2];
    bool json = false;
    std::string outputPath;
    for (int i = 3; i < argc; ++i) {
        std::string option = argv[i];
        if (option == "--json") {
            json = true;
        } else if (option == "-o" || option == "--out") {
            if (i + 1 >= argc) {
                std::cerr << "pfy: " << option << " requires a path\n";
                return 2;
            }
            outputPath = argv[++i];
        } else {
            std::cerr << "pfy: unknown option '" << option << "'\n";
            return 2;
        }
    }

    try {
        const std::string data = readFile(path);
        pfy::NodePtr program;
        std::string source = data;

        if (isBinary(data)) {
            const std::vector<std::uint8_t> bytes(data.begin(), data.end());
            pfy::BinaryImage image = pfy::decode(bytes);
            program = image.program;
            source = image.source;
            if (command == "build") {
                std::cerr << "pfy: build expects a source file, not a .pfyb image\n";
                return 2;
            }
        } else {
            if (command == "check" || command == "build" || command == "run") {
                program = pfy::parse(source, path);
            }
        }

        if (command == "check") {
            std::cout << "ok: " << path << " ("
                      << (isBinary(data) ? "binary image" : "source") << ")\n";
            return 0;
        }

        if (command == "build") {
            const std::string target = outputPath.empty() ? binaryOutputPath(path) : outputPath;
            writeFile(target, pfy::encode(program, source));
            std::cout << "built: " << target << "\n";
            return 0;
        }

        pfy::Runtime runtime(std::cout);
        pfy::Value result;
        try {
            result = runtime.run(program);
        } catch (const pfy::Error&) {
            throw;
        } catch (const std::exception& error) {
            std::cerr << "pfy: " << path << ": " << error.what() << "\n";
            return 1;
        }
        if (json) std::cout << runtime.json(result) << "\n";
        return 0;
    } catch (const pfy::Error& error) {
        return reportError(path, error, &error);
    } catch (const std::exception& error) {
        return reportError(path, error, nullptr);
    }
}

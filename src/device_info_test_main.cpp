#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <string>
#include <cmath>

static void usage(const char* prog){
    std::cerr <<
    "Usage: " << prog << " [options]\n" <<
    "  --ip <ipaddress>   IPv4 address of the device\n" <<
    "  --help             Print usage/help\n";
}

static bool arg_has(int argc, char** argv, const std::string& key){
    for (int i=1;i<argc;++i) if (key == argv[i]) return true;
    return false;
}

template<typename T>
static bool arg_get(int argc, char** argv, const std::string& key, T& out){
    for (int i=1;i<argc-1;++i) {
        if (key == argv[i]) {
            out = static_cast<T>(std::stod(argv[i+1]));
            return true;
        }
    }
    return false;
}

static bool arg_get_str(int argc, char** argv, const std::string& key, std::string& out){
    for (int i=1;i<argc-1;++i) {
        if (key == argv[i]) {
            out = argv[i+1];
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc == 1 || !arg_has(argc, argv, "--ip") || arg_has(argc, argv, "--help")) {
        usage(argv[0]);
        return 0;
    }

    std::cout << "Device Info" << std::endl;
    std::cout << "-----------" << std::endl;
}

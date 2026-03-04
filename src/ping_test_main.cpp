#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <string>
#include <cmath>

static void usage(const char* prog){
    std::cerr <<
    "Usage: " << prog << " [options]\n"
    "  --in <path>       Input RAW int16 LE mono file to demodulate\n"
    "  --blocks <n>      Number of 4-FSK blocks for DATA (1,2,4,8,16; default 8)\n"
    "  --sps <samples>   Samples per DATA symbol (default 3000)\n"
    "  --help            Show this help\n";
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
    if (arg_has(argc, argv, "--help")) {
        usage(argv[0]);
        return 0;
    }

    std::cout << "Ping Test" << std::endl;
}

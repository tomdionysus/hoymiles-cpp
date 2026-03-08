#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

#include "HoymilesMicroinverter.hpp"

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " --ip <ipaddress> [--port <port>] <command>\n"
              << "\n"
              << "Commands:\n"
              << "  info\n"
              << "  real\n"
              << "  real-all\n"
              << "  heartbeat\n"
              << "  config\n"
              << "  network\n"
              << "  hist-power\n"
              << "  hist-ed\n"
              << "  gpst\n"
              << "  auto-search\n"
              << "  dev-config-fetch\n"
              << "  dev-config-put-status\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " --ip 192.168.1.50 info\n"
              << "  " << prog << " --ip 192.168.1.50 real\n"
              << "  " << prog << " --ip 192.168.1.50 hist-power\n"
              << "\n"
              << "Options:\n"
              << "  --ip <ipaddress>   IPv4 address of the device\n"
              << "  --port <port>      TCP port (default 10081)\n"
              << "  --help             Print usage/help\n";
}

static bool arg_has(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (key == argv[i]) {
            return true;
        }
    }
    return false;
}

static bool arg_get_str(int argc, char** argv, const std::string& key, std::string& out) {
    for (int i = 1; i < argc - 1; ++i) {
        if (key == argv[i]) {
            out = argv[i + 1];
            return true;
        }
    }
    return false;
}

static bool arg_get_u16(int argc, char** argv, const std::string& key, uint16_t& out) {
    std::string value;
    if (!arg_get_str(argc, argv, key, value)) {
        return false;
    }

    try {
        const unsigned long parsed = std::stoul(value);
        if (parsed > 65535UL) {
            return false;
        }
        out = static_cast<uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

static std::string get_command(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--help") {
            continue;
        }

        if (a == "--ip" || a == "--port") {
            ++i;
            continue;
        }

        if (!a.empty() && a.rfind("--", 0) == 0) {
            continue;
        }

        return a;
    }

    return "";
}

static std::string format_timestamp(std::uint32_t ts) {
    const std::time_t t = static_cast<std::time_t>(ts);
    std::tm tm{};
#if defined(_POSIX_VERSION)
    gmtime_r(&t, &tm);
#else
    tm = *std::gmtime(&t);
#endif

    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm) == 0) {
        return std::to_string(ts);
    }
    return std::string(buf);
}

static void print_info(const HoymilesMicroinverter::DecodedInfo& info) {
    std::cout << "Info\n";
    std::cout << "====\n";
    std::cout << "DTU serial number : " << info.dtu_serial_number << "\n";
    std::cout << "Timestamp         : " << format_timestamp(info.timestamp) << "\n";
    std::cout << "Device count      : " << info.device_number << "\n";
    std::cout << "PV input count    : " << info.pv_number << "\n";
    std::cout << "Package count     : " << info.package_number << "\n";
    std::cout << "Current package   : " << info.current_package << "\n";
    std::cout << "Channel           : " << info.channel << "\n";
    std::cout << "WiFi version      : " << info.wifi_version << "\n";
    std::cout << "Signal strength   : " << info.signal_strength << "\n";
    std::cout << "Device kind       : " << info.device_kind << "\n";
    std::cout << "DTU SW version    : " << info.dtu_sw_version << "\n";
    std::cout << "DTU HW version    : " << info.dtu_hw_version << "\n";

    if (!info.pv_info.empty()) {
        std::cout << "\nPV Inventory\n";
        std::cout << "------------\n";
        for (std::size_t i = 0; i < info.pv_info.size(); ++i) {
            const auto& pv = info.pv_info[i];
            std::cout << "PV[" << i << "]"
                      << " serial=" << pv.serial_number << " sw=" << pv.sw_version << " hw=" << pv.hw_version
                      << " grid_profile=" << pv.grid_profile << "\n";
        }
    }

    std::cout << "\n";
}

static void print_real_data(const HoymilesMicroinverter::DecodedRealData& data) {
    std::cout << "Realtime\n";
    std::cout << "========\n";
    std::cout << "Device serial      : " << data.device_serial_number << "\n";
    std::cout << "Timestamp          : " << format_timestamp(data.timestamp) << "\n";
    std::cout << "AP                 : " << data.ap << "\n";
    std::cout << "CP                 : " << data.cp << "\n";
    std::cout << "Firmware version   : " << data.firmware_version << "\n";
    std::cout << "Inverter power     : " << std::fixed << std::setprecision(1) << data.dtu_power_w << " W\n";
    std::cout << "Daily energy       : " << std::fixed << std::setprecision(0) << data.dtu_daily_energy_wh << " Wh\n";

    if (!data.sgs_data.empty()) {
        std::cout << "\nAC / Inverter Status\n";
        std::cout << "--------------------\n";
        for (std::size_t i = 0; i < data.sgs_data.size(); ++i) {
            const auto& s = data.sgs_data[i];
            std::cout << "SGS[" << i << "]\n";
            std::cout << "  Serial           : " << s.serial_number << "\n";
            std::cout << "  Grid voltage     : " << std::fixed << std::setprecision(1) << s.voltage_v << " V\n";
            std::cout << "  Grid frequency   : " << std::fixed << std::setprecision(2) << s.frequency_hz << " Hz\n";
            std::cout << "  Active power     : " << std::fixed << std::setprecision(1) << s.active_power_w << " W\n";
            std::cout << "  Reactive power   : " << std::fixed << std::setprecision(1) << s.reactive_power_var
                      << " var\n";
            std::cout << "  AC current       : " << std::fixed << std::setprecision(1) << s.current_a << " A\n";
            std::cout << "  Power factor     : " << std::fixed << std::setprecision(3) << s.power_factor << "\n";
            std::cout << "  Temperature      : " << std::fixed << std::setprecision(1) << s.temperature_c << " C\n";
            std::cout << "  Online           : " << (s.online ? "yes" : "no") << "\n";
            std::cout << "  Warning code     : " << s.raw_warning_number << "\n";
            std::cout << "  Power limit raw  : " << s.raw_power_limit << "\n";
        }
    }

    if (!data.pv_data.empty()) {
        std::cout << "\nPV Inputs\n";
        std::cout << "---------\n";
        for (std::size_t i = 0; i < data.pv_data.size(); ++i) {
            const auto& pv = data.pv_data[i];
            std::cout << "PV[" << i << "]\n";
            std::cout << "  Serial           : " << pv.serial_number << "\n";
            std::cout << "  Port             : " << pv.port_number << "\n";
            std::cout << "  Voltage          : " << std::fixed << std::setprecision(1) << pv.voltage_v << " V\n";
            std::cout << "  Current          : " << std::fixed << std::setprecision(1) << pv.current_a << " A\n";
            std::cout << "  Power            : " << std::fixed << std::setprecision(1) << pv.power_w << " W\n";
            std::cout << "  Daily energy     : " << std::fixed << std::setprecision(0) << pv.energy_daily_wh << " Wh\n";
            std::cout << "  Total energy     : " << std::fixed << std::setprecision(1) << pv.energy_total_kwh
                      << " kWh\n";
            std::cout << "  Error code       : " << pv.error_code << "\n";
        }
    }

    std::cout << "\n";
}

template <typename ProtoT> static void print_debug_message(const std::string& title, const ProtoT& msg) {
    std::cout << title << "\n";
    for (std::size_t i = 0; i < title.size(); ++i) {
        std::cout << "=";
    }
    std::cout << "\n";
    std::cout << msg.DebugString() << "\n";
}

int main(int argc, char** argv) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    try {
        if (argc == 1 || arg_has(argc, argv, "--help")) {
            usage(argv[0]);
            google::protobuf::ShutdownProtobufLibrary();
            return 0;
        }

        std::string ip;
        if (!arg_get_str(argc, argv, "--ip", ip) || ip.empty()) {
            usage(argv[0]);
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }

        uint16_t port = 10081;
        if (arg_has(argc, argv, "--port") && !arg_get_u16(argc, argv, "--port", port)) {
            std::cerr << "error: invalid --port\n";
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }

        const std::string command = get_command(argc, argv);
        if (command.empty()) {
            usage(argv[0]);
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }

        HoymilesMicroinverter inverter(ip, port);
        inverter.connect();

        if (command == "info") {
            print_info(inverter.get_decoded_info());
        } else if (command == "real") {
            print_real_data(inverter.get_decoded_real_data());
        } else if (command == "real-all") {
            const auto raw = inverter.get_real_data_complete();
            print_real_data(HoymilesMicroinverter::decode_real_data(raw));
        } else if (command == "heartbeat") {
            print_debug_message("Heartbeat", inverter.get_heartbeat());
        } else if (command == "config") {
            print_debug_message("Config", inverter.get_config());
        } else if (command == "network") {
            print_debug_message("Network Info", inverter.get_network_info());
        } else if (command == "hist-power") {
            print_debug_message("Historical Power", inverter.get_hist_power());
        } else if (command == "hist-ed") {
            print_debug_message("Historical Energy Daily", inverter.get_hist_energy_daily());
        } else if (command == "gpst") {
            print_debug_message("GPST Data", inverter.get_gpst_data());
        } else if (command == "auto-search") {
            print_debug_message("Auto Search", inverter.get_auto_search());
        } else if (command == "dev-config-fetch") {
            print_debug_message("Dev Config Fetch", inverter.get_dev_config_fetch());
        } else if (command == "dev-config-put-status") {
            print_debug_message("Dev Config Put Status", inverter.get_dev_config_put_status());
        } else {
            inverter.disconnect();
            std::cerr << "error: unknown command: " << command << "\n\n";
            usage(argv[0]);
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }

        inverter.disconnect();
        google::protobuf::ShutdownProtobufLibrary();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        google::protobuf::ShutdownProtobufLibrary();
        return 1;
    }
}
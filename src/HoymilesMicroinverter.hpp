#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "APPInfomationData.pb.h"
#include "RealDataNew.pb.h"

class HoymilesMicroinverter {
public:
    struct DecodedPvInfo {
        std::string serial_number;
        int sw_version = 0;
        int hw_version = 0;
        int grid_profile = 0;
    };

    struct DecodedInfo {
        std::string dtu_serial_number;
        std::uint32_t timestamp = 0;
        int device_number = 0;
        int pv_number = 0;
        int package_number = 0;
        int current_package = 0;
        int channel = 0;

        std::string wifi_version;
        int signal_strength = 0;
        int device_kind = 0;
        int dtu_sw_version = 0;
        int dtu_hw_version = 0;

        std::vector<DecodedPvInfo> pv_info;
    };

    struct DecodedSgsData {
        std::string serial_number;

        int raw_voltage = 0;
        int raw_frequency = 0;
        int raw_active_power = 0;
        int raw_reactive_power = 0;
        int raw_current = 0;
        int raw_power_factor = 0;
        int raw_temperature = 0;
        int raw_warning_number = 0;
        int raw_link_status = 0;
        int raw_power_limit = 0;

        double voltage_v = 0.0;        // raw / 10
        double frequency_hz = 0.0;     // raw / 100
        double active_power_w = 0.0;   // raw
        double reactive_power_var = 0.0; // raw
        double current_a = 0.0;        // raw / 10
        double power_factor = 0.0;     // raw / 1000 (best current guess)
        double temperature_c = 0.0;    // raw
        bool online = false;           // raw_link_status != 0
    };

    struct DecodedPvData {
        std::string serial_number;
        int port_number = 0;

        int raw_voltage = 0;
        int raw_current = 0;
        int raw_power = 0;
        int raw_energy_total = 0;
        int raw_energy_daily = 0;
        int raw_error_code = 0;

        double voltage_v = 0.0;          // raw / 10
        double current_a = 0.0;          // raw / 10
        double power_w = 0.0;            // raw
        double energy_total_kwh = 0.0;   // raw / 10
        double energy_daily_wh = 0.0;    // raw
        int error_code = 0;
    };

    struct DecodedRealData {
        std::string device_serial_number;
        std::uint32_t timestamp = 0;
        int ap = 0;
        int cp = 0;
        int firmware_version = 0;

        int raw_dtu_power = 0;
        int raw_dtu_daily_energy = 0;

        double dtu_power_w = 0.0;         // raw
        double dtu_daily_energy_wh = 0.0; // raw

        std::vector<DecodedSgsData> sgs_data;
        std::vector<DecodedPvData> pv_data;
    };

    HoymilesMicroinverter(std::string ip, uint16_t port = 10081)
        : ip_(std::move(ip)),
          port_(port),
          socket_fd_(-1),
          next_sequence_(1) {}

    ~HoymilesMicroinverter() {
        disconnect();
    }

    HoymilesMicroinverter(const HoymilesMicroinverter&) = delete;
    HoymilesMicroinverter& operator=(const HoymilesMicroinverter&) = delete;

    HoymilesMicroinverter(HoymilesMicroinverter&& other) noexcept
        : ip_(std::move(other.ip_)),
          port_(other.port_),
          socket_fd_(other.socket_fd_),
          next_sequence_(other.next_sequence_) {
        other.socket_fd_ = -1;
        other.next_sequence_ = 1;
    }

    HoymilesMicroinverter& operator=(HoymilesMicroinverter&& other) noexcept {
        if (this != &other) {
            disconnect();

            ip_ = std::move(other.ip_);
            port_ = other.port_;
            socket_fd_ = other.socket_fd_;
            next_sequence_ = other.next_sequence_;

            other.socket_fd_ = -1;
            other.next_sequence_ = 1;
        }
        return *this;
    }

    void connect() {
        if (socket_fd_ >= 0) {
            return;
        }

        socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("socket() failed");
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);

        if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) != 1) {
            ::close(socket_fd_);
            socket_fd_ = -1;
            throw std::runtime_error("invalid IPv4 address: " + ip_);
        }

        if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            const int e = errno;
            ::close(socket_fd_);
            socket_fd_ = -1;
            throw std::runtime_error("connect() failed: " + std::string(std::strerror(e)));
        }
    }

    void disconnect() {
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
    }

    bool is_connected() const noexcept {
        return socket_fd_ >= 0;
    }

    APPInfoDataReqDTO get_info() {
        ensure_connected();

        const uint16_t seq = next_seq();
        const std::vector<uint8_t> payload = serialize_info_request();
        const std::vector<uint8_t> frame = build_frame(payload, CMD_APP_INFO, seq);

        write_all(frame);
        Frame reply = read_frame();

        APPInfoDataReqDTO response;
        if (!response.ParseFromArray(reply.payload.data(), static_cast<int>(reply.payload.size()))) {
            throw std::runtime_error("failed to parse APPInfoDataReqDTO");
        }

        return response;
    }

    RealDataNewReqDTO get_real_data() {
        ensure_connected();

        const uint16_t seq = next_seq();
        const std::vector<uint8_t> payload = serialize_real_data_request();
        const std::vector<uint8_t> frame = build_frame(payload, CMD_REAL_DATA, seq);

        write_all(frame);
        Frame reply = read_frame();

        RealDataNewReqDTO response;
        if (!response.ParseFromArray(reply.payload.data(), static_cast<int>(reply.payload.size()))) {
            throw std::runtime_error("failed to parse RealDataNewReqDTO");
        }

        return response;
    }

    DecodedInfo get_decoded_info() {
        return decode_info(get_info());
    }

    DecodedRealData get_decoded_real_data() {
        return decode_real_data(get_real_data());
    }

    static DecodedInfo decode_info(const APPInfoDataReqDTO& msg) {
        DecodedInfo out;
        out.dtu_serial_number = msg.dtu_serial_number();
        out.timestamp = static_cast<std::uint32_t>(msg.timestamp());
        out.device_number = msg.device_number();
        out.pv_number = msg.pv_number();
        out.package_number = msg.package_number();
        out.current_package = msg.current_package();
        out.channel = msg.channel();

        if (msg.has_dtu_info()) {
            const APPDtuInfoMO& dtu = msg.dtu_info();
            out.wifi_version = dtu.wifi_version();
            out.signal_strength = dtu.signal_strength();
            out.device_kind = dtu.device_kind();
            out.dtu_sw_version = dtu.dtu_sw_version();
            out.dtu_hw_version = dtu.dtu_hw_version();
        }

        out.pv_info.reserve(static_cast<std::size_t>(msg.pv_info_size()));
        for (int i = 0; i < msg.pv_info_size(); ++i) {
            const APPPvInfoMO& pv = msg.pv_info(i);
            DecodedPvInfo item;
            item.serial_number = pv.pv_serial_number();
            item.sw_version = pv.pv_sw_version();
            item.hw_version = pv.pv_hw_version();
            item.grid_profile = pv.pv_grid_profile();
            out.pv_info.push_back(item);
        }

        return out;
    }

    static DecodedRealData decode_real_data(const RealDataNewReqDTO& msg) {
        DecodedRealData out;
        out.device_serial_number = msg.device_serial_number();
        out.timestamp = static_cast<std::uint32_t>(msg.timestamp());
        out.ap = msg.ap();
        out.cp = msg.cp();
        out.firmware_version = msg.firmware_version();

        out.raw_dtu_power = msg.dtu_power();
        out.raw_dtu_daily_energy = msg.dtu_daily_energy();
        out.dtu_power_w = static_cast<double>(msg.dtu_power());
        out.dtu_daily_energy_wh = static_cast<double>(msg.dtu_daily_energy());

        out.sgs_data.reserve(static_cast<std::size_t>(msg.sgs_data_size()));
        for (int i = 0; i < msg.sgs_data_size(); ++i) {
            const SGSMO& sgs = msg.sgs_data(i);

            DecodedSgsData item;
            item.serial_number = sgs.serial_number();

            item.raw_voltage = sgs.voltage();
            item.raw_frequency = sgs.frequency();
            item.raw_active_power = sgs.active_power();
            item.raw_reactive_power = sgs.reactive_power();
            item.raw_current = sgs.current();
            item.raw_power_factor = sgs.power_factor();
            item.raw_temperature = sgs.temperature();
            item.raw_warning_number = sgs.warning_number();
            item.raw_link_status = sgs.link_status();
            item.raw_power_limit = sgs.power_limit();

            item.voltage_v = static_cast<double>(sgs.voltage()) / 10.0;
            item.frequency_hz = static_cast<double>(sgs.frequency()) / 100.0;
            item.active_power_w = static_cast<double>(sgs.active_power());
            item.reactive_power_var = static_cast<double>(sgs.reactive_power());
            item.current_a = static_cast<double>(sgs.current()) / 10.0;
            item.power_factor = static_cast<double>(sgs.power_factor()) / 1000.0;
            item.temperature_c = (static_cast<double>(sgs.temperature()) - 32.0) * (5.0 / 9.0);
            item.online = (sgs.link_status() != 0);

            out.sgs_data.push_back(item);
        }

        out.pv_data.reserve(static_cast<std::size_t>(msg.pv_data_size()));
        for (int i = 0; i < msg.pv_data_size(); ++i) {
            const PvMO& pv = msg.pv_data(i);

            DecodedPvData item;
            item.serial_number = pv.serial_number();
            item.port_number = pv.port_number();

            item.raw_voltage = pv.voltage();
            item.raw_current = pv.current();
            item.raw_power = pv.power();
            item.raw_energy_total = pv.energy_total();
            item.raw_energy_daily = pv.energy_daily();
            item.raw_error_code = pv.error_code();

            item.voltage_v = static_cast<double>(pv.voltage()) / 10.0;
            item.current_a = static_cast<double>(pv.current()) / 10.0;
            item.power_w = static_cast<double>(pv.power());
            item.energy_total_kwh = static_cast<double>(pv.energy_total()) / 10.0;
            item.energy_daily_wh = static_cast<double>(pv.energy_daily());
            item.error_code = pv.error_code();

            out.pv_data.push_back(item);
        }

        return out;
    }

private:
    struct Frame {
        uint16_t command = 0;
        uint16_t sequence = 0;
        uint16_t crc = 0;
        uint16_t total_len = 0;
        std::vector<uint8_t> payload;
    };

    static constexpr uint16_t CMD_APP_INFO  = 0xA301;
    static constexpr uint16_t CMD_REAL_DATA = 0xA311;

    std::string ip_;
    uint16_t port_;
    int socket_fd_;
    uint16_t next_sequence_;

    static uint16_t crc16_modbus(const uint8_t* data, std::size_t len) {
        uint16_t crc = 0xFFFF;

        for (std::size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint16_t>(data[i]);
            for (int bit = 0; bit < 8; ++bit) {
                if ((crc & 0x0001U) != 0U) {
                    crc = static_cast<uint16_t>((crc >> 1U) ^ 0xA001U);
                } else {
                    crc = static_cast<uint16_t>(crc >> 1U);
                }
            }
        }

        return crc;
    }

    static void append_u16_be(std::vector<uint8_t>& out, uint16_t value) {
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    static uint16_t read_u16_be(const uint8_t* data) {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(data[0]) << 8) |
            static_cast<uint16_t>(data[1]));
    }

    static std::vector<uint8_t> serialize_info_request() {
        APPInfoDataResDTO req;
        req.set_time_ymd_hms("1970-01-01 00:00:00");
        req.set_offset(0);
        req.set_current_package(0);
        req.set_error_code(0);
        req.set_time(static_cast<uint32_t>(std::time(nullptr)));

        std::string bytes;
        if (!req.SerializeToString(&bytes)) {
            throw std::runtime_error("failed to serialize APPInfoDataResDTO");
        }

        return std::vector<uint8_t>(bytes.begin(), bytes.end());
    }

    static std::vector<uint8_t> serialize_real_data_request() {
        RealDataNewResDTO req;
        req.set_time_ymd_hms("1970-01-01 00:00:00");
        req.set_cp(0);
        req.set_error_code(0);
        req.set_offset(0);
        req.set_time(static_cast<int32_t>(std::time(nullptr)));

        std::string bytes;
        if (!req.SerializeToString(&bytes)) {
            throw std::runtime_error("failed to serialize RealDataNewResDTO");
        }

        return std::vector<uint8_t>(bytes.begin(), bytes.end());
    }

    std::vector<uint8_t> build_frame(
        const std::vector<uint8_t>& payload,
        uint16_t command,
        uint16_t sequence) const {

        std::vector<uint8_t> frame;
        frame.reserve(10 + payload.size());

        frame.push_back(0x48);
        frame.push_back(0x4D);
        append_u16_be(frame, command);
        append_u16_be(frame, sequence);

        const uint16_t crc = crc16_modbus(payload.data(), payload.size());
        append_u16_be(frame, crc);

        const uint16_t total_len = static_cast<uint16_t>(10 + payload.size());
        append_u16_be(frame, total_len);

        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    void write_all(const std::vector<uint8_t>& data) {
        ensure_connected();

        std::size_t offset = 0;
        while (offset < data.size()) {
            const ssize_t n = ::send(socket_fd_, data.data() + offset, data.size() - offset, 0);
            if (n <= 0) {
                throw std::runtime_error("send() failed");
            }
            offset += static_cast<std::size_t>(n);
        }
    }

    std::vector<uint8_t> read_exact(std::size_t len) {
        ensure_connected();

        std::vector<uint8_t> out(len);
        std::size_t offset = 0;

        while (offset < len) {
            const ssize_t n = ::recv(socket_fd_, out.data() + offset, len - offset, 0);
            if (n <= 0) {
                throw std::runtime_error("recv() failed");
            }
            offset += static_cast<std::size_t>(n);
        }

        return out;
    }

    Frame read_frame() {
        const std::vector<uint8_t> hdr = read_exact(10);

        if (hdr[0] != 0x48 || hdr[1] != 0x4D) {
            throw std::runtime_error("bad frame header");
        }

        Frame frame;
        frame.command = read_u16_be(&hdr[2]);
        frame.sequence = read_u16_be(&hdr[4]);
        frame.crc = read_u16_be(&hdr[6]);
        frame.total_len = read_u16_be(&hdr[8]);

        if (frame.total_len < 10) {
            throw std::runtime_error("invalid frame length");
        }

        const std::size_t payload_len = static_cast<std::size_t>(frame.total_len - 10);
        frame.payload = read_exact(payload_len);

        const uint16_t actual_crc = crc16_modbus(frame.payload.data(), frame.payload.size());
        if (actual_crc != frame.crc) {
            throw std::runtime_error("CRC mismatch");
        }

        return frame;
    }

    uint16_t next_seq() {
        const uint16_t current = next_sequence_;
        ++next_sequence_;
        if (next_sequence_ == 0) {
            next_sequence_ = 1;
        }
        return current;
    }

    void ensure_connected() const {
        if (socket_fd_ < 0) {
            throw std::runtime_error("not connected");
        }
    }
};
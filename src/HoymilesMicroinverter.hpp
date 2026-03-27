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

#include "APPHeartbeatPB.pb.h"
#include "APPInfomationData.pb.h"
#include "AppGetHistED.pb.h"
#include "AppGetHistPower.pb.h"
#include "AutoSearch.pb.h"
#include "DevConfig.pb.h"
#include "GPSTData.pb.h"
#include "GetConfig.pb.h"
#include "NetworkInfo.pb.h"
#include "RealData.pb.h"
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

        double voltage_v = 0.0;
        double frequency_hz = 0.0;
        double active_power_w = 0.0;
        double reactive_power_var = 0.0;
        double current_a = 0.0;
        double power_factor = 0.0;
        double temperature_c = 0.0;
        bool online = false;
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

        double voltage_v = 0.0;
        double current_a = 0.0;
        double power_w = 0.0;
        double energy_total_kwh = 0.0;
        double energy_daily_wh = 0.0;
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
        : ip_(std::move(ip)), port_(port), socket_fd_(-1), next_sequence_(1) {}

    ~HoymilesMicroinverter() { disconnect(); }

    HoymilesMicroinverter(const HoymilesMicroinverter&) = delete;
    HoymilesMicroinverter& operator=(const HoymilesMicroinverter&) = delete;

    HoymilesMicroinverter(HoymilesMicroinverter&& other) noexcept
        : ip_(std::move(other.ip_)), port_(other.port_), socket_fd_(other.socket_fd_),
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

    bool is_connected() const noexcept { return socket_fd_ >= 0; }

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

    DecodedInfo get_decoded_info() { return decode_info(get_info()); }

    DecodedRealData get_decoded_real_data() { return decode_real_data(get_real_data()); }

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
            item.active_power_w = static_cast<double>(sgs.active_power()) / 10.0;
            item.reactive_power_var = static_cast<double>(sgs.reactive_power()) / 10.0;
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
            item.current_a = static_cast<double>(pv.current()) / 100.0;
            item.power_w = static_cast<double>(pv.power()) / 10.0;
            item.energy_total_kwh = static_cast<double>(pv.energy_total()) / 10.0;
            item.energy_daily_wh = static_cast<double>(pv.energy_daily());
            item.error_code = pv.error_code();

            out.pv_data.push_back(item);
        }

        return out;
    }

    HBReqDTO get_heartbeat() {
        ensure_connected();

        HBResDTO req;
        req.set_offset(0);
        req.set_time(static_cast<int32_t>(std::time(nullptr)));
        req.set_time_ymd_hms("1970-01-01 00:00:00");

        return send_request<HBResDTO, HBReqDTO>(CMD_HEARTBEAT, req);
    }

    RealDataReqDTO get_real_data_legacy() {
        ensure_connected();

        RealDataResDTO req;
        req.set_time_ymd_hms("1970-01-01 00:00:00");
        req.set_package_now(0);
        req.set_error_code(0);
        req.set_offset(0);
        req.set_time(static_cast<int32_t>(std::time(nullptr)));

        return send_request<RealDataResDTO, RealDataReqDTO>(CMD_REAL_DATA_LEGACY, req);
    }

    RealDataNewReqDTO get_real_data_complete() {
        ensure_connected();

        RealDataNewReqDTO combined;

        for (int cp = 0;; ++cp) {
            RealDataNewResDTO req;
            req.set_time_ymd_hms("1970-01-01 00:00:00");
            req.set_cp(cp);
            req.set_error_code(0);
            req.set_offset(0);
            req.set_time(static_cast<int32_t>(std::time(nullptr)));

            RealDataNewReqDTO part = send_request<RealDataNewResDTO, RealDataNewReqDTO>(CMD_REAL_DATA, req);

            if (cp == 0) {
                combined = part;
                if (part.ap() <= 1) {
                    break;
                }
            } else {
                combined.mutable_meter_data()->MergeFrom(part.meter_data());
                combined.mutable_rp_data()->MergeFrom(part.rp_data());
                combined.mutable_rsd_data()->MergeFrom(part.rsd_data());
                combined.mutable_sgs_data()->MergeFrom(part.sgs_data());
                combined.mutable_tgs_data()->MergeFrom(part.tgs_data());
                combined.mutable_pv_data()->MergeFrom(part.pv_data());
            }

            if (cp + 1 >= combined.ap()) {
                break;
            }
        }

        return combined;
    }

    GetConfigReqDTO get_config() {
        ensure_connected();

        GetConfigResDTO req;
        req.set_offset(0);
        req.set_time(static_cast<uint32_t>(std::time(nullptr) - 60));

        return send_request<GetConfigResDTO, GetConfigReqDTO>(CMD_GET_CONFIG, req);
    }

    NetworkInfoReqDTO get_network_info() {
        ensure_connected();

        NetworkInfoResDTO req;
        req.set_offset(0);
        req.set_time(static_cast<uint32_t>(std::time(nullptr)));

        return send_request<NetworkInfoResDTO, NetworkInfoReqDTO>(CMD_NETWORK_INFO, req);
    }

    AppGetHistPowerReqDTO get_hist_power(std::uint32_t requested_time = 0, std::uint32_t requested_day = 0) {
        ensure_connected();

        if (requested_time == 0) {
            requested_time = static_cast<std::uint32_t>(std::time(nullptr));
        }

        AppGetHistPowerReqDTO combined;
        bool first = true;
        std::uint32_t initial_absolute_start = 0;

        for (int cp = 0;; ++cp) {
            AppGetHistPowerResDTO req;
            req.set_cp(cp);
            req.set_offset(0);
            req.set_requested_time(requested_time);
            req.set_requested_day(requested_day);

            AppGetHistPowerReqDTO part =
                send_request<AppGetHistPowerResDTO, AppGetHistPowerReqDTO>(CMD_APP_HIST_POWER, req);

            if (first) {
                combined = part;
                initial_absolute_start = part.absolute_start();
                first = false;

                if (part.ap() <= 1) {
                    break;
                }
            } else {
                combined.mutable_power_array()->MergeFrom(part.power_array());
                combined.set_ap(part.ap());
                combined.set_cp(part.cp());
                combined.set_offset(part.offset());
                combined.set_request_time(part.request_time());
                combined.set_start_time(part.start_time());
                combined.set_long_term_start(part.long_term_start());
                combined.set_step_time(part.step_time());
                combined.set_relative_power(part.relative_power());
                combined.set_total_energy(part.total_energy());
                combined.set_daily_energy(part.daily_energy());
                combined.set_warning_number(part.warning_number());
            }

            if (cp + 1 >= combined.ap()) {
                break;
            }
        }

        combined.set_absolute_start(initial_absolute_start);
        return combined;
    }

    AppGetHistEDReqDTO get_hist_energy_daily() {
        ensure_connected();

        AppGetHistEDResDTO req;
        req.set_cp(0);
        req.set_oft(0);
        req.set_time(static_cast<uint32_t>(std::time(nullptr)));

        return send_request<AppGetHistEDResDTO, AppGetHistEDReqDTO>(CMD_APP_HIST_ED, req);
    }

    AutoSearchReqDTO get_auto_search() {
        ensure_connected();

        AutoSearchResDTO req;
        req.set_ymd_hms("1970-01-01 00:00:00");
        req.set_offset(0);
        req.set_current_package(0);
        req.set_error_code(0);
        req.set_time(static_cast<int32_t>(std::time(nullptr)));

        return send_request<AutoSearchResDTO, AutoSearchReqDTO>(CMD_AUTO_SEARCH, req);
    }

    GPSTReqDTO get_gpst_data() {
        ensure_connected();

        GPSTResDTO req;
        req.set_ymd_hms("1970-01-01 00:00:00");
        req.set_offset(0);
        req.set_package_now(0);
        req.set_err_code(0);
        req.set_time(static_cast<int32_t>(std::time(nullptr)));

        return send_request<GPSTResDTO, GPSTReqDTO>(CMD_GPST_DATA, req);
    }

    DevConfigFetchReqDTO get_dev_config_fetch() {
        ensure_connected();

        DevConfigFetchResDTO req;
        req.set_response_time(static_cast<int32_t>(std::time(nullptr)));
        req.set_transaction_id(static_cast<std::int64_t>(std::time(nullptr)));
        req.set_dtu_sn("");
        req.set_dev_sn("");
        req.set_current_package(0);
        req.set_rule_type(0);

        return send_request<DevConfigFetchResDTO, DevConfigFetchReqDTO>(CMD_DEV_CONFIG_FETCH, req);
    }

    DevConfigPutReqDTO get_dev_config_put_status() {
        ensure_connected();

        DevConfigPutResDTO req;
        req.set_response_time(static_cast<int32_t>(std::time(nullptr)));
        req.set_transaction_id(static_cast<std::int64_t>(std::time(nullptr)));
        req.set_rule_id(0);
        req.set_data("");
        req.set_crc(0);
        req.set_dtu_sn("");
        req.set_dev_sn("");
        req.set_cfg_data("");
        req.set_cfg_crc(0);
        req.set_total_packages(0);
        req.set_current_package(0);
        req.set_rule_type(0);

        return send_request<DevConfigPutResDTO, DevConfigPutReqDTO>(CMD_DEV_CONFIG_PUT, req);
    }

  private:
    struct Frame {
        uint16_t command = 0;
        uint16_t sequence = 0;
        uint16_t crc = 0;
        uint16_t total_len = 0;
        std::vector<uint8_t> payload;
    };

    static constexpr uint16_t CMD_APP_INFO = 0xA301;
    static constexpr uint16_t CMD_HEARTBEAT = 0xA302;
    static constexpr uint16_t CMD_REAL_DATA_LEGACY = 0xA303;
    static constexpr uint16_t CMD_COMMAND_STATUS = 0xA306;
    static constexpr uint16_t CMD_DEV_CONFIG_FETCH = 0xA307;
    static constexpr uint16_t CMD_DEV_CONFIG_PUT = 0xA308;
    static constexpr uint16_t CMD_GET_CONFIG = 0xA309;
    static constexpr uint16_t CMD_SET_CONFIG = 0xA310;
    static constexpr uint16_t CMD_REAL_DATA = 0xA311;
    static constexpr uint16_t CMD_GPST_DATA = 0xA312;
    static constexpr uint16_t CMD_AUTO_SEARCH = 0xA313;
    static constexpr uint16_t CMD_NETWORK_INFO = 0xA314;
    static constexpr uint16_t CMD_APP_HIST_POWER = 0xA315;
    static constexpr uint16_t CMD_APP_HIST_ED = 0xA316;

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
        return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]));
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

    std::vector<uint8_t> build_frame(const std::vector<uint8_t>& payload, uint16_t command, uint16_t sequence) const {

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

    template <typename RequestT, typename ResponseT> ResponseT send_request(uint16_t command, const RequestT& request) {
        std::string bytes;
        if (!request.SerializeToString(&bytes)) {
            throw std::runtime_error("failed to serialize protobuf request");
        }

        const std::vector<uint8_t> payload(bytes.begin(), bytes.end());
        const uint16_t seq = next_seq();
        const std::vector<uint8_t> frame = build_frame(payload, command, seq);

        write_all(frame);
        Frame reply = read_frame();

        ResponseT response;
        if (!response.ParseFromArray(reply.payload.data(), static_cast<int>(reply.payload.size()))) {
            throw std::runtime_error("failed to parse protobuf response");
        }

        return response;
    }
};
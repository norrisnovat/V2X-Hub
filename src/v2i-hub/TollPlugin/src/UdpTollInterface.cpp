/**
 * UdpTollInterface.cpp — Direct UDP interface for VISSIM/OBU simulation.
 */
#include "UdpTollInterface.h"
#include "J3217Codec.h"

#include <PluginLog.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

using namespace tmx::utils;

namespace v2x { namespace toll {

namespace {
bool isHexString(const char* data, int len) {
    if (len <= 0 || (len % 2) != 0) return false;
    for (int i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

template <typename T>
T getAny(const boost::property_tree::ptree& pt, const std::string& key, T def) {
    auto direct = pt.get_optional<T>(key);
    if (direct) return *direct;
    auto payload = pt.get_child_optional("payload");
    if (payload) {
        auto nested = payload->get_optional<T>(key);
        if (nested) return *nested;
    }
    return def;
}
}

UdpTollInterface::UdpTollInterface(TumHandler handler)
    : _handler(std::move(handler))
{
}

UdpTollInterface::~UdpTollInterface() {
    try {
        stop();
    } catch (...) {
        if (_listener.joinable()) {
            _listener.detach();
        }
    }
}

void UdpTollInterface::updateConfig(const TollPluginConfig& cfg) {
    bool needsRestart = false;
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        needsRestart = cfg.enableUdpInterface != _cfg.enableUdpInterface ||
                       cfg.udpBindHost        != _cfg.udpBindHost ||
                       cfg.tumUdpPort         != _cfg.tumUdpPort;
        _cfg = cfg;
    }
    if (needsRestart) {
        _restartLocked(cfg);
    }
}

void UdpTollInterface::stop() {
    _enabled = false;
    _running = false;

    TollPluginConfig cfg;
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        cfg = _cfg;
    }

    if (cfg.enableUdpInterface && cfg.tumUdpPort > 0) {
        int wakeSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (wakeSock >= 0) {
            sockaddr_in dst{};
            dst.sin_family = AF_INET;
            dst.sin_port = htons(static_cast<uint16_t>(cfg.tumUdpPort));
            const char* wakeHost = (cfg.udpBindHost.empty() || cfg.udpBindHost == "0.0.0.0")
                                       ? "127.0.0.1"
                                       : cfg.udpBindHost.c_str();
            if (inet_pton(AF_INET, wakeHost, &dst.sin_addr) == 1) {
                const char wakeByte = 0;
                sendto(wakeSock, &wakeByte, 1, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
            }
            close(wakeSock);
        }
    }

    if (_listener.joinable()) {
        try {
            if (_listener.get_id() == std::this_thread::get_id()) {
                _listener.detach();
            } else {
                _listener.join();
            }
        } catch (...) {
            if (_listener.joinable()) {
                _listener.detach();
            }
        }
    }
}

void UdpTollInterface::_restartLocked(const TollPluginConfig& cfg) {
    stop();
    if (!cfg.enableUdpInterface) {
        PLUGIN_LOG(logINFO, "UdpTollInterface") << "UDP interface disabled";
        return;
    }

    _enabled = true;
    _running = true;
    _listener = std::thread(&UdpTollInterface::_listenLoop, this, cfg);
    PLUGIN_LOG(logINFO, "UdpTollInterface") << "UDP interface enabled: bind="
                                            << cfg.udpBindHost << ":" << cfg.tumUdpPort
                                            << " OBE=" << cfg.obeHost
                                            << " TAM:" << cfg.tamUdpPort
                                            << " TumAck:" << cfg.tumAckUdpPort
                                            << " encoding=" << cfg.udpEncodingMode;
}

void UdpTollInterface::_listenLoop(TollPluginConfig cfg) {
    try {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << "socket() failed: " << strerror(errno);
        _running = false;
        return;
    }
    _sock = sock;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(cfg.tumUdpPort));
    if (cfg.udpBindHost.empty() || cfg.udpBindHost == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, cfg.udpBindHost.c_str(), &addr.sin_addr) != 1) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << "Invalid UdpBindHost: " << cfg.udpBindHost;
        close(sock);
        _sock = -1;
        _running = false;
        return;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << "bind(" << cfg.udpBindHost << ":"
                                                 << cfg.tumUdpPort << ") failed: "
                                                 << strerror(errno);
        close(sock);
        _sock = -1;
        _running = false;
        return;
    }

    while (_running) {
        char buf[8192];
        sockaddr_in peer{};
        socklen_t peerLen = sizeof(peer);
        int n = recvfrom(sock, buf, sizeof(buf), 0,
                         reinterpret_cast<sockaddr*>(&peer), &peerLen);
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (_running) ++_errors;
            continue;
        }
        if (!_running) {
            break;
        }

        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
        std::ostringstream peerText;
        peerText << ip << ":" << ntohs(peer.sin_port);
        _setLastPeer(peerText.str());

        TollPluginConfig current;
        {
            std::lock_guard<std::mutex> lock(_configMutex);
            current = _cfg;
        }

        TumMessage tum;
        if (!_decodeTum(buf, n, current, tum)) {
            ++_errors;
            PLUGIN_LOG(logWARNING, "UdpTollInterface") << "UDP TUM decode failed from " << peerText.str();
            continue;
        }

        ++_tumReceived;
        if (_handler) {
            _handler(tum);
        }
    }

    close(sock);
    _sock = -1;
    } catch (const std::exception& e) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << "UDP listener stopped after exception: " << e.what();
        _running = false;
    } catch (...) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << "UDP listener stopped after unknown exception";
        _running = false;
    }
}

bool UdpTollInterface::_decodeTum(const char* data, int len, const TollPluginConfig& cfg, TumMessage& out) {
    if (cfg.udpEncodingMode == "JSON") {
        return _decodeJsonTum(std::string(data, data + len), out);
    }
    if (cfg.udpEncodingMode == "J3217_UPER") {
        return _decodeUperTum(data, len, out);
    }
    return _decodeJsonTum(std::string(data, data + len), out) || _decodeUperTum(data, len, out);
}

bool UdpTollInterface::_decodeJsonTum(const std::string& json, TumMessage& out) {
    try {
        std::stringstream ss(json);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        out.set_msgType(getAny<std::string>(pt, "msgType", "TUM"));
        out.set_tumSequenceNum(getAny<int>(pt, "tumSequenceNum", 0));
        out.set_tamSequenceNum(getAny<int>(pt, "tamSequenceNum", 0));
        out.set_tempID(getAny<std::string>(pt, "tempID", ""));
        out.set_vehicleClass(getAny<std::string>(pt, "vehicleClass", ""));
        out.set_vehicleType(getAny<int>(pt, "vehicleType", 0));
        out.set_tollPointID(getAny<std::string>(pt, "tollPointID", ""));
        out.set_tollChargerID(getAny<std::string>(pt, "tollChargerID", ""));
        out.set_laneID(getAny<int>(pt, "laneID", 0));
        out.set_laneType(getAny<std::string>(pt, "laneType", ""));
        out.set_eventTimeUtc(getAny<std::string>(pt, "eventTimeUtc", ""));
        out.set_simTimeSec(getAny<double>(pt, "simTimeSec", 0.0));
        out.set_speedMps(getAny<double>(pt, "speedMps", 0.0));
        out.set_latDeg(getAny<double>(pt, "latDeg", 0.0));
        out.set_lonDeg(getAny<double>(pt, "lonDeg", 0.0));
        out.set_tumHash(getAny<std::string>(pt, "tumHash", ""));
        out.set_encryptedTumData(getAny<std::string>(pt, "encryptedTumData", ""));
        out.set_certificateId(getAny<std::string>(pt, "certificateId", ""));
        return true;
    } catch (const std::exception& e) {
        PLUGIN_LOG(logDEBUG, "UdpTollInterface") << "JSON TUM parse failed: " << e.what();
        return false;
    }
}

bool UdpTollInterface::_decodeUperTum(const char* data, int len, TumMessage& out) {
    std::vector<uint8_t> bytes;
    if (isHexString(data, len)) {
        bytes = J3217Codec::fromHex(std::string(data, data + len));
    } else {
        bytes.assign(data, data + len);
    }
    return !bytes.empty() && J3217Codec::decodeTum(bytes, out);
}

void UdpTollInterface::sendTam(TamMessage& tam, const TollPluginConfig& cfg) {
    if (!cfg.enableUdpInterface) return;

    tam.set_rseHost(cfg.udpBindHost);
    tam.set_tumPort(cfg.tumUdpPort);

    if (cfg.udpEncodingMode == "JSON" || cfg.udpEncodingMode == "DUAL") {
        std::string json = tam.to_string();
        _sendPayload(cfg.obeHost, cfg.tamUdpPort, json.data(), static_cast<int>(json.size()), "TAM JSON");
    }
    if (cfg.udpEncodingMode == "J3217_UPER" || cfg.udpEncodingMode == "DUAL") {
        auto bytes = J3217Codec::encodeTam(tam);
        if (!bytes.empty()) {
            _sendPayload(cfg.obeHost, cfg.tamUdpPort,
                         reinterpret_cast<const char*>(bytes.data()),
                         static_cast<int>(bytes.size()), "TAM UPER");
        } else {
            ++_errors;
        }
    }
    ++_tamSent;
}

void UdpTollInterface::sendTumAck(TumAckMessage& ack, const std::string& encodingMode) {
    TollPluginConfig cfg;
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        cfg = _cfg;
    }
    if (!cfg.enableUdpInterface) return;

    std::string mode = cfg.udpEncodingMode.empty() ? encodingMode : cfg.udpEncodingMode;
    if (mode == "JSON" || mode == "DUAL") {
        std::string json = ack.to_string();
        _sendPayload(cfg.obeHost, cfg.tumAckUdpPort, json.data(), static_cast<int>(json.size()), "TumAck JSON");
    }
    if (mode == "J3217_UPER" || mode == "DUAL") {
        auto bytes = J3217Codec::encodeTumAck(ack);
        if (!bytes.empty()) {
            _sendPayload(cfg.obeHost, cfg.tumAckUdpPort,
                         reinterpret_cast<const char*>(bytes.data()),
                         static_cast<int>(bytes.size()), "TumAck UPER");
        } else {
            ++_errors;
        }
    }
    ++_tumAckSent;
}

void UdpTollInterface::_sendPayload(const std::string& host, int port, const char* data, int len, const char* label) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ++_errors;
        return;
    }
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << "Invalid ObeHost for " << label << ": " << host;
        close(sock);
        return;
    }

    int n = sendto(sock, data, len, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (n != len) {
        ++_errors;
        PLUGIN_LOG(logERROR, "UdpTollInterface") << label << " UDP send failed to "
                                                 << host << ":" << port << ": "
                                                 << strerror(errno);
    }
    close(sock);
}

bool UdpTollInterface::enabled() const { return _enabled; }
int UdpTollInterface::tamSentCount() const { return _tamSent; }
int UdpTollInterface::tumReceivedCount() const { return _tumReceived; }
int UdpTollInterface::tumAckSentCount() const { return _tumAckSent; }
int UdpTollInterface::errorCount() const { return _errors; }

std::string UdpTollInterface::lastPeer() const {
    std::lock_guard<std::mutex> lock(_peerMutex);
    return _lastPeer;
}

void UdpTollInterface::_setLastPeer(const std::string& peer) {
    std::lock_guard<std::mutex> lock(_peerMutex);
    _lastPeer = peer;
}

}} // namespace v2x::toll

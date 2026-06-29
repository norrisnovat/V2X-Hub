/**
 * UdpTollInterface.h — Direct UDP interface for VISSIM/OBU simulation.
 */
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "PluginConfig.h"
#include "TamMessage.hpp"
#include "TumMessage.hpp"
#include "TumAckMessage.hpp"

namespace v2x { namespace toll {

class UdpTollInterface {
public:
    using TumHandler = std::function<void(TumMessage&)>;

    explicit UdpTollInterface(TumHandler handler);
    ~UdpTollInterface();

    void updateConfig(const TollPluginConfig& cfg);
    void stop();

    void sendTam(TamMessage& tam, const TollPluginConfig& cfg);
    void sendTumAck(TumAckMessage& ack, const std::string& encodingMode);

    bool enabled() const;
    int tamSentCount() const;
    int tumReceivedCount() const;
    int tumAckSentCount() const;
    int errorCount() const;
    std::string lastPeer() const;

private:
    TumHandler _handler;

    mutable std::mutex _configMutex;
    TollPluginConfig _cfg;

    mutable std::mutex _peerMutex;
    std::string _lastPeer;

    std::thread _listener;
    std::atomic<bool> _running{false};
    std::atomic<bool> _enabled{false};
    std::atomic<int> _tamSent{0};
    std::atomic<int> _tumReceived{0};
    std::atomic<int> _tumAckSent{0};
    std::atomic<int> _errors{0};
    int _sock = -1;

    void _restartLocked(const TollPluginConfig& cfg);
    void _listenLoop(TollPluginConfig cfg);
    bool _decodeTum(const char* data, int len, const TollPluginConfig& cfg, TumMessage& out);
    bool _decodeJsonTum(const std::string& json, TumMessage& out);
    bool _decodeUperTum(const char* data, int len, TumMessage& out);
    void _sendPayload(const std::string& host, int port, const char* data, int len, const char* label);
    void _setLastPeer(const std::string& peer);
};

}} // namespace v2x::toll

/**
 * TamManager.cpp — TAM broadcast manager implementation.
 *
 * Runs exclusively in TollPlugin::Main() thread.
 * FrequencyThrottle gates broadcasts to TamBroadcastIntervalMs.
 */
#include "TamManager.h"
#include "TamMessage.hpp"
#include "J3217Codec.h"

#include <PluginClientClockAware.h>

using namespace tmx::utils;

#include <chrono>
#include <iomanip>
#include <sstream>

namespace v2x { namespace toll {

TamManager::TamManager(tmx::utils::PluginClientClockAware* plugin)
    : _plugin(plugin)
    , _throttle(boost::chrono::milliseconds(1000))
{
}

void TamManager::tick(const TollPluginConfig& cfg) {
    // Resize throttle if interval changed.
    // FrequencyThrottle does not expose a resize method in all versions.
    // TO VERIFY: best pattern to dynamically update throttle period.
    if (_throttle.Monitor(TAM_KEY)) {
        _broadcast(cfg);
    }
}

void TamManager::broadcastNow(const TollPluginConfig& cfg) {
    _broadcast(cfg);
}

bool TamManager::isKnownTamSeq(int tamSeq) const {
    std::lock_guard<std::mutex> lock(_seqMutex);
    return _knownSeqs.count(tamSeq) > 0;
}

int TamManager::currentMsgCount() const {
    std::lock_guard<std::mutex> lock(_seqMutex);
    return _tamMsgCount;
}

void TamManager::setObserver(TamObserver observer) {
    _observer = std::move(observer);
}

void TamManager::_broadcast(const TollPluginConfig& cfg) {
    _incrementAndRegisterSeq();

    TamMessage tam;
    tam.set_msgType("TAM");
    tam.set_msgCount(currentMsgCount());
    tam.set_protocolVersion(1);
    tam.set_tollPointID(cfg.tollPointID);
    tam.set_tollChargerID(cfg.tollChargerID);
    tam.set_tollPointName(cfg.tollPointName);
    tam.set_validFromUtc(_buildValidFromUtc());
    tam.set_validUntilUtc(_buildValidUntilUtc(cfg.tamValidityWindowSec));
    tam.set_broadcastIntervalMs(cfg.tamBroadcastIntervalMs);
    tam.set_certificateId("");   // Phase 3: SCMS Authorization Ticket ID
    tam.set_signedData("");      // Phase 3: IEEE 1609.2 signed payload

    // Toll rates from config
    auto it = cfg.tollRates.find("passenger_car");
    if (it != cfg.tollRates.end()) tam.set_ratePassengerCar(it->second);
    it = cfg.tollRates.find("heavy_vehicle");
    if (it != cfg.tollRates.end()) tam.set_rateHeavyVehicle(it->second);
    it = cfg.tollRates.find("bus");
    if (it != cfg.tollRates.end()) tam.set_rateBus(it->second);

    IvpMsgFlags flags = (cfg.tamRouteMode == "DSRC") ? IvpMsgFlags_RouteDSRC : IvpMsgFlags_None;

    bool doUper = (cfg.messageEncodingMode == "J3217_UPER" || cfg.messageEncodingMode == "DUAL");
    bool doJson = (cfg.messageEncodingMode == "JSON"       || cfg.messageEncodingMode == "DUAL");

    if (doUper) {
        auto bytes = J3217Codec::encodeTam(tam);
        if (bytes.empty()) {
            PLUGIN_LOG(logERROR, "TamManager") << "TAM UPER encode failed — skipping UPER broadcast";
        } else {
            std::string hex = J3217Codec::toHex(bytes);
            tmx::routeable_message routeMsg;
            routeMsg.initialize(TamMessage::MessageType, TamMessage::MessageSubType, "", 0, flags);
            routeMsg.set_payload(hex);
            routeMsg.set_encoding("asn.1-uper/hexstring");
            _plugin->BroadcastMessage(static_cast<const tmx::routeable_message&>(routeMsg));
            PLUGIN_LOG(logINFO, "TamManager") << "TAM UPER broadcast: msgCount=" << currentMsgCount()
                                              << " bytes=" << bytes.size();

            if (cfg.messageEncodingMode == "DUAL") {
                // Verify round-trip matches original
                TamMessage check;
                bool ok = J3217Codec::decodeTam(bytes, check);
                bool match = ok &&
                    check.get_msgCount()    == tam.get_msgCount() &&
                    check.get_tollPointID() == tam.get_tollPointID();
                PLUGIN_LOG(logINFO, "TamManager") << "DUAL TAM field check: "
                    << (match ? "MATCH" : "MISMATCH");
            }
        }
    }

    if (doJson) {
        _plugin->BroadcastMessage(tam, "", 0, flags);
        PLUGIN_LOG(logDEBUG, "TamManager") << "TAM JSON broadcast: msgCount=" << currentMsgCount()
                                           << " tollPointID=" << cfg.tollPointID;
    }

    if (_observer) {
        _observer(tam, cfg);
    }
}

void TamManager::_incrementAndRegisterSeq() {
    std::lock_guard<std::mutex> lock(_seqMutex);
    _tamMsgCount = static_cast<uint8_t>((_tamMsgCount % 127) + 1);
    _knownSeqs.insert(_tamMsgCount);
    // Enforce max history size — remove oldest (lowest) entry.
    while (static_cast<int>(_knownSeqs.size()) > _maxHistory) {
        _knownSeqs.erase(_knownSeqs.begin());
    }
}

std::string TamManager::_buildValidFromUtc() const {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string TamManager::_buildValidUntilUtc(int windowSec) const {
    auto now = std::chrono::system_clock::now()
             + std::chrono::seconds(windowSec);
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

}} // namespace v2x::toll

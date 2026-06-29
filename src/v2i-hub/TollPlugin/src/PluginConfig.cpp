/**
 * PluginConfig.cpp — Configuration manager implementation.
 * All GetConfigValue() calls are centralised here.
 */
#include "PluginConfig.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace v2x { namespace toll {

namespace {
bool parseBool(const std::string& value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v == "true" || v == "1" || v == "yes" || v == "on";
}
}

void PluginConfig::set(const std::string& key, const std::string& value) {
    if (value.empty()) return;
    std::lock_guard<std::mutex> lock(_mutex);
    if      (key == "TollPointID")             _config.tollPointID             = value;
    else if (key == "TollChargerID")           _config.tollChargerID           = value;
    else if (key == "TollPointName")           _config.tollPointName           = value;
    else if (key == "RefLatDeg")               _config.refLatDeg               = std::stod(value);
    else if (key == "RefLonDeg")               _config.refLonDeg               = std::stod(value);
    else if (key == "TamBroadcastIntervalMs")  _config.tamBroadcastIntervalMs  = std::stoi(value);
    else if (key == "TamValidityWindowSec")    _config.tamValidityWindowSec    = std::stoi(value);
    else if (key == "TumDeduplicationWindowS") _config.tumDeduplicationWindowS = std::stoi(value);
    else if (key == "MaxTamHistorySize")       _config.maxTamHistorySize       = std::stoi(value);
    else if (key == "TransactionLogPath")      _config.transactionLogPath      = value;
    else if (key == "AuditLogPath")            _config.auditLogPath            = value;
    else if (key == "SecurityMode")            _config.securityMode            = value;
    else if (key == "CertificateStorePath")    _config.certificateStorePath    = value;
    else if (key == "TamRouteMode")            _config.tamRouteMode            = value;
    else if (key == "MessageEncodingMode")     _config.messageEncodingMode     = value;
    else if (key == "EnableUdpInterface")      _config.enableUdpInterface      = parseBool(value);
    else if (key == "UdpBindHost")             _config.udpBindHost             = value;
    else if (key == "TamUdpPort")              _config.tamUdpPort              = std::stoi(value);
    else if (key == "TumUdpPort")              _config.tumUdpPort              = std::stoi(value);
    else if (key == "TumAckUdpPort")           _config.tumAckUdpPort           = std::stoi(value);
    else if (key == "ObeHost")                 _config.obeHost                 = value;
    else if (key == "UdpEncodingMode")         _config.udpEncodingMode         = value;
    else if (key == "ValidLaneIDs")            _parseValidLaneIDs(value);
    else if (key == "RatePassengerCar")  { _ratePassengerCar  = std::stod(value); _buildRateTable(); }
    else if (key == "RateHeavyVehicle")  { _rateHeavyVehicle  = std::stod(value); _buildRateTable(); }
    else if (key == "RateBus")           { _rateBus           = std::stod(value); _buildRateTable(); }
}

TollPluginConfig PluginConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _config;
}

bool PluginConfig::isComplete() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _config.isComplete();
}

void PluginConfig::_parseValidLaneIDs(const std::string& csv) {
    _config.validLaneIDs.clear();
    std::istringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        if (!token.empty()) {
            try { _config.validLaneIDs.insert(std::stoi(token)); }
            catch (const std::exception&) {}
        }
    }
}

void PluginConfig::_buildRateTable() {
    _config.tollRates.clear();
    _config.tollRates["passenger_car"] = _ratePassengerCar;
    _config.tollRates["heavy_vehicle"] = _rateHeavyVehicle;
    _config.tollRates["bus"]           = _rateBus;
}

}} // namespace v2x::toll

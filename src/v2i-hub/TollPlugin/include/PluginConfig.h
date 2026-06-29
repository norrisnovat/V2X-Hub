/**
 * PluginConfig.h — Thread-safe configuration snapshot for TollPlugin.
 *
 * Wraps GetConfigValue() calls. All managers read configuration
 * exclusively through PluginConfig::snapshot() — never call
 * GetConfigValue() directly outside this class.
 */
#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>

namespace v2x { namespace toll {

struct TollPluginConfig {
    std::string tollPointID          = "TOLLPOINT-UNKNOWN";
    std::string tollChargerID        = "TOLLCHARGER-UNKNOWN";
    std::string tollPointName        = "";
    double      refLatDeg            = 0.0;
    double      refLonDeg            = 0.0;
    int         tamBroadcastIntervalMs = 1000;
    int         tamValidityWindowSec   = 10;
    std::set<int>                validLaneIDs;
    std::map<std::string,double> tollRates;
    int         tumDeduplicationWindowS = 5;
    int         maxTamHistorySize       = 128;
    std::string transactionLogPath = "/var/log/TollPlugin/v2xhub_toll_transactions.jsonl";
    std::string auditLogPath       = "/var/log/TollPlugin/audit.jsonl";
    std::string securityMode         = "PHASE2_NO_SECURITY";
    std::string certificateStorePath = "/etc/v2x/certs/";
    std::string tamRouteMode          = "INTERNAL";
    std::string messageEncodingMode   = "JSON";     // JSON | J3217_UPER | DUAL
    bool        enableUdpInterface    = false;
    std::string udpBindHost           = "0.0.0.0";
    int         tamUdpPort            = 5001;
    int         tumUdpPort            = 5002;
    int         tumAckUdpPort         = 5003;
    std::string obeHost               = "10.0.0.2";
    std::string udpEncodingMode       = "JSON";     // JSON | J3217_UPER | DUAL

    bool isComplete() const {
        return !tollPointID.empty() &&
               tollPointID != "TOLLPOINT-UNKNOWN" &&
               !tollChargerID.empty() &&
               tollChargerID != "TOLLCHARGER-UNKNOWN";
    }
};

class PluginConfig {
public:
    PluginConfig() = default;
    void set(const std::string& key, const std::string& value);
    TollPluginConfig snapshot() const;
    bool isComplete() const;

private:
    mutable std::mutex  _mutex;
    TollPluginConfig    _config;
    double _ratePassengerCar  = 5.00;
    double _rateHeavyVehicle  = 10.00;
    double _rateBus           = 7.50;
    void _parseValidLaneIDs(const std::string& csv);
    void _buildRateTable();
};

}} // namespace v2x::toll

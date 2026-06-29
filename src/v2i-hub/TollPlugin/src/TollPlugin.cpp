/**
 * TollPlugin.cpp — Main V2X-Hub plugin implementation.
 *
 * Follows V2X-Hub Programming Guide patterns:
 *   - Extends PluginClientClockAware
 *   - UpdateConfigSettings() via GetConfigValue()
 *   - OnConfigChanged() -> UpdateConfigSettings()
 *   - OnStateChange(registered) -> initial TAM broadcast
 *   - Main() -> FrequencyThrottle TAM loop
 *   - AddMessageFilter<TumMessage> -> HandleTumMessage()
 *   - BroadcastMessage() in TamManager and TumAckManager
 *   - SetStatus() in _updateStatus()
 *   - PLOG() for all logging
 */
#include "TollPlugin.h"
#include "TumMessage.hpp"
#include "J3217Codec.h"

#include <PluginClientClockAware.h>
#include <tmx/json/cJSON.h>   // raw cJSON access to avoid routeable_message parse exceptions
#include <chrono>
#include <unistd.h>   // usleep — POSIX, standard on Ubuntu/Linux

using namespace tmx::utils;

namespace v2x { namespace toll {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TollPlugin::TollPlugin(const std::string& name)
    : PluginClientClockAware(name)
    , _tamManager(this)
    , _ackManager(this)
    , _udpInterface([this](TumMessage& tum) {
        tmx::routeable_message dummy;
        if (_tumManager) _tumManager->handleTumMessage(tum, dummy);
    })
    , _statusThrottle(std::chrono::milliseconds(5000))
{
    // Subscribe to TUM messages from V2X-Hub TMX bus.
    // Type "J3217" subtype "TUM" — custom message, no J2735 typed wrapper.
    // TO VERIFY: whether templated AddMessageFilter<TumMessage> is preferred
    //            over the string form on the Ubuntu V2X-Hub build.
    AddMessageFilter<TumMessage>(this, &TollPlugin::HandleTumMessage);
    SubscribeToMessages();

    _tamManager.setObserver([this](TamMessage& tam, const TollPluginConfig& cfg) {
        _udpInterface.sendTam(tam, cfg);
    });
    _ackManager.setObserver([this](TumAckMessage& ack, const std::string& encodingMode) {
        _udpInterface.sendTumAck(ack, encodingMode);
    });

    PLOG(logINFO) << "TollPlugin constructed — waiting for config";
}

TollPlugin::~TollPlugin() {
    _udpInterface.stop();
    delete _tumManager;
    delete _store;
}

// ---------------------------------------------------------------------------
// V2X-Hub lifecycle
// ---------------------------------------------------------------------------

int TollPlugin::Main() {
    PLOG(logINFO) << "TollPlugin Main() starting";

    while (_plugin->state != IvpPluginState_error) {
        if (!_config.isComplete()) {
            usleep(50000);   // 50 ms — wait for config from V2X-Hub core
            continue;
        }

        auto cfg = _config.snapshot();

        // First-broadcast: fire once after config is fully settled (all
        // OnConfigChanged callbacks have run), so the correct encoding mode
        // is used. Replaces the old OnStateChange broadcastNow() which fired
        // before the IVP client-side config cache was populated.
        if (!_initialBroadcastDone) {
            _tamManager.broadcastNow(cfg);
            _initialBroadcastDone = true;
        }

        // TAM broadcast — gated by FrequencyThrottle in TamManager
        _tamManager.tick(cfg);

        // Status heartbeat — every 5 s
        if (_statusThrottle.Monitor(STATUS_KEY)) {
            _updateStatus();
        }

        usleep(10000);  // 10 ms main-loop cadence
    }

    PLOG(logINFO) << "TollPlugin Main() exiting";
    return 0;
}

void TollPlugin::UpdateConfigSettings() {
    // GetConfigValue() reads from V2X-Hub config database.
    // All parameters defined in manifest.json configuration[].
    std::string val;

    GetConfigValue("TollPointID",            val); _config.set("TollPointID",            val);
    GetConfigValue("TollChargerID",          val); _config.set("TollChargerID",          val);
    GetConfigValue("TollPointName",          val); _config.set("TollPointName",          val);
    GetConfigValue("RefLatDeg",              val); _config.set("RefLatDeg",              val);
    GetConfigValue("RefLonDeg",              val); _config.set("RefLonDeg",              val);
    GetConfigValue("TamBroadcastIntervalMs", val); _config.set("TamBroadcastIntervalMs", val);
    GetConfigValue("TamValidityWindowSec",   val); _config.set("TamValidityWindowSec",   val);
    GetConfigValue("TumDeduplicationWindowS",val); _config.set("TumDeduplicationWindowS",val);
    GetConfigValue("MaxTamHistorySize",      val); _config.set("MaxTamHistorySize",      val);
    GetConfigValue("ValidLaneIDs",           val); _config.set("ValidLaneIDs",           val);
    GetConfigValue("RatePassengerCar",       val); _config.set("RatePassengerCar",       val);
    GetConfigValue("RateHeavyVehicle",       val); _config.set("RateHeavyVehicle",       val);
    GetConfigValue("RateBus",               val); _config.set("RateBus",               val);
    GetConfigValue("TransactionLogPath",     val); _config.set("TransactionLogPath",     val);
    GetConfigValue("AuditLogPath",           val); _config.set("AuditLogPath",           val);
    GetConfigValue("SecurityMode",           val); _config.set("SecurityMode",           val);
    GetConfigValue("CertificateStorePath",   val); _config.set("CertificateStorePath",   val);
    GetConfigValue("TamRouteMode",           val); _config.set("TamRouteMode",           val);
    GetConfigValue("MessageEncodingMode",    val); _config.set("MessageEncodingMode",    val);
    GetConfigValue("EnableUdpInterface",     val); _config.set("EnableUdpInterface",     val);
    GetConfigValue("UdpBindHost",            val); _config.set("UdpBindHost",            val);
    GetConfigValue("TamUdpPort",             val); _config.set("TamUdpPort",             val);
    GetConfigValue("TumUdpPort",             val); _config.set("TumUdpPort",             val);
    GetConfigValue("TumAckUdpPort",          val); _config.set("TumAckUdpPort",          val);
    GetConfigValue("ObeHost",                val); _config.set("ObeHost",                val);
    GetConfigValue("UdpEncodingMode",        val); _config.set("UdpEncodingMode",        val);

    // Log effective mode values so startup config is always visible in tmxcore log.
    {
        auto snap = _config.snapshot();
        PLUGIN_LOG(logINFO, "TollPlugin") << "Config loaded:"
            << " TollPointID="       << snap.tollPointID
            << " MessageEncodingMode=" << snap.messageEncodingMode
            << " TamRouteMode="      << snap.tamRouteMode
            << " EnableUdpInterface=" << (snap.enableUdpInterface ? "true" : "false")
            << " UdpEncodingMode="   << snap.udpEncodingMode;
    }

    // Initialise managers after first complete config load
    if (_config.isComplete() && !_store) {
        auto cfg = _config.snapshot();
        _store = new TollTransactionStore(cfg.transactionLogPath, cfg.auditLogPath);
        _tumManager = new TumManager(&_tamManager, &_ackManager, _store);
    }

    if (_tumManager && _config.isComplete()) {
        auto cfg = _config.snapshot();
        _tumManager->updateConfig(cfg);
        _udpInterface.updateConfig(cfg);
    }

    PLOG(logINFO) << "TollPlugin config updated: "
                  << _config.snapshot().tollPointID;
}

void TollPlugin::OnConfigChanged(const char* key, const char* value) {
    PluginClientClockAware::OnConfigChanged(key, value);
    // Re-read all values — avoids subtle missed-update bugs.
    UpdateConfigSettings();
}

void TollPlugin::OnStateChange(IvpPluginState state) {
    PluginClientClockAware::OnStateChange(state);
    if (state == IvpPluginState_registered) {
        PLOG(logINFO) << "TollPlugin registered with V2X-Hub TMX core";
        UpdateConfigSettings();
        // Initial broadcast deferred to Main() — IVP client-side config cache
        // is not yet populated at this point; OnConfigChanged will settle it.
    }
}

// ---------------------------------------------------------------------------
// TUM message handler
// ---------------------------------------------------------------------------

void TollPlugin::OnMessageReceived(IvpMessage* msg) {
    if (!msg || !msg->type || !msg->subtype) {
        PluginClientClockAware::OnMessageReceived(msg);
        return;
    }

    auto cfg = _config.snapshot();
    bool isTum = (strcmp(msg->type,    TumMessage::MessageType)    == 0) &&
                 (strcmp(msg->subtype, TumMessage::MessageSubType) == 0);

    bool wantUper = (cfg.messageEncodingMode == "J3217_UPER" || cfg.messageEncodingMode == "DUAL");
    if (isTum && wantUper) {
        // Access payload directly via cJSON to avoid routeable_message
        // constructor throwing when parsing a hex string as JSON tree.
        const char* hex = (msg->payload && (msg->payload->type == cJSON_String))
                          ? msg->payload->valuestring : nullptr;
        if (!hex || hex[0] == '\0') {
            PLUGIN_LOG(logERROR, "TollPlugin") << "UPER TUM: missing or non-string payload";
            return;
        }
        auto bytes = J3217Codec::fromHex(hex);
        if (bytes.empty()) {
            PLUGIN_LOG(logERROR, "TollPlugin") << "UPER TUM: invalid hex payload";
            return;
        }
        TumMessage decoded;
        if (!J3217Codec::decodeTum(bytes, decoded)) {
            PLUGIN_LOG(logERROR, "TollPlugin") << "UPER TUM: UPER decode failed (" << bytes.size() << " bytes)";
            return;
        }
        PLUGIN_LOG(logINFO, "TollPlugin") << "UPER TUM decoded: " << bytes.size()
                                          << " bytes tempID=" << decoded.get_tempID();
        // Create a minimal routeable_message for the TumManager signature
        tmx::routeable_message dummy;
        if (_tumManager) _tumManager->handleTumMessage(decoded, dummy);
        return;
    }

    // JSON mode or non-TUM message — normal typed dispatch.
    PluginClientClockAware::OnMessageReceived(msg);
}

void TollPlugin::HandleTumMessage(TumMessage&             tum,
                                   tmx::routeable_message& routeableMsg) {
    if (!_tumManager) {
        PLOG(logWARNING) << "TUM received before plugin fully initialised — discarding";
        return;
    }
    _tumManager->handleTumMessage(tum, routeableMsg);
}

// ---------------------------------------------------------------------------
// Status reporting
// ---------------------------------------------------------------------------

void TollPlugin::_updateStatus() {
    if (_tamManager.currentMsgCount() > 0)
        SetStatus("TAM Sent Count",   _tamManager.currentMsgCount());

    if (_tumManager) {
        SetStatus("TUM Received Count",      _tumManager->receivedCount());
        SetStatus("TUM Accepted Count",      _tumManager->acceptedCount());
        SetStatus("TUM Rejected Count",      _tumManager->rejectedCount());
        SetStatus("Last Rejection Reason",   _tumManager->lastRejectionReason());
        SetStatus("TUM Avg Processing (us)", _tumManager->avgProcessingUs());
        SetStatus("TUM Max Processing (us)", _tumManager->maxProcessingUs());
    }

    if (_ackManager.sentCount() > 0)
        SetStatus("TumAck Sent Count", _ackManager.sentCount());

    if (_store) {
        SetStatus("Total Revenue USD",      _store->totalRevenue());
        SetStatus("Security Mode",          _config.snapshot().securityMode);
        SetStatus("TransactionWriteErrors", _store->txnWriteErrors());
        SetStatus("AuditWriteErrors",       _store->auditWriteErrors());
        if (!_store->lastFileError().empty())
            SetStatus("LastFileError",      _store->lastFileError());
    }

    SetStatus("UDP Interface Enabled", _udpInterface.enabled() ? "true" : "false");
    SetStatus("UDP TAM Sent Count",    _udpInterface.tamSentCount());
    SetStatus("UDP TUM Received Count", _udpInterface.tumReceivedCount());
    SetStatus("UDP TumAck Sent Count", _udpInterface.tumAckSentCount());
    SetStatus("UDP Last Peer",         _udpInterface.lastPeer());
    SetStatus("UDP Error Count",       _udpInterface.errorCount());
}

}} // namespace v2x::toll

// ---------------------------------------------------------------------------
// V2X-Hub plugin entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    return run_plugin<v2x::toll::TollPlugin>("TollPlugin", argc, argv);
}

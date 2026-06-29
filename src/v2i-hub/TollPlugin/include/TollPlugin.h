/**
 * TollPlugin.h — Main V2X-Hub plugin class declaration.
 *
 * Extends PluginClientClockAware (V2X-Hub Programming Guide pattern).
 * Owns all manager instances. Coordinates TAM broadcast, TUM handling,
 * config management, and status reporting.
 */
#pragma once

#include <mutex>
#include <PluginClientClockAware.h>
#include <FrequencyThrottle.h>
#include "PluginConfig.h"
#include "TamManager.h"
#include "TumManager.h"
#include "TumAckManager.h"
#include "TollTransactionStore.h"
#include "TumMessage.hpp"
#include "UdpTollInterface.h"

namespace v2x { namespace toll {

class TollPlugin : public tmx::utils::PluginClientClockAware {
public:
    explicit TollPlugin(const std::string& name);
    virtual ~TollPlugin();

    // V2X-Hub PluginClientClockAware overrides
    int  Main()                                                  override;
    void UpdateConfigSettings();
    void OnConfigChanged(const char* key, const char* value)    override;
    void OnStateChange(IvpPluginState state)                     override;
    void OnMessageReceived(IvpMessage* msg)                      override;

    /**
     * TUM message handler — registered via AddMessageFilter<TumMessage>.
     * Called by V2X-Hub on each received TUM in a dedicated receive thread.
     * TO VERIFY: exact signature expected by AddMessageFilter<> template.
     */
    void HandleTumMessage(TumMessage&             tum,
                          tmx::routeable_message& routeableMsg);

private:
    PluginConfig          _config;
    TamManager            _tamManager;
    TumAckManager         _ackManager;
    TollTransactionStore* _store   = nullptr;
    TumManager*           _tumManager = nullptr;
    UdpTollInterface      _udpInterface;

    // Status counters — flushed to SetStatus() periodically.
    void _updateStatus();

    // Frequency throttle for status updates (every 5 s).
    static constexpr int STATUS_KEY = 2;
    tmx::utils::FrequencyThrottle<int> _statusThrottle;

    // Set to true once Main() fires the initial TAM broadcast with settled config.
    bool _initialBroadcastDone = false;
};

}} // namespace v2x::toll

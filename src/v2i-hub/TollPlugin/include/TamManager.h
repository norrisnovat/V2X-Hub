/**
 * TamManager.h — TAM broadcast manager.
 *
 * Runs in TollPlugin::Main() thread.
 * Uses FrequencyThrottle to gate broadcasts to TamBroadcastIntervalMs.
 * Maintains tamMsgCount (0-127, J2735 wrapping).
 * Exposes knownTamSeqs ring buffer for TumManager tamSequenceNum validation.
 */
#pragma once

#include <functional>
#include <mutex>
#include <set>
#include "PluginConfig.h"

// Forward declaration — avoid pulling in full V2X-Hub headers in header.
// TO VERIFY: exact forward-declare pattern for PluginClientClockAware on Ubuntu.
namespace tmx { namespace utils { class PluginClientClockAware; } }
namespace v2x { namespace toll { class TamMessage; } }

// TO VERIFY: FrequencyThrottle.h exact include path in V2X-Hub TMX.
#include <FrequencyThrottle.h>

namespace v2x { namespace toll {

class TamManager {
public:
    explicit TamManager(tmx::utils::PluginClientClockAware* plugin);
    using TamObserver = std::function<void(TamMessage&, const TollPluginConfig&)>;

    /**
     * Called from TollPlugin::Main() on every loop iteration.
     * Broadcasts TAM if FrequencyThrottle permits (i.e. interval has elapsed).
     * @param cfg  Current configuration snapshot.
     */
    void tick(const TollPluginConfig& cfg);

    /**
     * Force an immediate TAM broadcast regardless of throttle.
     * Called from OnStateChange(registered) to announce RSE presence.
     */
    void broadcastNow(const TollPluginConfig& cfg);

    /**
     * Check whether a given tamSequenceNum was recently broadcast.
     * Used by TumManager validation step 7.
     * Thread-safe.
     */
    bool isKnownTamSeq(int tamSeq) const;

    /** Current TAM message count (0-127). */
    int currentMsgCount() const;

    void setObserver(TamObserver observer);

private:
    tmx::utils::PluginClientClockAware* _plugin;

    // FrequencyThrottle key: arbitrary int (single TAM stream).
    static constexpr int TAM_KEY = 1;
    tmx::utils::FrequencyThrottle<int> _throttle;

    // tamMsgCount protected by _seqMutex (read by TumManager threads).
    mutable std::mutex _seqMutex;
    uint8_t            _tamMsgCount  = 127;   // pre-increment: first broadcast → 1
    int                _maxHistory   = 128;

    // Circular set of recent tamMsgCounts.
    std::set<int>      _knownSeqs;
    TamObserver        _observer;

    void _broadcast(const TollPluginConfig& cfg);
    void _incrementAndRegisterSeq();
    std::string _buildValidFromUtc() const;
    std::string _buildValidUntilUtc(int windowSec) const;
};

}} // namespace v2x::toll

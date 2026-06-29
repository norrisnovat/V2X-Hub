/**
 * TumManager.h — TUM receive, 8-step validation, and dispatch.
 *
 * HandleTumMessage() is called by V2X-Hub on each received TUM.
 * Each call runs in its own receive thread (V2X-Hub threading model).
 * Passes TUM to ValidationEngine, then dispatches to TumAckManager
 * and TollTransactionStore.
 *
 * Validation pipeline (8 steps, per architecture doc):
 *   1. msgType == "TUM"
 *   2. tollPointID matches config
 *   3. tollChargerID matches config
 *   4. laneID is in ValidLaneIDs
 *   5. vehicleType / vehicleClass has configured rate
 *   6. tamSequenceNum is known (TamManager registry)
 *   7. Duplicate TUM (tempID + tumSequenceNum, dedup window)
 *   8. certificateId placeholder present (Phase 3: actual SCMS verify)
 */
#pragma once

#include <mutex>
#include <map>
#include <string>
#include <chrono>
#include <vector>
#include <tmx/messages/routeable_message.hpp>
#include "PluginConfig.h"
#include "TumMessage.hpp"

namespace v2x { namespace toll {

struct ValidationResult {
    bool        valid         = false;
    std::string rejectionCode = "";      // e.g. "UNKNOWN_TOLL_POINT"
    std::string rejectionDesc = "";      // human-readable
};

class TamManager;
class TumAckManager;
class TollTransactionStore;

class TumManager {
public:
    TumManager(TamManager*           tamManager,
               TumAckManager*        ackManager,
               TollTransactionStore* store);

    /**
     * Handler registered with AddMessageFilter<TumMessage>().
     * Called by V2X-Hub on each received TUM message.
     * Each call runs in a separate receive thread.
     *
     * TO VERIFY: exact handler signature matches V2X-Hub 2024/2025 API.
     */
    void handleTumMessage(TumMessage&               tum,
                          tmx::routeable_message&   routeableMsg);

    /** Total TUMs received. */
    int receivedCount()     const;
    int acceptedCount()     const;
    int rejectedCount()     const;
    std::string lastRejectionReason() const;
    long avgProcessingUs()  const;   // average TUM processing time (µs)
    long maxProcessingUs()  const;   // worst-case TUM processing time (µs)

private:
    TamManager*           _tamManager;
    TumAckManager*        _ackManager;
    TollTransactionStore* _store;

    // Counters and latency tracking (protected by _counterMutex)
    mutable std::mutex _counterMutex;
    int    _received         = 0;
    int    _accepted         = 0;
    int    _rejected         = 0;
    std::string _lastRejection;
    long   _totalProcessingUs = 0;
    long   _maxProcessingUs   = 0;

    // Deduplication cache: (tempID, tumSequenceNum) → wall-clock insertion time.
    // Uses steady_clock so VISSIM pause/rewind/speed does not affect expiry.
    // Protected by _dedupMutex.
    mutable std::mutex _dedupMutex;
    std::map<std::pair<std::string,int>,
             std::chrono::steady_clock::time_point> _dedupCache;

    // Current config snapshot — updated on each call from plugin config.
    // Protected by _configMutex.
    mutable std::mutex _configMutex;
    TollPluginConfig   _cfg;

public:
    /** TollPlugin calls this to push fresh config to TumManager. */
    void updateConfig(const TollPluginConfig& cfg);

private:
    ValidationResult _validate(TumMessage& tum) const;
    bool _isDuplicate(const std::string& tempID, int tumSeq);
};

}} // namespace v2x::toll

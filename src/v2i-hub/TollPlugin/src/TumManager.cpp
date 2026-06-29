/**
 * TumManager.cpp — TUM receive, 8-step validation, dispatch.
 *
 * HandleTumMessage() runs in a V2X-Hub receive thread per TUM.
 * All shared state access protected by appropriate mutexes.
 */
#include "TumManager.h"
#include "TamManager.h"
#include "TumAckManager.h"
#include "TollTransactionStore.h"

#include <PluginLog.h>

using namespace tmx::utils;

namespace v2x { namespace toll {

TumManager::TumManager(TamManager*           tamManager,
                       TumAckManager*        ackManager,
                       TollTransactionStore* store)
    : _tamManager(tamManager)
    , _ackManager(ackManager)
    , _store(store)
{
}

void TumManager::updateConfig(const TollPluginConfig& cfg) {
    std::lock_guard<std::mutex> lock(_configMutex);
    _cfg = cfg;
}

void TumManager::handleTumMessage(TumMessage&             tum,
                                  tmx::routeable_message& /*routeableMsg*/) {
    auto _handleStart = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(_counterMutex);
        ++_received;
    }

    TollPluginConfig cfg;
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        cfg = _cfg;
    }

    auto result = _validate(tum);

    if (!result.valid) {
        PLUGIN_LOG(logINFO, "TumManager") << "TUM REJECTED | tempID=" << tum.get_tempID()
                                          << " tum#" << tum.get_tumSequenceNum()
                                          << " reason=" << result.rejectionCode;
        {
            std::lock_guard<std::mutex> lock(_counterMutex);
            ++_rejected;
            _lastRejection = result.rejectionCode + ": " + result.rejectionDesc;
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - _handleStart).count();
            _totalProcessingUs += elapsed;
            if (elapsed > _maxProcessingUs) _maxProcessingUs = elapsed;
        }
        _ackManager->sendRejected(tum, result.rejectionCode, result.rejectionDesc, cfg.messageEncodingMode);
        _store->recordRejection(tum, result);
        return;
    }

    // Transaction amount from config rate table
    double amount = 0.0;
    {
        auto it = cfg.tollRates.find(tum.get_vehicleClass());
        if (it != cfg.tollRates.end()) amount = it->second;
    }

    TransactionRecord txn = _store->record(tum, amount);

    PLUGIN_LOG(logINFO, "TumManager") << "TUM ACCEPTED | tempID=" << tum.get_tempID()
                                      << " txn=" << txn.transactionID
                                      << " lane=" << tum.get_laneID()
                                      << " $" << amount;

    {
        std::lock_guard<std::mutex> lock(_counterMutex);
        ++_accepted;
    }

    _ackManager->sendAccepted(tum, txn, cfg.messageEncodingMode);

    // Processing latency tracking
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - _handleStart).count();
    {
        std::lock_guard<std::mutex> lock(_counterMutex);
        _totalProcessingUs += elapsed;
        if (elapsed > _maxProcessingUs) _maxProcessingUs = elapsed;
    }
}

// -----------------------------------------------------------------------
// Validation pipeline — 8 steps
// -----------------------------------------------------------------------

ValidationResult TumManager::_validate(TumMessage& tum) const {
    TollPluginConfig cfg;
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        cfg = _cfg;
    }

    // Step 1: msgType
    if (tum.get_msgType() != "TUM")
        return {false, "WRONG_MSG_TYPE",
                "Expected msgType=TUM, got " + tum.get_msgType()};

    // Step 2: tollPointID
    if (tum.get_tollPointID() != cfg.tollPointID)
        return {false, "UNKNOWN_TOLL_POINT",
                "Unknown tollPointID: " + tum.get_tollPointID()};

    // Step 3: tollChargerID (optional field; validate if present)
    if (!tum.get_tollChargerID().empty() &&
        tum.get_tollChargerID() != cfg.tollChargerID)
        return {false, "UNKNOWN_TOLL_CHARGER",
                "Unknown tollChargerID: " + tum.get_tollChargerID()};

    // Step 4: laneID
    if (cfg.validLaneIDs.count(tum.get_laneID()) == 0)
        return {false, "INVALID_LANE",
                "laneID " + std::to_string(tum.get_laneID()) + " not in ValidLaneIDs"};

    // Step 5: vehicleClass / vehicleType has rate
    if (cfg.tollRates.count(tum.get_vehicleClass()) == 0)
        return {false, "UNKNOWN_VEHICLE_TYPE",
                "No rate for vehicleClass: " + tum.get_vehicleClass()};

    // Step 6: tamSequenceNum is known (registered by TamManager)
    if (!_tamManager->isKnownTamSeq(tum.get_tamSequenceNum()))
        return {false, "UNKNOWN_TAM_SEQ",
                "tamSequenceNum " + std::to_string(tum.get_tamSequenceNum()) + " not known"};

    // Step 7: duplicate TUM (wall-clock window — VISSIM time not used)
    if (const_cast<TumManager*>(this)->_isDuplicate(
            tum.get_tempID(),
            tum.get_tumSequenceNum()))
        return {false, "DUPLICATE_TUM",
                "Duplicate: tempID=" + tum.get_tempID() +
                " tum#" + std::to_string(tum.get_tumSequenceNum())};

    // Step 8: certificateId placeholder (Phase 3: real SCMS verify)
    // In Phase 2: field is optional, no rejection. Log if absent.
    if (tum.get_certificateId().empty()) {
        PLUGIN_LOG(logDEBUG, "TumManager") << "TUM certificateId empty — Phase 2 mode, no security check";
    }

    return {true, "", ""};
}

bool TumManager::_isDuplicate(const std::string& tempID, int tumSeq) {
    int windowSec;
    {
        std::lock_guard<std::mutex> cfgLock(_configMutex);
        windowSec = _cfg.tumDeduplicationWindowS;
    }

    auto now = std::chrono::steady_clock::now();
    auto window = std::chrono::seconds(windowSec);
    auto key = std::make_pair(tempID, tumSeq);

    std::lock_guard<std::mutex> lock(_dedupMutex);

    // Evict entries older than 2× window — keeps cache bounded.
    auto evictBefore = now - 2 * window;
    for (auto it = _dedupCache.begin(); it != _dedupCache.end(); ) {
        if (it->second < evictBefore)
            it = _dedupCache.erase(it);
        else
            ++it;
    }

    auto it = _dedupCache.find(key);
    if (it != _dedupCache.end() && (now - it->second) < window)
        return true;

    _dedupCache[key] = now;
    return false;
}

int  TumManager::receivedCount()      const { std::lock_guard<std::mutex> l(_counterMutex); return _received; }
int  TumManager::acceptedCount()      const { std::lock_guard<std::mutex> l(_counterMutex); return _accepted; }
int  TumManager::rejectedCount()      const { std::lock_guard<std::mutex> l(_counterMutex); return _rejected; }
std::string TumManager::lastRejectionReason() const {
    std::lock_guard<std::mutex> l(_counterMutex);
    return _lastRejection;
}
long TumManager::avgProcessingUs() const {
    std::lock_guard<std::mutex> l(_counterMutex);
    int total = _received;
    return (total > 0) ? (_totalProcessingUs / total) : 0;
}
long TumManager::maxProcessingUs() const {
    std::lock_guard<std::mutex> l(_counterMutex);
    return _maxProcessingUs;
}

}} // namespace v2x::toll

/**
 * TumAckManager.h — TumAck construction and broadcast.
 *
 * Called by TumManager after validation result is known.
 * Builds TumAckMessage and calls BroadcastMessage() via plugin pointer.
 * Maintains msgCount (0-127, J2735 wrapping).
 */
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include "TumMessage.hpp"
#include "TumAckMessage.hpp"

namespace tmx { namespace utils { class PluginClientClockAware; } }

namespace v2x { namespace toll {

struct TransactionRecord;

class TumAckManager {
public:
    explicit TumAckManager(tmx::utils::PluginClientClockAware* plugin);
    using AckObserver = std::function<void(TumAckMessage&, const std::string&)>;

    /**
     * Build and broadcast a TumAck for an accepted TUM.
     * @param tum    The original TUM being acknowledged.
     * @param txn    The transaction record created by TollTransactionStore.
     */
    void sendAccepted(TumMessage&              tum,
                      const TransactionRecord& txn,
                      const std::string&       encodingMode = "JSON");

    /**
     * Build and broadcast a TumAck for a rejected TUM.
     * @param tum            The rejected TUM.
     * @param rejectionCode  Short code (e.g. "INVALID_LANE").
     * @param rejectionDesc  Human-readable reason.
     * @param encodingMode   "JSON" or "J3217_UPER".
     */
    void sendRejected(TumMessage&        tum,
                      const std::string& rejectionCode,
                      const std::string& rejectionDesc,
                      const std::string& encodingMode = "JSON");

    int sentCount() const;
    void setObserver(AckObserver observer);

private:
    tmx::utils::PluginClientClockAware* _plugin;

    mutable std::mutex _mutex;
    uint8_t            _msgCount = 0;
    int                _sent     = 0;
    AckObserver        _observer;

    uint8_t _nextMsgCount();
    void    _broadcast(TumAckMessage& ack, const std::string& encodingMode);
    static std::string _nowUtc();
};

}} // namespace v2x::toll

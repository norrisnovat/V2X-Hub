/**
 * TumAckManager.cpp — TumAck construction and broadcast.
 */
#include "TumAckManager.h"
#include "TollTransactionStore.h"
#include "J3217Codec.h"
#include "TumAckMessage.hpp"

#include <PluginClientClockAware.h>

using namespace tmx::utils;

#include <chrono>
#include <iomanip>
#include <sstream>

namespace v2x { namespace toll {

TumAckManager::TumAckManager(tmx::utils::PluginClientClockAware* plugin)
    : _plugin(plugin)
{
}

void TumAckManager::sendAccepted(TumMessage&              tum,
                                  const TransactionRecord& txn,
                                  const std::string&       encodingMode) {
    TumAckMessage ack;
    ack.set_msgType("TumAck");
    ack.set_msgCount(_nextMsgCount());
    ack.set_ackMaxAge(300);
    ack.set_tempID(tum.get_tempID());
    ack.set_tumSequenceNum(tum.get_tumSequenceNum());
    ack.set_tollPointID(tum.get_tollPointID());
    ack.set_transactionID(txn.transactionID);
    ack.set_status("accepted");
    ack.set_rejectionReason("");
    ack.set_amountUsd(txn.amountUsd);
    ack.set_currency("USD");
    ack.set_ackTimeUtc(_nowUtc());
    ack.set_signedTumHash("");    // Phase 3: RSE signs the TUM hash
    ack.set_certificateId("");   // Phase 3: RSE signing cert

    _broadcast(ack, encodingMode);

    PLUGIN_LOG(logINFO, "TumAckManager") << "TumAck ACCEPTED | tempID=" << tum.get_tempID()
                                         << " txn=" << txn.transactionID
                                         << " $" << txn.amountUsd;
}

void TumAckManager::sendRejected(TumMessage&        tum,
                                   const std::string& rejectionCode,
                                   const std::string& rejectionDesc,
                                   const std::string& encodingMode) {
    TumAckMessage ack;
    ack.set_msgType("TumAck");
    ack.set_msgCount(_nextMsgCount());
    ack.set_ackMaxAge(300);
    ack.set_tempID(tum.get_tempID());
    ack.set_tumSequenceNum(tum.get_tumSequenceNum());
    ack.set_tollPointID(tum.get_tollPointID());
    ack.set_transactionID("");
    ack.set_status("rejected:" + rejectionCode);
    ack.set_rejectionReason(rejectionDesc);
    ack.set_amountUsd(0.0);
    ack.set_currency("USD");
    ack.set_ackTimeUtc(_nowUtc());
    ack.set_signedTumHash("");
    ack.set_certificateId("");

    _broadcast(ack, encodingMode);

    PLUGIN_LOG(logINFO, "TumAckManager") << "TumAck REJECTED | tempID=" << tum.get_tempID()
                                          << " code=" << rejectionCode;
}

int TumAckManager::sentCount() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _sent;
}

void TumAckManager::setObserver(AckObserver observer) {
    _observer = std::move(observer);
}

uint8_t TumAckManager::_nextMsgCount() {
    std::lock_guard<std::mutex> lock(_mutex);
    _msgCount = static_cast<uint8_t>((_msgCount % 127) + 1);
    ++_sent;
    return _msgCount;
}

void TumAckManager::_broadcast(TumAckMessage& ack, const std::string& encodingMode) {
    bool doUper = (encodingMode == "J3217_UPER" || encodingMode == "DUAL");
    bool doJson = (encodingMode == "JSON"       || encodingMode == "DUAL");

    if (doUper) {
        auto bytes = J3217Codec::encodeTumAck(ack);
        if (bytes.empty()) {
            PLUGIN_LOG(logERROR, "TumAckManager") << "UPER TumAck encode failed";
        } else {
            std::string hex = J3217Codec::toHex(bytes);
            tmx::routeable_message routeMsg;
            routeMsg.initialize(TumAckMessage::MessageType, TumAckMessage::MessageSubType, "", 0, IvpMsgFlags_None);
            routeMsg.set_payload(hex);
            routeMsg.set_encoding("asn.1-uper/hexstring");
            _plugin->BroadcastMessage(static_cast<const tmx::routeable_message&>(routeMsg));
            PLUGIN_LOG(logINFO, "TumAckManager") << "TumAck UPER broadcast: " << bytes.size() << " bytes";

            if (encodingMode == "DUAL") {
                TumAckMessage check;
                bool ok = J3217Codec::decodeTumAck(bytes, check);
                bool match = ok &&
                    check.get_tempID()  == ack.get_tempID() &&
                    check.get_status()  == ack.get_status();
                PLUGIN_LOG(logINFO, "TumAckManager") << "DUAL TumAck field check: "
                    << (match ? "MATCH" : "MISMATCH");
            }
        }
    }

    if (doJson) {
        _plugin->BroadcastMessage(ack, "", 0, IvpMsgFlags_None);
    }

    if (_observer) {
        _observer(ack, encodingMode);
    }
}

std::string TumAckManager::_nowUtc() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

}} // namespace v2x::toll

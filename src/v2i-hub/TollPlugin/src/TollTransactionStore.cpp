/**
 * TollTransactionStore.cpp — JSONL transaction writer.
 *
 * Writes to:
 *   outputs/transactions/v2xhub_toll_transactions.jsonl  (accepted only)
 *   <auditLogPath>                                        (all: accepted + rejected)
 */
#include "TollTransactionStore.h"
#include "TumManager.h"   // ValidationResult

#include <PluginLog.h>

using namespace tmx::utils;

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Minimal JSON serialisation — no external dependency.
// TO VERIFY: whether V2X-Hub Ubuntu build provides nlohmann/json or similar.
// If available, replace string concatenation with json library.
static std::string jsonStr(const std::string& key, const std::string& val) {
    return "\"" + key + "\":\"" + val + "\"";
}
static std::string jsonNum(const std::string& key, double val) {
    return "\"" + key + "\":" + std::to_string(val);
}
static std::string jsonInt(const std::string& key, int val) {
    return "\"" + key + "\":" + std::to_string(val);
}

namespace v2x { namespace toll {

TollTransactionStore::TollTransactionStore(const std::string& transactionLogPath,
                                           const std::string& auditLogPath) {
    _txnFile.open(transactionLogPath, std::ios::app);
    if (!_txnFile.is_open())
        PLUGIN_LOG(logERROR, "TollTransactionStore") << "Cannot open transaction log: " << transactionLogPath;

    _auditFile.open(auditLogPath, std::ios::app);
    if (!_auditFile.is_open())
        PLUGIN_LOG(logERROR, "TollTransactionStore") << "Cannot open audit log: " << auditLogPath;
}

TollTransactionStore::~TollTransactionStore() {
    if (_txnFile.is_open())  _txnFile.close();
    if (_auditFile.is_open()) _auditFile.close();
}

TransactionRecord TollTransactionStore::record(TumMessage& tum, double amountUsd) {
    TransactionRecord txn;
    txn.transactionID  = _generateTransactionID(tum);
    txn.tempID         = tum.get_tempID();
    txn.vehicleID      = tum.get_vehicleType();  // Phase 2: Vissim type as proxy
    txn.vehicleType    = tum.get_vehicleType();
    txn.vehicleClass   = tum.get_vehicleClass();
    txn.tollPointID    = tum.get_tollPointID();
    txn.tollChargerID  = tum.get_tollChargerID();
    txn.laneID         = tum.get_laneID();
    txn.tumSequenceNum = tum.get_tumSequenceNum();
    txn.tamSequenceNum = tum.get_tamSequenceNum();
    txn.eventTimeUtc   = tum.get_eventTimeUtc();
    txn.simTimeSec     = tum.get_simTimeSec();
    txn.amountUsd      = amountUsd;
    txn.processedUtc   = _nowUtc();
    txn.status         = "accepted";

    // Build JSONL record
    std::string json = "{" +
        jsonStr("transaction_id",  txn.transactionID)  + "," +
        jsonStr("temp_id",         txn.tempID)          + "," +
        jsonInt("vehicle_type",    txn.vehicleType)     + "," +
        jsonStr("vehicle_class",   txn.vehicleClass)    + "," +
        jsonStr("toll_point_id",   txn.tollPointID)     + "," +
        jsonStr("toll_charger_id", txn.tollChargerID)   + "," +
        jsonInt("lane_id",         txn.laneID)          + "," +
        jsonInt("tum_sequence_num",txn.tumSequenceNum)  + "," +
        jsonInt("tam_sequence_num",txn.tamSequenceNum)  + "," +
        jsonStr("event_time_utc",  txn.eventTimeUtc)    + "," +
        jsonNum("sim_time_s",      txn.simTimeSec)      + "," +
        jsonNum("amount_usd",      txn.amountUsd)       + "," +
        jsonStr("processed_utc",   txn.processedUtc)    + "," +
        jsonStr("status",          txn.status)          +
        "}";

    _writeJson(_txnFile,   _txnMutex,   json, _txnWriteErrors,   "transaction");
    _writeJson(_auditFile, _auditMutex, json, _auditWriteErrors, "audit");     // accepted also in audit

    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        ++_accepted;
        _totalRevenue += amountUsd;
    }

    PLUGIN_LOG(logINFO, "TollTransactionStore") << "TRANSACTION written | txn=" << txn.transactionID
                                                << " $" << amountUsd;
    return txn;
}

void TollTransactionStore::recordRejection(TumMessage&             tum,
                                            const ValidationResult& result) {
    std::string json = "{" +
        jsonStr("temp_id",         tum.get_tempID())           + "," +
        jsonStr("toll_point_id",   tum.get_tollPointID())      + "," +
        jsonInt("tum_sequence_num",tum.get_tumSequenceNum())   + "," +
        jsonStr("rejection_code",  result.rejectionCode)       + "," +
        jsonStr("rejection_desc",  result.rejectionDesc)       + "," +
        jsonStr("processed_utc",   _nowUtc())                  + "," +
        jsonStr("status",          "rejected")                 +
        "}";

    _writeJson(_auditFile, _auditMutex, json, _auditWriteErrors, "audit");

    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        ++_rejected;
    }
}

int    TollTransactionStore::acceptedCount()    const { std::lock_guard<std::mutex> l(_statsMutex); return _accepted; }
int    TollTransactionStore::rejectedCount()    const { std::lock_guard<std::mutex> l(_statsMutex); return _rejected; }
double TollTransactionStore::totalRevenue()     const { std::lock_guard<std::mutex> l(_statsMutex); return _totalRevenue; }
int    TollTransactionStore::txnWriteErrors()   const { std::lock_guard<std::mutex> l(_statsMutex); return _txnWriteErrors; }
int    TollTransactionStore::auditWriteErrors() const { std::lock_guard<std::mutex> l(_statsMutex); return _auditWriteErrors; }
std::string TollTransactionStore::lastFileError() const { std::lock_guard<std::mutex> l(_statsMutex); return _lastFileError; }

std::string TollTransactionStore::_generateTransactionID(TumMessage& tum) {
    // Deterministic ID: RSE-<date>-<hash6>
    // Same algorithm as Python rse/tum_receiver.py for consistency.
    std::string raw = tum.get_tollPointID() + "-" +
                      tum.get_tempID()      + "-" +
                      std::to_string(tum.get_tumSequenceNum()) + "-" +
                      std::to_string(tum.get_simTimeSec());

    // Simple 6-char uppercase hex from a cheap hash of raw string.
    // TO VERIFY / upgrade: use SHA-256 on Ubuntu with OpenSSL.
    uint32_t h = 5381;
    for (char c : raw) h = ((h << 5) + h) ^ static_cast<uint8_t>(c);

    std::ostringstream ss;
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char date[12];
    std::strftime(date, sizeof(date), "%Y-%m-%d", std::gmtime(&t));
    ss << "RSE-" << date << "-" << std::uppercase << std::hex
       << std::setw(6) << std::setfill('0') << (h & 0xFFFFFF);
    return ss.str();
}

std::string TollTransactionStore::_nowUtc() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

void TollTransactionStore::_writeJson(std::ofstream& file,
                                       std::mutex&    fileMutex,
                                       const std::string& json,
                                       int&           errorCounter,
                                       const char*    logLabel) {
    std::lock_guard<std::mutex> fileLock(fileMutex);

    if (!file.is_open()) {
        // File never opened — log once per call so operators see repeated alerts.
        PLUGIN_LOG(logERROR, "TollTransactionStore")
            << logLabel << " log not open — record LOST: " << json.substr(0, 80);
        std::lock_guard<std::mutex> statsLock(_statsMutex);
        ++errorCounter;
        _lastFileError = std::string(logLabel) + " log not open";
        return;
    }

    file << json << "\n";
    file.flush();   // flush per write — IVP socket latency dominates, not I/O

    if (!file.good()) {
        PLUGIN_LOG(logERROR, "TollTransactionStore")
            << logLabel << " write failed (disk full or I/O error) — record may be LOST";
        std::lock_guard<std::mutex> statsLock(_statsMutex);
        ++errorCounter;
        _lastFileError = std::string(logLabel) + " write failed";
    }
}

}} // namespace v2x::toll

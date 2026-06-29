/**
 * TollTransactionStore.h — JSONL transaction writer.
 *
 * Writes one JSON record per accepted TUM to:
 *   outputs/transactions/v2xhub_toll_transactions.jsonl
 *
 * Also writes an audit record (accepted AND rejected) to auditLogPath.
 * Thread-safe: separate mutex per output file.
 */
#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include "TumMessage.hpp"

namespace v2x { namespace toll {

struct ValidationResult;

/** Complete record of one accepted toll transaction. */
struct TransactionRecord {
    std::string transactionID;
    std::string tempID;
    int         vehicleID        = 0;
    int         vehicleType      = 0;
    std::string vehicleClass;
    std::string tollPointID;
    std::string tollChargerID;
    int         laneID           = 0;
    int         tumSequenceNum   = 0;
    int         tamSequenceNum   = 0;
    std::string eventTimeUtc;
    double      simTimeSec       = 0.0;
    double      amountUsd        = 0.0;
    std::string processedUtc;
    std::string status           = "accepted";
};

class TollTransactionStore {
public:
    explicit TollTransactionStore(const std::string& transactionLogPath,
                                  const std::string& auditLogPath);
    ~TollTransactionStore();

    /**
     * Create a TransactionRecord from an accepted TUM.
     * Writes to transactions JSONL. Returns the populated record.
     */
    TransactionRecord record(TumMessage& tum, double amountUsd);

    /**
     * Write an audit entry for a rejected TUM.
     * Rejected TUMs do not produce a TransactionRecord.
     */
    void recordRejection(TumMessage&             tum,
                         const ValidationResult& result);

    int    acceptedCount()    const;
    int    rejectedCount()    const;
    double totalRevenue()     const;
    int    txnWriteErrors()   const;
    int    auditWriteErrors() const;
    std::string lastFileError() const;

private:
    std::mutex  _txnMutex;
    std::ofstream _txnFile;

    std::mutex  _auditMutex;
    std::ofstream _auditFile;

    mutable std::mutex _statsMutex;
    int    _accepted        = 0;
    int    _rejected        = 0;
    double _totalRevenue    = 0.0;
    int    _txnWriteErrors  = 0;
    int    _auditWriteErrors = 0;
    std::string _lastFileError;

    static std::string _generateTransactionID(TumMessage& tum);
    static std::string _nowUtc();
    void _writeJson(std::ofstream& file, std::mutex& fileMutex,
                    const std::string& json,
                    int& errorCounter, const char* logLabel);
};

}} // namespace v2x::toll

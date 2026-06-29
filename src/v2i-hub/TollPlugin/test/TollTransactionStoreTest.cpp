/**
 * TollTransactionStoreTest.cpp
 *
 * Tests accepted and rejected transaction record construction,
 * counter/revenue accumulation, and audit separation.
 * Uses /dev/null for file output — tests verify returned values and counters,
 * not the on-disk JSONL format.
 */
#include <gtest/gtest.h>
#include "../include/TollTransactionStore.h"
#include "../include/TumMessage.hpp"
#include "../include/TumManager.h"   // ValidationResult full definition

namespace v2x { namespace toll { namespace test {

static TumMessage makeTxnTum() {
    TumMessage tum;
    tum.set_msgType("TUM");
    tum.set_tumSequenceNum(7);
    tum.set_tamSequenceNum(3);
    tum.set_tempID("CCDD5678");
    tum.set_vehicleClass("passenger_car");
    tum.set_vehicleType(610);
    tum.set_tollPointID("camden-nj-001");
    tum.set_tollChargerID("camden-rse-01");
    tum.set_laneID(4);
    tum.set_eventTimeUtc("2026-06-01T12:00:00Z");
    tum.set_simTimeSec(100.0);
    return tum;
}

class TollTransactionStoreTest : public ::testing::Test {
protected:
    // /dev/null: always open, discards writes — lets us test record() return value
    TollTransactionStore store{"/dev/null", "/dev/null"};
};

// ---------------------------------------------------------------------------
// Accepted record field mapping
// ---------------------------------------------------------------------------

TEST_F(TollTransactionStoreTest, AcceptedRecordFieldsMatchTum) {
    auto tum = makeTxnTum();
    TransactionRecord txn = store.record(tum, 5.00);

    EXPECT_EQ(txn.tempID,         "CCDD5678");
    EXPECT_EQ(txn.vehicleClass,   "passenger_car");
    EXPECT_EQ(txn.vehicleType,    610);
    EXPECT_EQ(txn.tollPointID,    "camden-nj-001");
    EXPECT_EQ(txn.tollChargerID,  "camden-rse-01");
    EXPECT_EQ(txn.laneID,         4);
    EXPECT_EQ(txn.tumSequenceNum, 7);
    EXPECT_EQ(txn.tamSequenceNum, 3);
    EXPECT_EQ(txn.eventTimeUtc,   "2026-06-01T12:00:00Z");
    EXPECT_DOUBLE_EQ(txn.simTimeSec, 100.0);
    EXPECT_DOUBLE_EQ(txn.amountUsd,  5.00);
    EXPECT_EQ(txn.status,         "accepted");
}

TEST_F(TollTransactionStoreTest, TransactionIDNonEmptyAndPrefixed) {
    auto tum = makeTxnTum();
    TransactionRecord txn = store.record(tum, 5.00);
    EXPECT_FALSE(txn.transactionID.empty());
    EXPECT_EQ(txn.transactionID.substr(0, 4), "RSE-");
}

TEST_F(TollTransactionStoreTest, ProcessedUtcNonEmpty) {
    auto tum = makeTxnTum();
    TransactionRecord txn = store.record(tum, 5.00);
    EXPECT_FALSE(txn.processedUtc.empty());
}

// ---------------------------------------------------------------------------
// Counter and revenue accumulation
// ---------------------------------------------------------------------------

TEST_F(TollTransactionStoreTest, AcceptedCountStartsAtZero) {
    EXPECT_EQ(store.acceptedCount(), 0);
    EXPECT_EQ(store.rejectedCount(), 0);
    EXPECT_DOUBLE_EQ(store.totalRevenue(), 0.0);
}

TEST_F(TollTransactionStoreTest, AcceptedCountIncrementsPerRecord) {
    auto tum = makeTxnTum();
    store.record(tum, 5.00);
    EXPECT_EQ(store.acceptedCount(), 1);
    store.record(tum, 10.00);
    EXPECT_EQ(store.acceptedCount(), 2);
}

TEST_F(TollTransactionStoreTest, TotalRevenueAccumulates) {
    auto tum = makeTxnTum();
    store.record(tum, 5.00);
    store.record(tum, 10.00);
    EXPECT_DOUBLE_EQ(store.totalRevenue(), 15.00);
}

TEST_F(TollTransactionStoreTest, AcceptedDoesNotIncrementRejectedCount) {
    auto tum = makeTxnTum();
    store.record(tum, 5.00);
    EXPECT_EQ(store.rejectedCount(), 0);
}

// ---------------------------------------------------------------------------
// Rejected audit record
// ---------------------------------------------------------------------------

TEST_F(TollTransactionStoreTest, RejectedCountIncrementsOnRejection) {
    EXPECT_EQ(store.rejectedCount(), 0);
    auto tum = makeTxnTum();
    ValidationResult r{false, "INVALID_LANE", "laneID 99 not in ValidLaneIDs"};
    store.recordRejection(tum, r);
    EXPECT_EQ(store.rejectedCount(), 1);
}

TEST_F(TollTransactionStoreTest, RejectedDoesNotAffectAcceptedCountOrRevenue) {
    auto tum = makeTxnTum();
    ValidationResult r{false, "WRONG_MSG_TYPE", "Expected msgType=TUM"};
    store.recordRejection(tum, r);
    EXPECT_EQ(store.acceptedCount(), 0);
    EXPECT_DOUBLE_EQ(store.totalRevenue(), 0.0);
}

TEST_F(TollTransactionStoreTest, MultipleRejectionsAccumulate) {
    auto tum = makeTxnTum();
    store.recordRejection(tum, {false, "INVALID_LANE",         "lane 99"});
    store.recordRejection(tum, {false, "UNKNOWN_VEHICLE_TYPE", "class=unknown"});
    store.recordRejection(tum, {false, "DUPLICATE_TUM",        "dup"});
    EXPECT_EQ(store.rejectedCount(), 3);
    EXPECT_EQ(store.acceptedCount(), 0);
}

// ---------------------------------------------------------------------------
// Mixed accepted + rejected
// ---------------------------------------------------------------------------

TEST_F(TollTransactionStoreTest, AcceptedAndRejectedCountedIndependently) {
    auto tum = makeTxnTum();
    store.record(tum, 5.00);
    store.record(tum, 7.50);
    store.recordRejection(tum, {false, "INVALID_LANE", "lane 99"});
    EXPECT_EQ(store.acceptedCount(), 2);
    EXPECT_EQ(store.rejectedCount(), 1);
    EXPECT_DOUBLE_EQ(store.totalRevenue(), 12.50);
}

// ---------------------------------------------------------------------------
// Error handling — invalid log paths and write error counters
// ---------------------------------------------------------------------------

TEST(TollTransactionStoreErrorTest, NormalWriteHasZeroErrors) {
    TollTransactionStore store{"/dev/null", "/dev/null"};
    EXPECT_EQ(store.txnWriteErrors(),   0);
    EXPECT_EQ(store.auditWriteErrors(), 0);
    EXPECT_TRUE(store.lastFileError().empty());
}

TEST(TollTransactionStoreErrorTest, InvalidTxnPathIncrementsWriteError) {
    // Path to a directory that does not exist → open fails → write error on record().
    TollTransactionStore store{"/nonexistent/dir/txn.jsonl", "/dev/null"};

    TumMessage tum;
    tum.set_msgType("TUM");
    tum.set_tumSequenceNum(1);
    tum.set_tamSequenceNum(1);
    tum.set_tempID("AAAA0001");
    tum.set_vehicleClass("passenger_car");
    tum.set_vehicleType(610);
    tum.set_tollPointID("test-pt");
    tum.set_tollChargerID("test-rse");
    tum.set_laneID(1);
    tum.set_eventTimeUtc("2026-01-01T00:00:00Z");
    tum.set_simTimeSec(0.0);

    store.record(tum, 5.00);

    EXPECT_GT(store.txnWriteErrors(),   0) << "Transaction write error not counted";
    EXPECT_EQ(store.auditWriteErrors(), 0) << "Audit should succeed (/dev/null)";
    EXPECT_FALSE(store.lastFileError().empty()) << "LastFileError must be set";
}

TEST(TollTransactionStoreErrorTest, InvalidAuditPathIncrementsAuditError) {
    TollTransactionStore store{"/dev/null", "/nonexistent/dir/audit.jsonl"};

    TumMessage tum;
    tum.set_msgType("TUM");
    tum.set_tumSequenceNum(2);
    tum.set_tamSequenceNum(1);
    tum.set_tempID("BBBB0002");
    tum.set_vehicleClass("passenger_car");
    tum.set_vehicleType(610);
    tum.set_tollPointID("test-pt");
    tum.set_tollChargerID("test-rse");
    tum.set_laneID(1);
    tum.set_eventTimeUtc("2026-01-01T00:00:00Z");
    tum.set_simTimeSec(0.0);

    // Both record() and recordRejection() write to audit
    store.record(tum, 5.00);
    EXPECT_GT(store.auditWriteErrors(), 0) << "Audit write error not counted on record()";

    int prevErrors = store.auditWriteErrors();
    ValidationResult r{false, "INVALID_LANE", "lane 99"};
    store.recordRejection(tum, r);
    EXPECT_GT(store.auditWriteErrors(), prevErrors) << "Audit write error not counted on recordRejection()";
}

TEST(TollTransactionStoreErrorTest, WriteErrorDoesNotPreventTumAck) {
    // TumAck is sent regardless of write failure — accepted count still increments.
    TollTransactionStore store{"/nonexistent/dir/txn.jsonl", "/nonexistent/dir/audit.jsonl"};

    TumMessage tum;
    tum.set_msgType("TUM");
    tum.set_tumSequenceNum(3);
    tum.set_tamSequenceNum(1);
    tum.set_tempID("CCCC0003");
    tum.set_vehicleClass("passenger_car");
    tum.set_vehicleType(610);
    tum.set_tollPointID("test-pt");
    tum.set_tollChargerID("test-rse");
    tum.set_laneID(1);
    tum.set_eventTimeUtc("2026-01-01T00:00:00Z");
    tum.set_simTimeSec(0.0);

    store.record(tum, 5.00);

    // record() returns a TransactionRecord and increments accepted count even on I/O failure.
    EXPECT_EQ(store.acceptedCount(), 1) << "acceptedCount must increment regardless of I/O failure";
    EXPECT_GT(store.txnWriteErrors(), 0);
}

TEST(TollTransactionStoreErrorTest, ErrorCountAccumulatesAcrossMultipleCalls) {
    TollTransactionStore store{"/nonexistent/dir/txn.jsonl", "/dev/null"};

    TumMessage tum;
    tum.set_msgType("TUM");
    tum.set_tumSequenceNum(1);
    tum.set_tamSequenceNum(1);
    tum.set_tempID("DDDD0004");
    tum.set_vehicleClass("passenger_car");
    tum.set_vehicleType(610);
    tum.set_tollPointID("test-pt");
    tum.set_tollChargerID("test-rse");
    tum.set_laneID(1);
    tum.set_eventTimeUtc("2026-01-01T00:00:00Z");
    tum.set_simTimeSec(0.0);

    store.record(tum, 5.00);
    store.record(tum, 5.00);
    store.record(tum, 5.00);

    EXPECT_EQ(store.txnWriteErrors(), 3) << "Each failed write must increment the counter";
}

}}} // namespace v2x::toll::test

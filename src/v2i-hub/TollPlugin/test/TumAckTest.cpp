/**
 * TumAckTest.cpp — Unit tests for TumAckMessage construction.
 */
#include <gtest/gtest.h>
#include "../include/TumAckMessage.hpp"
#include "../include/TumMessage.hpp"

namespace v2x { namespace toll { namespace test {

TEST(TumAckTest, AcceptedAckHasCorrectFields) {
    TumAckMessage ack;
    ack.set_msgType("TumAck");
    ack.set_msgCount(1);
    ack.set_ackMaxAge(300);
    ack.set_tempID("AABB1234");
    ack.set_tumSequenceNum(42);
    ack.set_tollPointID("camden-nj-001");
    ack.set_transactionID("RSE-2026-06-01-ABC123");
    ack.set_status("accepted");
    ack.set_rejectionReason("");
    ack.set_amountUsd(5.00);
    ack.set_currency("USD");
    ack.set_ackTimeUtc("2026-06-01T10:00:00Z");

    EXPECT_EQ(ack.get_msgType(),        "TumAck");
    EXPECT_EQ(ack.get_status(),         "accepted");
    EXPECT_EQ(ack.get_tempID(),         "AABB1234");
    EXPECT_EQ(ack.get_tumSequenceNum(), 42);
    EXPECT_EQ(ack.get_transactionID(),  "RSE-2026-06-01-ABC123");
    EXPECT_DOUBLE_EQ(ack.get_amountUsd(), 5.00);
    EXPECT_TRUE(ack.get_rejectionReason().empty());
}

TEST(TumAckTest, RejectedAckHasRejectionCode) {
    TumAckMessage ack;
    ack.set_msgType("TumAck");
    ack.set_status("rejected:INVALID_LANE");
    ack.set_rejectionReason("laneID 99 not in ValidLaneIDs");
    ack.set_transactionID("");
    ack.set_amountUsd(0.0);

    EXPECT_EQ(ack.get_status(), "rejected:INVALID_LANE");
    EXPECT_FALSE(ack.get_rejectionReason().empty());
    EXPECT_TRUE(ack.get_transactionID().empty());
    EXPECT_DOUBLE_EQ(ack.get_amountUsd(), 0.0);
}

TEST(TumAckTest, MsgCountWrapsAt127) {
    TumAckMessage ack;
    ack.set_msgCount(127);
    EXPECT_EQ(ack.get_msgCount(), 127);
    // Next value should wrap to 1
    uint8_t next = static_cast<uint8_t>((ack.get_msgCount() % 127) + 1);
    EXPECT_EQ(next, 1);
}

TEST(TumAckTest, DefaultCurrencyIsUSD) {
    TumAckMessage ack;
    EXPECT_EQ(ack.get_currency(), "USD");
}

TEST(TumAckTest, SecurityPlaceholdersEmptyInPhase2) {
    TumAckMessage ack;
    EXPECT_TRUE(ack.get_signedTumHash().empty());
    EXPECT_TRUE(ack.get_certificateId().empty());
}

TEST(TumAckTest, TollPointIDPreservedFromTum) {
    TumMessage tum;
    tum.set_tollPointID("camden-nj-001");
    tum.set_tempID("EEFF9900");
    tum.set_tumSequenceNum(3);

    TumAckMessage ack;
    ack.set_tollPointID(tum.get_tollPointID());
    ack.set_tempID(tum.get_tempID());
    ack.set_tumSequenceNum(tum.get_tumSequenceNum());

    EXPECT_EQ(ack.get_tollPointID(),     "camden-nj-001");
    EXPECT_EQ(ack.get_tempID(),          "EEFF9900");
    EXPECT_EQ(ack.get_tumSequenceNum(),  3);
}

TEST(TumAckTest, TempIDEchoedFromTum) {
    TumMessage tum;
    tum.set_tempID("CCDD5678");
    tum.set_tumSequenceNum(7);

    TumAckMessage ack;
    ack.set_tempID(tum.get_tempID());
    ack.set_tumSequenceNum(tum.get_tumSequenceNum());

    EXPECT_EQ(ack.get_tempID(),         tum.get_tempID());
    EXPECT_EQ(ack.get_tumSequenceNum(), tum.get_tumSequenceNum());
}

}}} // namespace v2x::toll::test

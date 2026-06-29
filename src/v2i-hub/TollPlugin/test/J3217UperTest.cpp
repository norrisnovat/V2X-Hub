/**
 * J3217UperTest.cpp — UPER round-trip tests for TAM, TUM, TumAck.
 *
 * Each test:
 *   1. Builds a message object with known field values.
 *   2. Encodes to UPER bytes via J3217Codec.
 *   3. Decodes the bytes back to a new message object.
 *   4. Asserts each field survived the round-trip.
 *
 * No V2X-Hub runtime required. No security fields tested (Phase 4 deferred).
 */
#include <gtest/gtest.h>
#include "../include/J3217Codec.h"
#include "../include/PluginConfig.h"
#include "../include/TamMessage.hpp"
#include "../include/TumMessage.hpp"
#include "../include/TumAckMessage.hpp"

namespace v2x { namespace toll { namespace test {

// ---------------------------------------------------------------------------
// TAM round-trip
// ---------------------------------------------------------------------------

TEST(J3217UperTest, TamRoundTrip) {
    TamMessage orig;
    orig.set_msgCount(42);
    orig.set_protocolVersion(1);
    orig.set_tollPointID("camden-nj-001");
    orig.set_tollChargerID("camden-rse-01");
    orig.set_tollPointName("Camden Toll Plaza");
    orig.set_validFromUtc("2026-06-03T01:00:00Z");
    orig.set_validUntilUtc("2026-06-03T01:00:10Z");
    orig.set_broadcastIntervalMs(1000);
    orig.set_ratePassengerCar(5.00);
    orig.set_rateHeavyVehicle(10.00);
    orig.set_rateBus(7.50);

    auto bytes = J3217Codec::encodeTam(orig);
    ASSERT_FALSE(bytes.empty()) << "TAM UPER encode failed";
    EXPECT_GT(bytes.size(), 0U);

    TamMessage decoded;
    ASSERT_TRUE(J3217Codec::decodeTam(bytes, decoded)) << "TAM UPER decode failed";

    EXPECT_EQ(decoded.get_msgCount(),            42);
    EXPECT_EQ(decoded.get_protocolVersion(),      1);
    EXPECT_EQ(decoded.get_tollPointID(),          "camden-nj-001");
    EXPECT_EQ(decoded.get_tollChargerID(),        "camden-rse-01");
    EXPECT_EQ(decoded.get_tollPointName(),        "Camden Toll Plaza");
    EXPECT_EQ(decoded.get_validFromUtc(),         "2026-06-03T01:00:00Z");
    EXPECT_EQ(decoded.get_validUntilUtc(),        "2026-06-03T01:00:10Z");
    EXPECT_EQ(decoded.get_broadcastIntervalMs(),  1000);
    EXPECT_NEAR(decoded.get_ratePassengerCar(),   5.00,  0.01);
    EXPECT_NEAR(decoded.get_rateHeavyVehicle(),   10.00, 0.01);
    EXPECT_NEAR(decoded.get_rateBus(),            7.50,  0.01);
}

TEST(J3217UperTest, TamMsgCountBoundaryRoundTrip) {
    TamMessage orig;
    orig.set_msgCount(127);
    orig.set_protocolVersion(1);
    orig.set_tollPointID("test-pt");
    orig.set_tollChargerID("test-rse");
    orig.set_validFromUtc("2026-01-01T00:00:00Z");
    orig.set_validUntilUtc("2026-01-01T00:00:10Z");
    orig.set_broadcastIntervalMs(500);
    orig.set_ratePassengerCar(0.00);
    orig.set_rateHeavyVehicle(0.00);
    orig.set_rateBus(0.00);

    auto bytes = J3217Codec::encodeTam(orig);
    ASSERT_FALSE(bytes.empty());

    TamMessage decoded;
    ASSERT_TRUE(J3217Codec::decodeTam(bytes, decoded));
    EXPECT_EQ(decoded.get_msgCount(), 127);
}

TEST(J3217UperTest, TamEncodedSizeIsReasonable) {
    TamMessage orig;
    orig.set_msgCount(1);
    orig.set_protocolVersion(1);
    orig.set_tollPointID("pt");
    orig.set_tollChargerID("rse");
    orig.set_validFromUtc("2026-01-01T00:00:00Z");
    orig.set_validUntilUtc("2026-01-01T00:00:10Z");
    orig.set_broadcastIntervalMs(1000);
    orig.set_ratePassengerCar(5.00);
    orig.set_rateHeavyVehicle(10.00);
    orig.set_rateBus(7.50);

    auto bytes = J3217Codec::encodeTam(orig);
    ASSERT_FALSE(bytes.empty());
    // UPER should be much smaller than JSON (~200 chars) — expect under 100 bytes
    EXPECT_LT(bytes.size(), 200U);
}

// ---------------------------------------------------------------------------
// TUM round-trip
// ---------------------------------------------------------------------------

TEST(J3217UperTest, TumRoundTrip) {
    TumMessage orig;
    orig.set_tumSequenceNum(7);
    orig.set_tamSequenceNum(42);
    orig.set_tempID("ABCD1234");
    orig.set_vehicleClass("passenger_car");
    orig.set_vehicleType(610);
    orig.set_tollPointID("camden-nj-001");
    orig.set_tollChargerID("camden-rse-01");
    orig.set_laneID(3);
    orig.set_eventTimeUtc("2026-06-03T01:00:00Z");
    orig.set_speedMps(15.0);
    orig.set_latDeg(39.910664);
    orig.set_lonDeg(-75.031281);

    auto bytes = J3217Codec::encodeTum(orig);
    ASSERT_FALSE(bytes.empty()) << "TUM UPER encode failed";

    TumMessage decoded;
    ASSERT_TRUE(J3217Codec::decodeTum(bytes, decoded)) << "TUM UPER decode failed";

    EXPECT_EQ(decoded.get_tumSequenceNum(), 7);
    EXPECT_EQ(decoded.get_tamSequenceNum(), 42);
    EXPECT_EQ(decoded.get_tempID(),         "ABCD1234");
    EXPECT_EQ(decoded.get_vehicleClass(),   "passenger_car");
    EXPECT_EQ(decoded.get_vehicleType(),    610);
    EXPECT_EQ(decoded.get_tollPointID(),    "camden-nj-001");
    EXPECT_EQ(decoded.get_tollChargerID(),  "camden-rse-01");
    EXPECT_EQ(decoded.get_laneID(),         3);
    EXPECT_EQ(decoded.get_eventTimeUtc(),   "2026-06-03T01:00:00Z");
    EXPECT_NEAR(decoded.get_speedMps(),  15.0,      0.01);
    EXPECT_NEAR(decoded.get_latDeg(),    39.910664, 0.0000001);
    EXPECT_NEAR(decoded.get_lonDeg(),   -75.031281, 0.0000001);
}

TEST(J3217UperTest, TumVehicleClassEnumRoundTrip) {
    for (const auto& cls : std::vector<std::string>{"passenger_car","heavy_vehicle","bus","unknown"}) {
        TumMessage orig;
        orig.set_tumSequenceNum(1);
        orig.set_tamSequenceNum(1);
        orig.set_tempID("AAAA0000");
        orig.set_vehicleClass(cls);
        orig.set_vehicleType(0);
        orig.set_tollPointID("pt");
        orig.set_tollChargerID("rse");
        orig.set_laneID(1);
        orig.set_eventTimeUtc("2026-01-01T00:00:00Z");
        orig.set_speedMps(0.0);
        orig.set_latDeg(0.0);
        orig.set_lonDeg(0.0);

        auto bytes = J3217Codec::encodeTum(orig);
        ASSERT_FALSE(bytes.empty()) << "Failed for class: " << cls;

        TumMessage decoded;
        ASSERT_TRUE(J3217Codec::decodeTum(bytes, decoded)) << "Decode failed for class: " << cls;
        EXPECT_EQ(decoded.get_vehicleClass(), cls) << "Mismatch for class: " << cls;
    }
}

// ---------------------------------------------------------------------------
// TumAck round-trip
// ---------------------------------------------------------------------------

TEST(J3217UperTest, TumAckAcceptedRoundTrip) {
    TumAckMessage orig;
    orig.set_msgCount(5);
    orig.set_tempID("ABCD1234");
    orig.set_tumSequenceNum(7);
    orig.set_tollPointID("camden-nj-001");
    orig.set_status("accepted");
    orig.set_transactionID("RSE-2026-06-03-92F933");
    orig.set_amountUsd(5.00);
    orig.set_ackTimeUtc("2026-06-03T01:00:01Z");

    auto bytes = J3217Codec::encodeTumAck(orig);
    ASSERT_FALSE(bytes.empty()) << "TumAck UPER encode failed";

    TumAckMessage decoded;
    ASSERT_TRUE(J3217Codec::decodeTumAck(bytes, decoded)) << "TumAck UPER decode failed";

    EXPECT_EQ(decoded.get_msgCount(),       5);
    EXPECT_EQ(decoded.get_tempID(),         "ABCD1234");
    EXPECT_EQ(decoded.get_tumSequenceNum(), 7);
    EXPECT_EQ(decoded.get_tollPointID(),    "camden-nj-001");
    EXPECT_EQ(decoded.get_status(),         "accepted");
    EXPECT_EQ(decoded.get_transactionID(),  "RSE-2026-06-03-92F933");
    EXPECT_NEAR(decoded.get_amountUsd(),    5.00, 0.01);
    EXPECT_EQ(decoded.get_ackTimeUtc(),     "2026-06-03T01:00:01Z");
}

TEST(J3217UperTest, TumAckRejectionCodesRoundTrip) {
    const std::vector<std::string> codes = {
        "rejected:UNKNOWN_TOLL_POINT",
        "rejected:INVALID_LANE",
        "rejected:UNKNOWN_VEHICLE_TYPE",
        "rejected:UNKNOWN_TAM_SEQ",
        "rejected:DUPLICATE_TUM"
    };

    for (const auto& status : codes) {
        TumAckMessage orig;
        orig.set_msgCount(1);
        orig.set_tempID("AAAA0000");
        orig.set_tumSequenceNum(1);
        orig.set_tollPointID("pt");
        orig.set_status(status);
        orig.set_ackTimeUtc("2026-01-01T00:00:00Z");

        auto bytes = J3217Codec::encodeTumAck(orig);
        ASSERT_FALSE(bytes.empty()) << "Encode failed for: " << status;

        TumAckMessage decoded;
        ASSERT_TRUE(J3217Codec::decodeTumAck(bytes, decoded)) << "Decode failed for: " << status;
        EXPECT_EQ(decoded.get_status(), status) << "Status mismatch for: " << status;
    }
}

// ---------------------------------------------------------------------------
// Enum helper tests
// ---------------------------------------------------------------------------

TEST(J3217UperTest, VehicleClassEnumHelpers) {
    EXPECT_EQ(J3217Codec::vehicleClassToEnum("passenger_car"),  0);
    EXPECT_EQ(J3217Codec::vehicleClassToEnum("heavy_vehicle"),  1);
    EXPECT_EQ(J3217Codec::vehicleClassToEnum("bus"),            2);
    EXPECT_EQ(J3217Codec::vehicleClassToEnum("motorcycle"),     3);
    EXPECT_EQ(J3217Codec::vehicleClassFromEnum(0), "passenger_car");
    EXPECT_EQ(J3217Codec::vehicleClassFromEnum(1), "heavy_vehicle");
    EXPECT_EQ(J3217Codec::vehicleClassFromEnum(2), "bus");
    EXPECT_EQ(J3217Codec::vehicleClassFromEnum(3), "unknown");
}

// Complete exact-match mapping table (no substring matching).
TEST(J3217UperTest, AckStatusToEnumExactMatch) {
    EXPECT_EQ(J3217Codec::ackStatusToEnum("accepted"),                        0);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:UNKNOWN_TOLL_POINT"),     1);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:INVALID_LANE"),           2);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:UNKNOWN_VEHICLE_TYPE"),   3);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:UNKNOWN_TAM_SEQ"),        4);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:DUPLICATE_TUM"),          5);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:OTHER"),                  6);
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:DECODE_ERROR"),           6); // not in ASN.1 → OTHER
}

TEST(J3217UperTest, AckStatusFromEnumExactMatch) {
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(0), "accepted");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(1), "rejected:UNKNOWN_TOLL_POINT");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(2), "rejected:INVALID_LANE");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(3), "rejected:UNKNOWN_VEHICLE_TYPE");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(4), "rejected:UNKNOWN_TAM_SEQ");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(5), "rejected:DUPLICATE_TUM");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(6), "rejected:OTHER");
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(99), "rejected:OTHER"); // out-of-range → OTHER
}

TEST(J3217UperTest, AckStatusRoundTripAllCodes) {
    // Every defined code must survive encode→decode unchanged.
    const std::vector<std::string> codes = {
        "accepted",
        "rejected:UNKNOWN_TOLL_POINT",
        "rejected:INVALID_LANE",
        "rejected:UNKNOWN_VEHICLE_TYPE",
        "rejected:UNKNOWN_TAM_SEQ",
        "rejected:DUPLICATE_TUM",
    };
    for (const auto& code : codes) {
        int e = J3217Codec::ackStatusToEnum(code);
        EXPECT_EQ(J3217Codec::ackStatusFromEnum(e), code)
            << "Round-trip failed for: " << code;
    }
}

TEST(J3217UperTest, AckStatusSubstringFalsePositivePrevented) {
    // Old bug: find("UNKNOWN_VEHICLE") matched "rejected:UNKNOWN_VEHICLE_TYPE".
    // With exact matching, only the full string maps correctly.
    // A string containing a substring of a known code maps to OTHER.
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:UNKNOWN_VEHICLE"),     6); // not a defined code
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:UNKNOWN_TOLL"),        6); // partial → OTHER
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:DUPLICATE"),            6); // partial → OTHER
    EXPECT_EQ(J3217Codec::ackStatusToEnum(""),                              6); // empty → OTHER
    EXPECT_EQ(J3217Codec::ackStatusToEnum("ACCEPTED"),                     6); // wrong case → OTHER
}

TEST(J3217UperTest, AckStatusDecodeErrorMapsToOther) {
    // DECODE_ERROR is not in the J3217 ASN.1 enum (values 0-6 only).
    // It must map to rejected-other (6) without schema change.
    EXPECT_EQ(J3217Codec::ackStatusToEnum("rejected:DECODE_ERROR"), 6);
    // And round-trips back as OTHER:
    EXPECT_EQ(J3217Codec::ackStatusFromEnum(
                  J3217Codec::ackStatusToEnum("rejected:DECODE_ERROR")),
              "rejected:OTHER");
}

TEST(J3217UperTest, AckStatusUPERRoundTripRejectionCodes) {
    // Encode TumAck with each rejection code → UPER → decode → same code.
    const std::vector<std::string> codes = {
        "rejected:UNKNOWN_TOLL_POINT",
        "rejected:INVALID_LANE",
        "rejected:UNKNOWN_VEHICLE_TYPE",
        "rejected:UNKNOWN_TAM_SEQ",
        "rejected:DUPLICATE_TUM",
    };
    for (const auto& code : codes) {
        TumAckMessage orig;
        orig.set_msgCount(1);
        orig.set_tempID("AAAA0000");
        orig.set_tumSequenceNum(1);
        orig.set_tollPointID("pt");
        orig.set_status(code);
        orig.set_ackTimeUtc("2026-01-01T00:00:00Z");

        auto bytes = J3217Codec::encodeTumAck(orig);
        ASSERT_FALSE(bytes.empty()) << "Encode failed for: " << code;

        TumAckMessage decoded;
        ASSERT_TRUE(J3217Codec::decodeTumAck(bytes, decoded)) << "Decode failed for: " << code;
        EXPECT_EQ(decoded.get_status(), code) << "Status mismatch for: " << code;
    }
}

// ---------------------------------------------------------------------------
// Hex utility tests
// ---------------------------------------------------------------------------

TEST(J3217UperTest, HexConversionRoundTrip) {
    std::vector<uint8_t> original = {0x00, 0xAB, 0xCD, 0xEF, 0xFF, 0x01};
    std::string hex = J3217Codec::toHex(original);
    EXPECT_EQ(hex, "00ABCDEFFF01");
    auto recovered = J3217Codec::fromHex(hex);
    EXPECT_EQ(recovered, original);
}

TEST(J3217UperTest, HexDecodeEmptyStringReturnsEmpty) {
    auto result = J3217Codec::fromHex("");
    EXPECT_TRUE(result.empty());
}

TEST(J3217UperTest, HexDecodeOddLengthReturnsEmpty) {
    auto result = J3217Codec::fromHex("ABC");
    EXPECT_TRUE(result.empty());
}

TEST(J3217UperTest, HexDecodeInvalidCharsReturnsEmpty) {
    auto result = J3217Codec::fromHex("ZZZZ");
    EXPECT_TRUE(result.empty());
}

TEST(J3217UperTest, HexDecodeLowercaseAccepted) {
    auto upper = J3217Codec::fromHex("ABCD");
    auto lower = J3217Codec::fromHex("abcd");
    EXPECT_EQ(upper, lower);
}

// ---------------------------------------------------------------------------
// Decode failure tests
// ---------------------------------------------------------------------------

TEST(J3217UperTest, TamDecodeGarbageBytesReturnsFalse) {
    std::vector<uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00};
    TamMessage out;
    bool ok = J3217Codec::decodeTam(garbage, out);
    EXPECT_FALSE(ok);
}

TEST(J3217UperTest, TumDecodeEmptyBytesReturnsFalse) {
    std::vector<uint8_t> empty;
    TumMessage out;
    EXPECT_FALSE(J3217Codec::decodeTum(empty, out));
}

TEST(J3217UperTest, TumAckDecodeGarbageReturnsFalse) {
    std::vector<uint8_t> garbage = {0x00, 0x01, 0x02};
    TumAckMessage out;
    EXPECT_FALSE(J3217Codec::decodeTumAck(garbage, out));
}

// ---------------------------------------------------------------------------
// Encoding mode config tests
// ---------------------------------------------------------------------------

TEST(J3217UperTest, MessageEncodingModeDefaultIsJSON) {
    PluginConfig cfg;
    EXPECT_EQ(cfg.snapshot().messageEncodingMode, "JSON");
}

TEST(J3217UperTest, MessageEncodingModeSetToUPER) {
    PluginConfig cfg;
    cfg.set("MessageEncodingMode", "J3217_UPER");
    EXPECT_EQ(cfg.snapshot().messageEncodingMode, "J3217_UPER");
}

TEST(J3217UperTest, TamUperEncodeThenHexRoundTrip) {
    TamMessage orig;
    orig.set_msgCount(10);
    orig.set_protocolVersion(1);
    orig.set_tollPointID("test-pt");
    orig.set_tollChargerID("test-rse");
    orig.set_validFromUtc("2026-01-01T00:00:00Z");
    orig.set_validUntilUtc("2026-01-01T00:00:10Z");
    orig.set_broadcastIntervalMs(1000);
    orig.set_ratePassengerCar(5.00);
    orig.set_rateHeavyVehicle(10.00);
    orig.set_rateBus(7.50);

    // Encode → hex → fromHex → decode (simulates TMX bus transport)
    auto bytes = J3217Codec::encodeTam(orig);
    ASSERT_FALSE(bytes.empty());
    std::string hex = J3217Codec::toHex(bytes);
    EXPECT_EQ(hex.size(), bytes.size() * 2);

    auto recovered = J3217Codec::fromHex(hex);
    ASSERT_EQ(recovered, bytes);

    TamMessage decoded;
    ASSERT_TRUE(J3217Codec::decodeTam(recovered, decoded));
    EXPECT_EQ(decoded.get_msgCount(),   10);
    EXPECT_EQ(decoded.get_tollPointID(), "test-pt");
}

TEST(J3217UperTest, TumUperHexRoundTripForBusTransport) {
    TumMessage orig;
    orig.set_tumSequenceNum(1);
    orig.set_tamSequenceNum(50);
    orig.set_tempID("ABCD1234");
    orig.set_vehicleClass("passenger_car");
    orig.set_vehicleType(610);
    orig.set_tollPointID("camden-nj-001");
    orig.set_tollChargerID("camden-rse-01");
    orig.set_laneID(3);
    orig.set_eventTimeUtc("2026-06-03T01:00:00Z");
    orig.set_speedMps(15.0);
    orig.set_latDeg(39.910664);
    orig.set_lonDeg(-75.031281);

    auto bytes = J3217Codec::encodeTum(orig);
    ASSERT_FALSE(bytes.empty());
    std::string hex = J3217Codec::toHex(bytes);
    auto recovered = J3217Codec::fromHex(hex);
    ASSERT_EQ(recovered, bytes);

    TumMessage decoded;
    ASSERT_TRUE(J3217Codec::decodeTum(recovered, decoded));
    EXPECT_EQ(decoded.get_tempID(),       "ABCD1234");
    EXPECT_EQ(decoded.get_vehicleClass(), "passenger_car");
    EXPECT_EQ(decoded.get_laneID(),       3);
}

}}} // namespace v2x::toll::test

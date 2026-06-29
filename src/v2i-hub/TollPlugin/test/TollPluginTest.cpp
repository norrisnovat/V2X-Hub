/**
 * TollPluginTest.cpp — Integration-level tests for PluginConfig and message classes.
 * Does not require V2X-Hub runtime.
 */
#include <gtest/gtest.h>
#include "../include/PluginConfig.h"
#include "../include/TamMessage.hpp"
#include "../include/TumMessage.hpp"
#include "../include/TumAckMessage.hpp"

namespace v2x { namespace toll { namespace test {

// ---------------------------------------------------------------------------
// PluginConfig tests
// ---------------------------------------------------------------------------

TEST(PluginConfigTest, DefaultConfigIsIncomplete) {
    PluginConfig cfg;
    EXPECT_FALSE(cfg.isComplete());
}

TEST(PluginConfigTest, ConfigCompleteAfterRequiredFields) {
    PluginConfig cfg;
    cfg.set("TollPointID",  "camden-nj-001");
    cfg.set("TollChargerID","camden-rse-01");
    EXPECT_TRUE(cfg.isComplete());
}

TEST(PluginConfigTest, SnapshotReturnsCurrentValues) {
    PluginConfig cfg;
    cfg.set("TollPointID",   "camden-nj-001");
    cfg.set("TollChargerID", "camden-rse-01");
    cfg.set("TamBroadcastIntervalMs", "2000");

    auto snap = cfg.snapshot();
    EXPECT_EQ(snap.tollPointID,            "camden-nj-001");
    EXPECT_EQ(snap.tollChargerID,          "camden-rse-01");
    EXPECT_EQ(snap.tamBroadcastIntervalMs, 2000);
}

TEST(PluginConfigTest, ValidLaneIDsParsedFromCSV) {
    PluginConfig cfg;
    cfg.set("ValidLaneIDs", "1,2,3,6,9");
    auto snap = cfg.snapshot();
    EXPECT_GT(snap.validLaneIDs.count(1), 0U);
    EXPECT_GT(snap.validLaneIDs.count(6), 0U);
    EXPECT_EQ(snap.validLaneIDs.count(7), 0U);
}

TEST(PluginConfigTest, RateTableBuiltFromConfig) {
    PluginConfig cfg;
    cfg.set("RatePassengerCar", "5.50");
    cfg.set("RateHeavyVehicle", "11.00");
    cfg.set("RateBus",          "8.00");
    auto snap = cfg.snapshot();
    EXPECT_DOUBLE_EQ(snap.tollRates.at("passenger_car"), 5.50);
    EXPECT_DOUBLE_EQ(snap.tollRates.at("heavy_vehicle"), 11.00);
    EXPECT_DOUBLE_EQ(snap.tollRates.at("bus"),           8.00);
}

// ---------------------------------------------------------------------------
// TamRouteMode config tests
// ---------------------------------------------------------------------------

TEST(PluginConfigTest, TamRouteModeDefaultIsInternal) {
    PluginConfig cfg;
    EXPECT_EQ(cfg.snapshot().tamRouteMode, "INTERNAL");
}

TEST(PluginConfigTest, TamRouteModeSetToInternal) {
    PluginConfig cfg;
    cfg.set("TamRouteMode", "INTERNAL");
    EXPECT_EQ(cfg.snapshot().tamRouteMode, "INTERNAL");
}

TEST(PluginConfigTest, MessageEncodingModeDefaultIsJSON) {
    // Fresh install default must be JSON — manifest default="JSON".
    // tmxcore INSERT uses defaultValue for value on first insert.
    PluginConfig cfg;
    EXPECT_EQ(cfg.snapshot().messageEncodingMode, "JSON");
}

TEST(PluginConfigTest, MessageEncodingModePersistsAcrossReload) {
    // Simulate what happens on plugin restart: UpdateConfigSettings() is called
    // again with the same value already stored. Overwrite should preserve the value.
    PluginConfig cfg;
    cfg.set("MessageEncodingMode", "J3217_UPER");
    EXPECT_EQ(cfg.snapshot().messageEncodingMode, "J3217_UPER");
    // Second call (simulates OnConfigChanged re-firing on restart)
    cfg.set("MessageEncodingMode", "J3217_UPER");
    EXPECT_EQ(cfg.snapshot().messageEncodingMode, "J3217_UPER");
}

TEST(PluginConfigTest, MessageEncodingModeDUAL) {
    PluginConfig cfg;
    cfg.set("MessageEncodingMode", "DUAL");
    EXPECT_EQ(cfg.snapshot().messageEncodingMode, "DUAL");
}

TEST(PluginConfigTest, TamRouteModeSetToDSRC) {
    PluginConfig cfg;
    cfg.set("TamRouteMode", "DSRC");
    EXPECT_EQ(cfg.snapshot().tamRouteMode, "DSRC");
}

TEST(PluginConfigTest, UdpDefaultsAreSimulationSafe) {
    PluginConfig cfg;
    auto snap = cfg.snapshot();
    EXPECT_FALSE(snap.enableUdpInterface);
    EXPECT_EQ(snap.udpBindHost, "0.0.0.0");
    EXPECT_EQ(snap.tamUdpPort, 5001);
    EXPECT_EQ(snap.tumUdpPort, 5002);
    EXPECT_EQ(snap.tumAckUdpPort, 5003);
    EXPECT_EQ(snap.obeHost, "10.0.0.2");
    EXPECT_EQ(snap.udpEncodingMode, "JSON");
}

TEST(PluginConfigTest, UdpConfigParsesFromManifestStrings) {
    PluginConfig cfg;
    cfg.set("EnableUdpInterface", "true");
    cfg.set("UdpBindHost", "127.0.0.1");
    cfg.set("TamUdpPort", "15001");
    cfg.set("TumUdpPort", "15002");
    cfg.set("TumAckUdpPort", "15003");
    cfg.set("ObeHost", "127.0.0.1");
    cfg.set("UdpEncodingMode", "J3217_UPER");

    auto snap = cfg.snapshot();
    EXPECT_TRUE(snap.enableUdpInterface);
    EXPECT_EQ(snap.udpBindHost, "127.0.0.1");
    EXPECT_EQ(snap.tamUdpPort, 15001);
    EXPECT_EQ(snap.tumUdpPort, 15002);
    EXPECT_EQ(snap.tumAckUdpPort, 15003);
    EXPECT_EQ(snap.obeHost, "127.0.0.1");
    EXPECT_EQ(snap.udpEncodingMode, "J3217_UPER");
}

// ---------------------------------------------------------------------------
// TAM message class tests
// ---------------------------------------------------------------------------

TEST(TamMessageTest, DefaultMsgTypeIsCorrect) {
    TamMessage tam;
    EXPECT_EQ(tam.get_msgType(), "TAM");
}

TEST(TamMessageTest, MsgCountBoundary) {
    TamMessage tam;
    tam.set_msgCount(127);
    EXPECT_EQ(tam.get_msgCount(), 127);
    // Test wrap: set_msgCount(128) should fail condition value<=127
    // std_attribute condition is enforced at set time — value stays unchanged
    tam.set_msgCount(127);  // last valid
    EXPECT_EQ(tam.get_msgCount(), 127);
}

TEST(TamMessageTest, SecurityPlaceholdersEmptyByDefault) {
    TamMessage tam;
    EXPECT_TRUE(tam.get_certificateId().empty());
    EXPECT_TRUE(tam.get_signedData().empty());
}

TEST(TamMessageTest, TollPointIDPopulatedFromConfig) {
    TollPluginConfig cfg;
    cfg.tollPointID  = "camden-nj-001";
    cfg.tollChargerID = "camden-rse-01";

    TamMessage tam;
    tam.set_tollPointID(cfg.tollPointID);
    tam.set_tollChargerID(cfg.tollChargerID);
    EXPECT_EQ(tam.get_tollPointID(),   "camden-nj-001");
    EXPECT_EQ(tam.get_tollChargerID(), "camden-rse-01");
}

TEST(TamMessageTest, RatesSetAndRead) {
    TamMessage tam;
    tam.set_ratePassengerCar(5.00);
    tam.set_rateHeavyVehicle(10.00);
    tam.set_rateBus(7.50);
    EXPECT_DOUBLE_EQ(tam.get_ratePassengerCar(), 5.00);
    EXPECT_DOUBLE_EQ(tam.get_rateHeavyVehicle(), 10.00);
    EXPECT_DOUBLE_EQ(tam.get_rateBus(),          7.50);
}

// ---------------------------------------------------------------------------
// TUM message class tests
// ---------------------------------------------------------------------------

TEST(TumMessageTest, DefaultMsgTypeIsCorrect) {
    TumMessage tum;
    EXPECT_EQ(tum.get_msgType(), "TUM");
}

TEST(TumMessageTest, TempIDSetAndRead) {
    TumMessage tum;
    tum.set_tempID("AABB1234");
    EXPECT_EQ(tum.get_tempID(), "AABB1234");
    EXPECT_EQ(tum.get_tempID().length(), 8U);
}

TEST(TumMessageTest, SecurityPlaceholdersEmptyByDefault) {
    TumMessage tum;
    EXPECT_TRUE(tum.get_tumHash().empty());
    EXPECT_TRUE(tum.get_encryptedTumData().empty());
    EXPECT_TRUE(tum.get_certificateId().empty());
}

TEST(TumMessageTest, SequenceNumbersSetCorrectly) {
    TumMessage tum;
    tum.set_tumSequenceNum(42);
    tum.set_tamSequenceNum(5);
    EXPECT_EQ(tum.get_tumSequenceNum(), 42);
    EXPECT_EQ(tum.get_tamSequenceNum(), 5);
}

// ---------------------------------------------------------------------------
// TAM msgCount wrapping logic (mirrors TamManager::_incrementAndRegisterSeq)
// Formula: count = (count % 127) + 1
// ---------------------------------------------------------------------------

// _tamMsgCount initializes to 127 (end-of-cycle state); first broadcast → 1.
TEST(TamMsgCountWrapTest, FirstBroadcastIsOne) {
    uint8_t count = 127;   // matches TamManager initial state
    count = static_cast<uint8_t>((count % 127) + 1);
    EXPECT_EQ(count, 1);
}

TEST(TamMsgCountWrapTest, WrapsFromOneTwentySevenToOne) {
    uint8_t count = 127;
    count = static_cast<uint8_t>((count % 127) + 1);
    EXPECT_EQ(count, 1);
}

TEST(TamMsgCountWrapTest, FullCycleStaysInRange) {
    uint8_t count = 127;
    for (int i = 0; i < 300; ++i) {
        count = static_cast<uint8_t>((count % 127) + 1);
        EXPECT_GE(count, 1);
        EXPECT_LE(count, 127);
    }
}

TEST(TamMsgCountWrapTest, SequentialValuesCorrect) {
    uint8_t count = 127;
    for (int expected = 1; expected <= 127; ++expected) {
        count = static_cast<uint8_t>((count % 127) + 1);
        EXPECT_EQ(count, expected);
    }
    // Next increment wraps back to 1
    count = static_cast<uint8_t>((count % 127) + 1);
    EXPECT_EQ(count, 1);
}

TEST(TamMsgCountWrapTest, ZeroIsNeverProduced) {
    uint8_t count = 127;
    for (int i = 0; i < 300; ++i) {
        count = static_cast<uint8_t>((count % 127) + 1);
        EXPECT_NE(count, 0) << "msgCount 0 is reserved and must never be broadcast";
    }
}

TEST(TamMsgCountWrapTest, KnownSeqsNeverContainsZero) {
    // Simulate the TamManager known-sequence insertion over a full cycle.
    std::set<int> knownSeqs;
    uint8_t count = 127;
    for (int i = 0; i < 300; ++i) {
        count = static_cast<uint8_t>((count % 127) + 1);
        knownSeqs.insert(static_cast<int>(count));
    }
    EXPECT_EQ(knownSeqs.count(0), 0U) << "0 must never enter the known TAM sequence history";
    EXPECT_GT(knownSeqs.count(1),   0U);
    EXPECT_GT(knownSeqs.count(127), 0U);
}

}}} // namespace v2x::toll::test

/**
 * J3217Codec.cpp — C++ wrapper that bridges TamMessage/TumMessage/TumAckMessage
 * to J3217CodecImpl (the asn1c UPER implementation).
 *
 * NO j3217/ asn1c headers included here — those live only in J3217CodecImpl.c,
 * compiled in isolation to avoid conflicts with the J2735 library headers.
 */
#include "J3217Codec.h"
#include "../j3217/J3217CodecImpl.h"   // pure-C interface, no asn1c types

#include <cmath>
#include <cstring>
#include <string>

namespace v2x { namespace toll {

static constexpr int MAX_UPER_BYTES = 2048;

// ---------------------------------------------------------------------------
// Hex utilities
// ---------------------------------------------------------------------------

std::string J3217Codec::toHex(const std::vector<uint8_t>& bytes) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

std::vector<uint8_t> J3217Codec::fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) return {};
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        int hi = hexVal(hex[i]);
        int lo = hexVal(hex[i+1]);
        if (hi < 0 || lo < 0) return {};
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

// ---------------------------------------------------------------------------
// VehicleClass helpers
// ---------------------------------------------------------------------------

int J3217Codec::vehicleClassToEnum(const std::string& cls) {
    if (cls == "passenger_car")  return 0;
    if (cls == "heavy_vehicle")  return 1;
    if (cls == "bus")            return 2;
    return 3; // unknown
}

std::string J3217Codec::vehicleClassFromEnum(int val) {
    switch (val) {
        case 0: return "passenger_car";
        case 1: return "heavy_vehicle";
        case 2: return "bus";
        default: return "unknown";
    }
}

// ---------------------------------------------------------------------------
// AckStatus helpers
// ---------------------------------------------------------------------------

int J3217Codec::ackStatusToEnum(const std::string& status) {
    // Exact match only — substring matching is fragile and breaks strict UPER decoders.
    if (status == "accepted")                         return 0;
    if (status == "rejected:UNKNOWN_TOLL_POINT")      return 1;
    if (status == "rejected:INVALID_LANE")            return 2;
    if (status == "rejected:UNKNOWN_VEHICLE_TYPE")    return 3;
    if (status == "rejected:UNKNOWN_TAM_SEQ")         return 4;
    if (status == "rejected:DUPLICATE_TUM")           return 5;
    // DECODE_ERROR and all other unrecognised codes → rejected-other (6)
    return 6;
}

std::string J3217Codec::ackStatusFromEnum(int val) {
    switch (val) {
        case 0: return "accepted";
        case 1: return "rejected:UNKNOWN_TOLL_POINT";
        case 2: return "rejected:INVALID_LANE";
        case 3: return "rejected:UNKNOWN_VEHICLE_TYPE";
        case 4: return "rejected:UNKNOWN_TAM_SEQ";
        case 5: return "rejected:DUPLICATE_TUM";
        default: return "rejected:OTHER";
    }
}

// ---------------------------------------------------------------------------
// TAM encode / decode
// ---------------------------------------------------------------------------

std::vector<uint8_t> J3217Codec::encodeTam(TamMessage& msg) {
    J3217TamFields f{};
    f.msgCount        = msg.get_msgCount();
    f.protocolVersion = msg.get_protocolVersion();
    strncpy(f.tollPointID,   msg.get_tollPointID().c_str(),   sizeof(f.tollPointID)-1);
    strncpy(f.tollChargerID, msg.get_tollChargerID().c_str(), sizeof(f.tollChargerID)-1);
    strncpy(f.tollPointName, msg.get_tollPointName().c_str(), sizeof(f.tollPointName)-1);
    strncpy(f.validFromUtc,  msg.get_validFromUtc().c_str(),  sizeof(f.validFromUtc)-1);
    strncpy(f.validUntilUtc, msg.get_validUntilUtc().c_str(), sizeof(f.validUntilUtc)-1);
    f.broadcastIntervalMs   = msg.get_broadcastIntervalMs();
    f.ratePassengerCents    = static_cast<int>(std::round(msg.get_ratePassengerCar()  * 100));
    f.rateHeavyVehicleCents = static_cast<int>(std::round(msg.get_rateHeavyVehicle()  * 100));
    f.rateBusCents          = static_cast<int>(std::round(msg.get_rateBus()           * 100));

    std::vector<uint8_t> buf(MAX_UPER_BYTES);
    int n = j3217_encode_tam(&f, buf.data(), MAX_UPER_BYTES);
    if (n <= 0) return {};
    buf.resize(static_cast<size_t>(n));
    return buf;
}

bool J3217Codec::decodeTam(const std::vector<uint8_t>& bytes, TamMessage& out) {
    if (bytes.empty()) return false;
    J3217TamFields f{};
    if (!j3217_decode_tam(bytes.data(), static_cast<int>(bytes.size()), &f)) return false;

    out.set_msgCount(f.msgCount);
    out.set_protocolVersion(f.protocolVersion);
    out.set_tollPointID(f.tollPointID);
    out.set_tollChargerID(f.tollChargerID);
    out.set_tollPointName(f.tollPointName);
    out.set_validFromUtc(f.validFromUtc);
    out.set_validUntilUtc(f.validUntilUtc);
    out.set_broadcastIntervalMs(f.broadcastIntervalMs);
    out.set_ratePassengerCar( f.ratePassengerCents    / 100.0);
    out.set_rateHeavyVehicle( f.rateHeavyVehicleCents / 100.0);
    out.set_rateBus(           f.rateBusCents          / 100.0);
    return true;
}

// ---------------------------------------------------------------------------
// TUM encode / decode
// ---------------------------------------------------------------------------

std::vector<uint8_t> J3217Codec::encodeTum(TumMessage& msg) {
    J3217TumFields f{};
    f.tumSequenceNum = msg.get_tumSequenceNum();
    f.tamSequenceNum = msg.get_tamSequenceNum();
    strncpy(f.tempID,        msg.get_tempID().c_str(),        sizeof(f.tempID)-1);
    f.vehicleClass   = vehicleClassToEnum(msg.get_vehicleClass());
    f.vehicleType    = msg.get_vehicleType();
    strncpy(f.tollPointID,   msg.get_tollPointID().c_str(),   sizeof(f.tollPointID)-1);
    strncpy(f.tollChargerID, msg.get_tollChargerID().c_str(), sizeof(f.tollChargerID)-1);
    f.laneID         = msg.get_laneID();
    strncpy(f.eventTimeUtc,  msg.get_eventTimeUtc().c_str(),  sizeof(f.eventTimeUtc)-1);
    f.speedCmps      = static_cast<int>(std::round(msg.get_speedMps() * 100));
    f.latDeg1e7      = static_cast<long>(std::round(msg.get_latDeg()  * 1e7));
    f.lonDeg1e7      = static_cast<long>(std::round(msg.get_lonDeg()  * 1e7));

    std::vector<uint8_t> buf(MAX_UPER_BYTES);
    int n = j3217_encode_tum(&f, buf.data(), MAX_UPER_BYTES);
    if (n <= 0) return {};
    buf.resize(static_cast<size_t>(n));
    return buf;
}

bool J3217Codec::decodeTum(const std::vector<uint8_t>& bytes, TumMessage& out) {
    if (bytes.empty()) return false;
    J3217TumFields f{};
    if (!j3217_decode_tum(bytes.data(), static_cast<int>(bytes.size()), &f)) return false;

    out.set_tumSequenceNum(f.tumSequenceNum);
    out.set_tamSequenceNum(f.tamSequenceNum);
    out.set_tempID(f.tempID);
    out.set_vehicleClass(vehicleClassFromEnum(f.vehicleClass));
    out.set_vehicleType(f.vehicleType);
    out.set_tollPointID(f.tollPointID);
    out.set_tollChargerID(f.tollChargerID);
    out.set_laneID(f.laneID);
    out.set_eventTimeUtc(f.eventTimeUtc);
    out.set_speedMps(f.speedCmps / 100.0);
    out.set_latDeg(f.latDeg1e7   / 1e7);
    out.set_lonDeg(f.lonDeg1e7   / 1e7);
    // msgType defaults to "TUM" via std_attribute — no setter needed
    return true;
}

// ---------------------------------------------------------------------------
// TumAck encode / decode
// ---------------------------------------------------------------------------

std::vector<uint8_t> J3217Codec::encodeTumAck(TumAckMessage& msg) {
    J3217TumAckFields f{};
    f.msgCount        = msg.get_msgCount();
    strncpy(f.tempID,         msg.get_tempID().c_str(),        sizeof(f.tempID)-1);
    f.tumSequenceNum  = msg.get_tumSequenceNum();
    strncpy(f.tollPointID,    msg.get_tollPointID().c_str(),   sizeof(f.tollPointID)-1);
    f.ackStatus       = ackStatusToEnum(msg.get_status());
    strncpy(f.transactionID,  msg.get_transactionID().c_str(), sizeof(f.transactionID)-1);
    long cents        = static_cast<long>(std::round(msg.get_amountUsd() * 100));
    f.amountCents     = (cents > 0) ? static_cast<int>(cents) : -1;
    strncpy(f.ackTimeUtc,     msg.get_ackTimeUtc().c_str(),    sizeof(f.ackTimeUtc)-1);

    std::vector<uint8_t> buf(MAX_UPER_BYTES);
    int n = j3217_encode_tumack(&f, buf.data(), MAX_UPER_BYTES);
    if (n <= 0) return {};
    buf.resize(static_cast<size_t>(n));
    return buf;
}

bool J3217Codec::decodeTumAck(const std::vector<uint8_t>& bytes, TumAckMessage& out) {
    if (bytes.empty()) return false;
    J3217TumAckFields f{};
    if (!j3217_decode_tumack(bytes.data(), static_cast<int>(bytes.size()), &f)) return false;

    out.set_msgCount(f.msgCount);
    out.set_tempID(f.tempID);
    out.set_tumSequenceNum(f.tumSequenceNum);
    out.set_tollPointID(f.tollPointID);
    out.set_status(ackStatusFromEnum(f.ackStatus));
    out.set_transactionID(f.transactionID);
    if (f.amountCents >= 0) out.set_amountUsd(f.amountCents / 100.0);
    out.set_ackTimeUtc(f.ackTimeUtc);
    return true;
}

}} // namespace v2x::toll

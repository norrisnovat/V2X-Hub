/**
 * J3217Codec.h — UPER encode/decode for TAM, TUM, TumAck.
 *
 * Converts between the JSON message classes (TamMessage, TumMessage,
 * TumAckMessage) and asn1c-generated C structs, then UPER-encodes to bytes.
 *
 * Security fields (certificateId, signedData, tumHash, signedTumHash) are
 * accepted as empty strings and encoded as ASN.1 ABSENT (OPTIONAL not present).
 * Deferred to Phase 4 Security.
 *
 * Amounts:  USD → cents (×100) for integer encoding.
 * Speed:    m/s → cm/s (×100) for integer encoding.
 * Lat/lon:  degrees → 1e7 fixed-point integer.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "TamMessage.hpp"
#include "TumMessage.hpp"
#include "TumAckMessage.hpp"

namespace v2x { namespace toll {

class J3217Codec {
public:
    // -----------------------------------------------------------------------
    // UPER encode — returns packed bytes, empty on failure.
    // -----------------------------------------------------------------------
    static std::vector<uint8_t> encodeTam   (TamMessage&    msg);
    static std::vector<uint8_t> encodeTum   (TumMessage&    msg);
    static std::vector<uint8_t> encodeTumAck(TumAckMessage& msg);

    // -----------------------------------------------------------------------
    // UPER decode — populates msg, returns true on success.
    // -----------------------------------------------------------------------
    static bool decodeTam   (const std::vector<uint8_t>& bytes, TamMessage&    out);
    static bool decodeTum   (const std::vector<uint8_t>& bytes, TumMessage&    out);
    static bool decodeTumAck(const std::vector<uint8_t>& bytes, TumAckMessage& out);

    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------
    // Hex utilities — encode UPER bytes as hex string for TMX bus transport.
    // -----------------------------------------------------------------------
    static std::string          toHex  (const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> fromHex(const std::string& hex);

    // VehicleClass string ↔ ASN.1 enum helpers (shared with tests).
    // -----------------------------------------------------------------------
    static int         vehicleClassToEnum (const std::string& cls);
    static std::string vehicleClassFromEnum(int val);

    // AckStatus string ↔ ASN.1 enum helpers.
    static int         ackStatusToEnum   (const std::string& status);
    static std::string ackStatusFromEnum (int val);
};

}} // namespace v2x::toll

/**
 * TamMessage.hpp — Toll Advertisement Message (J3217)
 *
 * tmx::message subclass for TAM (TollAdvertisementMessage).
 * Direction: TollPlugin (RSE) → OBE broadcast.
 *
 * Uses V2X-Hub std_attribute() macro to generate typed get/set accessors.
 * Registered with TMX type system as type="J3217" subtype="TAM".
 *
 * Phase 2: JSON encoding. Phase 3: ASN.1 UPER via asn.1-uper/hexstring.
 *
 * NOTE: All #include paths assume V2X-Hub TMX headers are on the include path.
 *       TO VERIFY on Ubuntu: exact header location for tmx::message.
 */
#pragma once

#include <tmx/messages/message.hpp>

namespace v2x { namespace toll {

/**
 * TollAdvertisementMessage — broadcast by RSE once per BroadcastIntervalMs.
 * OBE must receive a valid TAM before it may send a TUM (J3217 §5.3).
 */
class TamMessage : public tmx::message {
public:
    TamMessage() = default;
    virtual ~TamMessage() = default;

    // Required no-arg constructor for TMX message system.
    static constexpr const char* MessageType    = "J3217";
    static constexpr const char* MessageSubType = "TAM";

    // ----------------------------------------------------------------
    // Header fields
    // ----------------------------------------------------------------

    /** SAE J3217 message type identifier. Always "TAM". */
    std_attribute(this->msg, std::string, msgType, "TAM",
                  if (value == "TAM"))

    /** J2735 MsgCount: 0-127 wrapping per transmission. */
    std_attribute(this->msg, int, msgCount, 0,
                  if (value >= 0 && value <= 127))

    /** Protocol version. 1 = Phase 2 JSON. Future: 2 = ASN.1. */
    std_attribute(this->msg, int, protocolVersion, 1,
                  if (value >= 1))

    // ----------------------------------------------------------------
    // Toll point identity (J3217 TollPointDescription)
    // ----------------------------------------------------------------

    /** J3217 TollPointID — globally unique toll point identifier. */
    std_attribute(this->msg, std::string, tollPointID, "",
                  if (!value.empty()))

    /** J3217 TollChargerID — identifies this RSE charger unit. */
    std_attribute(this->msg, std::string, tollChargerID, "",
                  if (!value.empty()))

    /** Human-readable name for admin display. Not transmitted to OBE. */
    std_attribute(this->msg, std::string, tollPointName, "", )

    // ----------------------------------------------------------------
    // Validity window
    // ----------------------------------------------------------------

    /** ISO 8601 UTC: TAM valid-from time. */
    std_attribute(this->msg, std::string, validFromUtc, "", )

    /** ISO 8601 UTC: TAM expiry time. OBE must discard after this time. */
    std_attribute(this->msg, std::string, validUntilUtc, "", )

    /** Milliseconds between broadcasts. OBE uses this for TAM-loss detection. */
    std_attribute(this->msg, int, broadcastIntervalMs, 1000,
                  if (value >= 100))

    // ----------------------------------------------------------------
    // RSE network location (OBE uses these to address TUM)
    // ----------------------------------------------------------------

    /** IP or hostname of RSE TUM listener. */
    std_attribute(this->msg, std::string, rseHost, "", )

    /** UDP port of RSE TUM listener (default 5002). */
    std_attribute(this->msg, int, tumPort, 5002,
                  if (value > 0 && value < 65536))

    // ----------------------------------------------------------------
    // Security placeholder (Phase 3: IEEE 1609.2 / SCMS)
    // ----------------------------------------------------------------

    /** Phase 3: SCMS Authorization Ticket reference. Empty in Phase 2. */
    std_attribute(this->msg, std::string, certificateId, "", )

    /** Phase 3: IEEE 1609.2 signed payload (ETSI TS 103 097). Empty in Phase 2. */
    std_attribute(this->msg, std::string, signedData, "", )

    // ----------------------------------------------------------------
    // Toll charges table and lane charges are JSON arrays.
    // They are stored in the message container as nested JSON.
    // Access via tmx::message tree manipulation (to_tree / from_tree).
    // Typed helpers below provide simplified single-class-of-service access.
    // ----------------------------------------------------------------

    /** Rate for passenger_car class in USD. Convenience accessor. */
    std_attribute(this->msg, double, ratePassengerCar, 5.00,
                  if (value >= 0.0))

    /** Rate for heavy_vehicle class in USD. */
    std_attribute(this->msg, double, rateHeavyVehicle, 10.00,
                  if (value >= 0.0))

    /** Rate for bus class in USD. */
    std_attribute(this->msg, double, rateBus, 7.50,
                  if (value >= 0.0))
};

}} // namespace v2x::toll

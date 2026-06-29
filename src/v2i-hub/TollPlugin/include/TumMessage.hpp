/**
 * TumMessage.hpp — Toll Usage Message (J3217)
 *
 * tmx::message subclass for TUM (TollUsageMessage).
 * Direction: OBE → TollPlugin (RSE).
 * Received by TollPlugin via AddMessageFilter() on type="J3217" subtype="TUM".
 *
 * OBE must not send TUM unless a valid TAM has been received (J3217 §5.3).
 */
#pragma once

#include <tmx/messages/message.hpp>

namespace v2x { namespace toll {

class TumMessage : public tmx::message {
public:
    TumMessage() = default;
    virtual ~TumMessage() = default;

    static constexpr const char* MessageType    = "J3217";
    static constexpr const char* MessageSubType = "TUM";

    // ----------------------------------------------------------------
    // Header
    // ----------------------------------------------------------------

    /** Always "TUM". Validated in step 1 of TumManager pipeline. */
    std_attribute(this->msg, std::string, msgType, "TUM",
                  if (value == "TUM"))

    /** Per-OBE, per-tollPoint sequence counter (0-65535). Used for duplicate detection. */
    std_attribute(this->msg, int, tumSequenceNum, 0,
                  if (value >= 0))

    /** J2735 MsgCount echoed from the TAM that authorized this TUM (0-127). */
    std_attribute(this->msg, int, tamSequenceNum, 0,
                  if (value >= 0 && value <= 127))

    // ----------------------------------------------------------------
    // Vehicle identity (J2735 / J3217)
    // ----------------------------------------------------------------

    /**
     * J2735 TemporaryID: 4-byte hex pseudonym (e.g. "AABB1234").
     * Phase 2: derived from Vissim vehicle ID & 0xFFFFFFFF.
     * Phase 3: real SCMS pseudonym with certificate binding.
     */
    std_attribute(this->msg, std::string, tempID, "",
                  if (value.length() == 8))

    /**
     * J3217 vehicle class string.
     * Valid values: passenger_car | heavy_vehicle | bus
     */
    std_attribute(this->msg, std::string, vehicleClass, "", )

    /**
     * Vissim vehicle type number (Phase 2 simulation field only).
     * 610=CV Car, 620=CV Truck, 630=CV Bus.
     * Not transmitted in Phase 3 real deployments.
     */
    std_attribute(this->msg, int, vehicleType, 0, )

    // ----------------------------------------------------------------
    // Toll point reference (must match RSE configuration)
    // ----------------------------------------------------------------

    /** Must match TollPlugin config parameter TollPointID. */
    std_attribute(this->msg, std::string, tollPointID, "",
                  if (!value.empty()))

    /** Must match TollPlugin config parameter TollChargerID. */
    std_attribute(this->msg, std::string, tollChargerID, "", )

    // ----------------------------------------------------------------
    // Lane identification
    // ----------------------------------------------------------------

    /** J3217 LaneID. Must be in the configured valid lane set. */
    std_attribute(this->msg, int, laneID, 0,
                  if (value > 0))

    /** Descriptive lane type. Informational. */
    std_attribute(this->msg, std::string, laneType, "", )

    // ----------------------------------------------------------------
    // Timestamps
    // ----------------------------------------------------------------

    /** ISO 8601 UTC wall-clock time of gantry crossing. */
    std_attribute(this->msg, std::string, eventTimeUtc, "",
                  if (!value.empty()))

    /**
     * Vissim simulation time in seconds (Phase 2 only).
     * Used for duplicate detection window comparison.
     */
    std_attribute(this->msg, double, simTimeSec, 0.0,
                  if (value >= 0.0))

    // ----------------------------------------------------------------
    // Position and dynamics
    // ----------------------------------------------------------------

    /** Speed at gantry crossing in m/s. */
    std_attribute(this->msg, double, speedMps, 0.0,
                  if (value >= 0.0))

    /** WGS84 latitude in decimal degrees. */
    std_attribute(this->msg, double, latDeg, 0.0, )

    /** WGS84 longitude in decimal degrees. */
    std_attribute(this->msg, double, lonDeg, 0.0, )

    // ----------------------------------------------------------------
    // Security placeholders (Phase 3: IEEE 1609.2 / SCMS)
    // ----------------------------------------------------------------

    /** Phase 3: SHA-256 hash of TUM content for integrity verification. */
    std_attribute(this->msg, std::string, tumHash, "", )

    /** Phase 3: AES-128-CCM encrypted TUM data. */
    std_attribute(this->msg, std::string, encryptedTumData, "", )

    /** Phase 3: SCMS pseudonym certificate reference. */
    std_attribute(this->msg, std::string, certificateId, "", )
};

}} // namespace v2x::toll

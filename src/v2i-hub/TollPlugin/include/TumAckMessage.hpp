/**
 * TumAckMessage.hpp — Toll Usage Acknowledgment Message (J3217)
 *
 * tmx::message subclass for TumAck (TollUsageAckMessage).
 * Direction: TollPlugin (RSE) → OBE.
 *
 * Sent after TUM validation succeeds (status="accepted")
 * or fails (status="rejected:<code>").
 *
 * Structure supports a single-TUM acknowledgment in Phase 2.
 * The architecture supports batch acknowledgment in Phase 3
 * via extension of the tumAck array (see TollPlugin_Architecture.md §2.3).
 */
#pragma once

#include <tmx/messages/message.hpp>

namespace v2x { namespace toll {

class TumAckMessage : public tmx::message {
public:
    TumAckMessage() = default;
    virtual ~TumAckMessage() = default;

    static constexpr const char* MessageType    = "J3217";
    static constexpr const char* MessageSubType = "TumAck";

    // ----------------------------------------------------------------
    // Header
    // ----------------------------------------------------------------

    /** Always "TumAck". */
    std_attribute(this->msg, std::string, msgType, "TumAck",
                  if (value == "TumAck"))

    /** J2735 MsgCount: 0-127 wrapping. Incremented per TumAck sent. */
    std_attribute(this->msg, int, msgCount, 0,
                  if (value >= 0 && value <= 127))

    /**
     * Maximum age in seconds OBE should consider this TumAck valid.
     * If OBE does not receive TumAck within ackMaxAge seconds it should retry.
     */
    std_attribute(this->msg, int, ackMaxAge, 300,
                  if (value > 0))

    // ----------------------------------------------------------------
    // Vehicle / TUM reference (echoed from incoming TUM)
    // ----------------------------------------------------------------

    /** J2735 TemporaryID echoed from TUM. OBE matches this to its pending TUM. */
    std_attribute(this->msg, std::string, tempID, "", )

    /** tumSequenceNum echoed from TUM. Used with tempID for matching. */
    std_attribute(this->msg, int, tumSequenceNum, 0,
                  if (value >= 0))

    // ----------------------------------------------------------------
    // Transaction result
    // ----------------------------------------------------------------

    /** Toll point this transaction was processed at. */
    std_attribute(this->msg, std::string, tollPointID, "", )

    /** RSE-assigned transaction ID. Empty if rejected. */
    std_attribute(this->msg, std::string, transactionID, "", )

    /**
     * Acknowledgment status.
     * "accepted"         — transaction created, charge applied.
     * "rejected:<code>"  — see TollPlugin_Architecture.md Appendix for codes.
     *
     * Rejection codes (J3217-aligned):
     *   rejected:PARSE_ERROR
     *   rejected:WRONG_MSG_TYPE
     *   rejected:MISSING_REQUIRED_FIELD
     *   rejected:UNKNOWN_TOLL_POINT
     *   rejected:UNKNOWN_TOLL_CHARGER
     *   rejected:INVALID_LANE
     *   rejected:UNKNOWN_VEHICLE_TYPE
     *   rejected:UNKNOWN_TAM_SEQ
     *   rejected:DUPLICATE_TUM
     *   rejected:MISSING_CERT_PLACEHOLDER
     *   rejected:INVALID_SIGNATURE  (Phase 3)
     */
    std_attribute(this->msg, std::string, status, "", )

    /** Human-readable rejection description. Empty if accepted. */
    std_attribute(this->msg, std::string, rejectionReason, "", )

    // ----------------------------------------------------------------
    // Financial
    // ----------------------------------------------------------------

    /** Transaction amount in USD. 0.0 if rejected. */
    std_attribute(this->msg, double, amountUsd, 0.0,
                  if (value >= 0.0))

    /** Currency code. Always "USD" in Phase 2. */
    std_attribute(this->msg, std::string, currency, "USD", )

    // ----------------------------------------------------------------
    // Timestamp
    // ----------------------------------------------------------------

    /** ISO 8601 UTC time this TumAck was generated. */
    std_attribute(this->msg, std::string, ackTimeUtc, "", )

    // ----------------------------------------------------------------
    // Security placeholders (Phase 3: IEEE 1609.2 / SCMS)
    // ----------------------------------------------------------------

    /** Phase 3: RSE-signed hash of the original TUM. */
    std_attribute(this->msg, std::string, signedTumHash, "", )

    /** Phase 3: RSE signing certificate reference. */
    std_attribute(this->msg, std::string, certificateId, "", )
};

}} // namespace v2x::toll

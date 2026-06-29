/**
 * J3217CodecImpl.c — UPER encode/decode implementation using asn1c.
 *
 * Compiled in complete isolation from the J2735 library headers.
 * Only includes j3217/ generated headers and J3217CodecImpl.h.
 * NO tmxutils, NO J2735 library headers included here.
 */
#include "J3217CodecImpl.h"

/* asn1c 0.9.28 generated headers — must be the ONLY asn1c runtime here */
#include "TollAdvertisementMessage.h"
#include "TollUsageMessage.h"
#include "TollUsageAckMessage.h"
#include "per_encoder.h"
#include "per_decoder.h"

#include <string.h>
#include <stdlib.h>

#define MAX_UPER_BUF 2048

/* -----------------------------------------------------------------------
 * String helpers
 * ----------------------------------------------------------------------- */
static void str_to_vs(VisibleString_t* vs, const char* s) {
    OCTET_STRING_fromBuf(vs, s, (int)strlen(s));
}

static void vs_to_str(const VisibleString_t* vs, char* dst, size_t cap) {
    size_t n = vs->size < (int)(cap-1) ? (size_t)vs->size : cap-1;
    memcpy(dst, vs->buf, n);
    dst[n] = '\0';
}

static VisibleString_t* opt_vs(const char* s) {
    if (!s || s[0] == '\0') return NULL;
    VisibleString_t* p = calloc(1, sizeof(VisibleString_t));
    OCTET_STRING_fromBuf(p, s, (int)strlen(s));
    return p;
}

static long* opt_long(int val) {
    if (val < 0) return NULL;
    long* p = malloc(sizeof(long));
    *p = (long)val;
    return p;
}

/* -----------------------------------------------------------------------
 * TAM encode / decode
 * ----------------------------------------------------------------------- */
int j3217_encode_tam(const J3217TamFields* f, uint8_t* buf, int max_bytes) {
    TollAdvertisementMessage_t tam;
    memset(&tam, 0, sizeof(tam));

    tam.msgCount        = (MsgCount_t)f->msgCount;
    tam.protocolVersion = f->protocolVersion;
    str_to_vs(&tam.tollPointID,   f->tollPointID);
    str_to_vs(&tam.tollChargerID, f->tollChargerID);
    tam.tollPointName   = opt_vs(f->tollPointName);
    str_to_vs(&tam.validFromUtc,  f->validFromUtc);
    str_to_vs(&tam.validUntilUtc, f->validUntilUtc);
    tam.broadcastIntervalMs   = f->broadcastIntervalMs;
    tam.ratePassengerCents    = f->ratePassengerCents;
    tam.rateHeavyVehicleCents = f->rateHeavyVehicleCents;
    tam.rateBusCents          = f->rateBusCents;
    tam.certificateId = NULL;
    tam.signedData    = NULL;

    asn_enc_rval_t rv = uper_encode_to_buffer(
        (asn_TYPE_descriptor_t*)&asn_DEF_TollAdvertisementMessage,
        &tam, buf, (size_t)max_bytes);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_TollAdvertisementMessage, &tam);

    if (rv.encoded < 0) return -1;
    return (int)((rv.encoded + 7) / 8);
}

int j3217_decode_tam(const uint8_t* buf, int n, J3217TamFields* out) {
    TollAdvertisementMessage_t* tam = NULL;
    asn_dec_rval_t rv = uper_decode_complete(
        NULL, (asn_TYPE_descriptor_t*)&asn_DEF_TollAdvertisementMessage,
        (void**)&tam, buf, (size_t)n);
    if (rv.code != RC_OK || !tam) {
        ASN_STRUCT_FREE(asn_DEF_TollAdvertisementMessage, tam);
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->msgCount        = (int)tam->msgCount;
    out->protocolVersion = (int)tam->protocolVersion;
    vs_to_str(&tam->tollPointID,   out->tollPointID,   sizeof(out->tollPointID));
    vs_to_str(&tam->tollChargerID, out->tollChargerID, sizeof(out->tollChargerID));
    if (tam->tollPointName)
        vs_to_str(tam->tollPointName, out->tollPointName, sizeof(out->tollPointName));
    vs_to_str(&tam->validFromUtc,  out->validFromUtc,  sizeof(out->validFromUtc));
    vs_to_str(&tam->validUntilUtc, out->validUntilUtc, sizeof(out->validUntilUtc));
    out->broadcastIntervalMs   = (int)tam->broadcastIntervalMs;
    out->ratePassengerCents    = (int)tam->ratePassengerCents;
    out->rateHeavyVehicleCents = (int)tam->rateHeavyVehicleCents;
    out->rateBusCents          = (int)tam->rateBusCents;
    ASN_STRUCT_FREE(asn_DEF_TollAdvertisementMessage, tam);
    return 1;
}

/* -----------------------------------------------------------------------
 * TUM encode / decode
 * ----------------------------------------------------------------------- */
int j3217_encode_tum(const J3217TumFields* f, uint8_t* buf, int max_bytes) {
    TollUsageMessage_t tum;
    memset(&tum, 0, sizeof(tum));

    tum.tumSequenceNum = (TumSeqNum_t)f->tumSequenceNum;
    tum.tamSequenceNum = (MsgCount_t)f->tamSequenceNum;
    str_to_vs(&tum.tempID,        f->tempID);
    tum.vehicleClass   = (VehicleClass_t)f->vehicleClass;
    tum.vehicleType    = f->vehicleType;
    str_to_vs(&tum.tollPointID,   f->tollPointID);
    str_to_vs(&tum.tollChargerID, f->tollChargerID);
    tum.laneID         = (LaneID_t)f->laneID;
    str_to_vs(&tum.eventTimeUtc,  f->eventTimeUtc);
    tum.speedCmps      = f->speedCmps;
    tum.latDeg1e7      = f->latDeg1e7;
    tum.lonDeg1e7      = f->lonDeg1e7;
    tum.tumHash       = NULL;
    tum.certificateId = NULL;

    asn_enc_rval_t rv = uper_encode_to_buffer(
        (asn_TYPE_descriptor_t*)&asn_DEF_TollUsageMessage,
        &tum, buf, (size_t)max_bytes);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_TollUsageMessage, &tum);
    if (rv.encoded < 0) return -1;
    return (int)((rv.encoded + 7) / 8);
}

int j3217_decode_tum(const uint8_t* buf, int n, J3217TumFields* out) {
    TollUsageMessage_t* tum = NULL;
    asn_dec_rval_t rv = uper_decode_complete(
        NULL, (asn_TYPE_descriptor_t*)&asn_DEF_TollUsageMessage,
        (void**)&tum, buf, (size_t)n);
    if (rv.code != RC_OK || !tum) {
        ASN_STRUCT_FREE(asn_DEF_TollUsageMessage, tum);
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->tumSequenceNum = (int)tum->tumSequenceNum;
    out->tamSequenceNum = (int)tum->tamSequenceNum;
    vs_to_str(&tum->tempID,        out->tempID,        sizeof(out->tempID));
    out->vehicleClass = (int)tum->vehicleClass;
    out->vehicleType  = (int)tum->vehicleType;
    vs_to_str(&tum->tollPointID,   out->tollPointID,   sizeof(out->tollPointID));
    vs_to_str(&tum->tollChargerID, out->tollChargerID, sizeof(out->tollChargerID));
    out->laneID     = (int)tum->laneID;
    vs_to_str(&tum->eventTimeUtc,  out->eventTimeUtc,  sizeof(out->eventTimeUtc));
    out->speedCmps  = (int)tum->speedCmps;
    out->latDeg1e7  = tum->latDeg1e7;
    out->lonDeg1e7  = tum->lonDeg1e7;
    ASN_STRUCT_FREE(asn_DEF_TollUsageMessage, tum);
    return 1;
}

/* -----------------------------------------------------------------------
 * TumAck encode / decode
 * ----------------------------------------------------------------------- */
int j3217_encode_tumack(const J3217TumAckFields* f, uint8_t* buf, int max_bytes) {
    TollUsageAckMessage_t ack;
    memset(&ack, 0, sizeof(ack));

    ack.msgCount        = (MsgCount_t)f->msgCount;
    str_to_vs(&ack.tempID,       f->tempID);
    ack.tumSequenceNum  = (TumSeqNum_t)f->tumSequenceNum;
    str_to_vs(&ack.tollPointID,  f->tollPointID);
    ack.ackStatus       = (AckStatus_t)f->ackStatus;
    ack.transactionID   = opt_vs(f->transactionID);
    ack.amountCents     = opt_long(f->amountCents);
    str_to_vs(&ack.ackTimeUtc,   f->ackTimeUtc);
    ack.signedTumHash  = NULL;
    ack.certificateId  = NULL;

    asn_enc_rval_t rv = uper_encode_to_buffer(
        (asn_TYPE_descriptor_t*)&asn_DEF_TollUsageAckMessage,
        &ack, buf, (size_t)max_bytes);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_TollUsageAckMessage, &ack);
    if (rv.encoded < 0) return -1;
    return (int)((rv.encoded + 7) / 8);
}

int j3217_decode_tumack(const uint8_t* buf, int n, J3217TumAckFields* out) {
    TollUsageAckMessage_t* ack = NULL;
    asn_dec_rval_t rv = uper_decode_complete(
        NULL, (asn_TYPE_descriptor_t*)&asn_DEF_TollUsageAckMessage,
        (void**)&ack, buf, (size_t)n);
    if (rv.code != RC_OK || !ack) {
        ASN_STRUCT_FREE(asn_DEF_TollUsageAckMessage, ack);
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->msgCount       = (int)ack->msgCount;
    vs_to_str(&ack->tempID,       out->tempID,       sizeof(out->tempID));
    out->tumSequenceNum = (int)ack->tumSequenceNum;
    vs_to_str(&ack->tollPointID,  out->tollPointID,  sizeof(out->tollPointID));
    out->ackStatus      = (int)ack->ackStatus;
    if (ack->transactionID)
        vs_to_str(ack->transactionID, out->transactionID, sizeof(out->transactionID));
    out->amountCents    = ack->amountCents ? (int)*ack->amountCents : -1;
    vs_to_str(&ack->ackTimeUtc,   out->ackTimeUtc,   sizeof(out->ackTimeUtc));
    ASN_STRUCT_FREE(asn_DEF_TollUsageAckMessage, ack);
    return 1;
}

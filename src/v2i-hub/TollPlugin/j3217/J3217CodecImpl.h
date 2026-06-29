/**
 * J3217CodecImpl.h — Pure C interface between J3217 UPER codec and the rest
 * of the plugin.
 *
 * No asn1c types exposed. No TMX types exposed.
 * Allows J3217CodecImpl.c (asn1c world) to compile in total isolation from
 * the J2735 library headers (different asn1c version).
 *
 * All strings are null-terminated, caller provides fixed-size buffers.
 * Amounts are in cents (integer). Speed is cm/s. Lat/lon are 1e7 fixed-point.
 */
#ifndef J3217_CODEC_IMPL_H
#define J3217_CODEC_IMPL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * TAM fields
 * ----------------------------------------------------------------------- */
typedef struct {
    int  msgCount;
    int  protocolVersion;
    char tollPointID[65];
    char tollChargerID[65];
    char tollPointName[129];   /* empty string = absent */
    char validFromUtc[33];
    char validUntilUtc[33];
    int  broadcastIntervalMs;
    int  ratePassengerCents;
    int  rateHeavyVehicleCents;
    int  rateBusCents;
} J3217TamFields;

/* -----------------------------------------------------------------------
 * TUM fields
 * ----------------------------------------------------------------------- */
typedef struct {
    int  tumSequenceNum;
    int  tamSequenceNum;
    char tempID[9];            /* exactly 8 hex chars + NUL */
    int  vehicleClass;         /* 0=passenger_car 1=heavy_vehicle 2=bus 3=unknown */
    int  vehicleType;
    char tollPointID[65];
    char tollChargerID[65];
    int  laneID;
    char eventTimeUtc[33];
    int  speedCmps;
    long latDeg1e7;
    long lonDeg1e7;
} J3217TumFields;

/* -----------------------------------------------------------------------
 * TumAck fields
 * ----------------------------------------------------------------------- */
typedef struct {
    int  msgCount;
    char tempID[9];
    int  tumSequenceNum;
    char tollPointID[65];
    int  ackStatus;            /* 0=accepted 1-5=specific rejection 6=other */
    char transactionID[65];    /* empty = absent */
    int  amountCents;          /* -1 = absent */
    char ackTimeUtc[33];
} J3217TumAckFields;

/* -----------------------------------------------------------------------
 * Encode — returns bytes written (>=0) or -1 on failure.
 * ----------------------------------------------------------------------- */
int j3217_encode_tam   (const J3217TamFields*    f, uint8_t* buf, int max_bytes);
int j3217_encode_tum   (const J3217TumFields*    f, uint8_t* buf, int max_bytes);
int j3217_encode_tumack(const J3217TumAckFields* f, uint8_t* buf, int max_bytes);

/* -----------------------------------------------------------------------
 * Decode — returns 1 on success, 0 on failure.
 * ----------------------------------------------------------------------- */
int j3217_decode_tam   (const uint8_t* buf, int n, J3217TamFields*    out);
int j3217_decode_tum   (const uint8_t* buf, int n, J3217TumFields*    out);
int j3217_decode_tumack(const uint8_t* buf, int n, J3217TumAckFields* out);

#ifdef __cplusplus
}
#endif
#endif /* J3217_CODEC_IMPL_H */

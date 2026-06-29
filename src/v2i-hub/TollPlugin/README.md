# TollPlugin

SAE J3217 Electronic Toll Collection plugin for V2X Hub.

## Overview

TollPlugin implements the RSE (Roadside Equipment) side of the J3217 V2X-based fee collection system:

1. Broadcasts **TAM** (TollAdvertisementMessage) to announce toll zone to nearby OBEs
2. Receives **TUM** (TollUsageMessage) from OBEs when vehicles cross a gantry
3. Validates TUM through an 8-step pipeline
4. Creates toll transactions and writes to JSONL audit log
5. Sends **TumAck** (TollUsageAckMessage) confirming acceptance or rejection

## Standards

| Standard | Scope |
|---|---|
| SAE J3217 | V2X Fee Collection — message flow, field names, validation |
| SAE J2735 | MsgCount (0-127), TemporaryID (tempID), vehicle classes |
| IEEE 1609.2 | Phase 3: message signing readiness (placeholder fields present) |
| ETSI TS 103 097 | Phase 3: security header readiness |
| V2X Hub Programming Guide | PluginClientClockAware, BroadcastMessage, SetStatus, PLOG |

## ASN.1 Schema

The ASN.1 message definitions for TAM, TUM, and TumAck are derived from **SAE J3217**, which is a
paid standard. The schema is not included in this repository.

To obtain the standard:
- Purchase from SAE International: https://www.sae.org/standards/content/j3217/

The generated C codec files (`j3217/*.c`, `j3217/*.h`) are included and were produced using
[asn1c](https://github.com/vlm/asn1c) from the SAE J3217 ASN.1 definitions.

## Message Flow

Preferred VISSIM simulation runtime:

```
VISSIM DriverModel DLL  <-- UDP 5001 TAM ----  TollPlugin
VISSIM DriverModel DLL  -- UDP 5002 TUM ---->  TollPlugin
VISSIM DriverModel DLL  <-- UDP 5003 TumAck -  TollPlugin
```

TollPlugin still publishes TAM/TUM/TumAck activity and status to TMX so the V2X Hub UI remains useful for visibility.

```
RSE (TollPlugin)                    OBE (vehicle)
      |                                  |
      |--- TAM (broadcast, 1 Hz) ------->|
      |                                  |
      |                       [vehicle crosses gantry]
      |                                  |
      |<-- TUM (unicast) ----------------|
      |                                  |
      | [8-step validation]              |
      | [create transaction]             |
      |                                  |
      |--- TumAck (accepted|rejected) -->|
```

## Configuration

Set in V2X Hub Admin Portal or `manifest.json` defaults:

| Key | Default | Description |
|---|---|---|
| TollPointID | camden-nj-001 | J3217 unique toll point ID |
| TollChargerID | camden-rse-01 | J3217 RSE charger ID |
| TamBroadcastIntervalMs | 1000 | TAM frequency (ms) |
| ValidLaneIDs | 1-13 | Accepted gantry lane IDs |
| RatePassengerCar | 5.00 | USD toll for passenger car |
| RateHeavyVehicle | 10.00 | USD toll for truck |
| RateBus | 7.50 | USD toll for bus |
| TransactionLogPath | /var/log/TollPlugin/... | JSONL transaction output |
| SecurityMode | PHASE2_NO_SECURITY | Phase 3: PHASE3_SCMS |
| MessageEncodingMode | JSON | TMX bus encoding: JSON, J3217_UPER, or DUAL |
| EnableUdpInterface | false | Enable direct UDP interface for VISSIM/OBU simulation |
| UdpBindHost | 0.0.0.0 | Local IPv4 address for TUM UDP listener |
| TamUdpPort | 5001 | UDP destination port for TAM |
| TumUdpPort | 5002 | UDP listener port for TUM |
| TumAckUdpPort | 5003 | UDP destination port for TumAck |
| ObeHost | 10.0.0.2 | Windows/VISSIM host IPv4 address |
| UdpEncodingMode | JSON | UDP payload encoding: JSON, J3217_UPER, or DUAL |

## Run / Restart V2X Hub With TollPlugin

Use the helper script from the `V2X-tolling` repo. It layers the TollPlugin compose overlay on top of the official V2X Hub compose file and sets the required host volume path.

```bash
cd ~/repos/V2X-tolling
./scripts/run-toll-stack.sh up
./scripts/run-toll-stack.sh down
./scripts/run-toll-stack.sh logs
```

Do not run `docker/docker-compose.toll.yml` by itself. It is an overlay file only; running it alone can create a second V2X Hub container and fail with port conflicts on UDP `5001-5003`.

Admin UI:

```text
https://localhost
```

To confirm the stack is running:

```bash
docker ps
```

## Build And Test

```bash
cd ~/repos/V2X-tolling
./scripts/build-plugin.sh tests
```

The helper script links this plugin into the sibling V2X-Hub source tree, configures the V2X-Hub build if needed, builds `TollPlugin`, and then builds `TollPluginTests` when `tests` is passed.

Environment overrides:

```bash
V2X_HUB_DIR=/path/to/V2X-Hub ./scripts/build-plugin.sh tests
V2X_HUB_BUILD_DIR=/path/to/build ./scripts/build-plugin.sh tests
```

## Output Files

| File | Contents |
|---|---|
| `v2xhub_toll_transactions.jsonl` | Accepted transactions (one JSON record per line) |
| `audit.jsonl` | All TUMs received, accepted and rejected |

## Validation Pipeline (8 steps)

1. `msgType == "TUM"`
2. `tollPointID` matches config
3. `tollChargerID` matches config (if present in TUM)
4. `laneID` is in `ValidLaneIDs`
5. `vehicleClass` has configured rate
6. `tamSequenceNum` was recently broadcast by this RSE
7. `(tempID, tumSequenceNum)` not seen within deduplication window
8. `certificateId` placeholder present (Phase 3: real SCMS verification)

## Current Status

| Area | Status |
|---|---|
| TollPlugin build/tests | Current target is `./scripts/build-plugin.sh tests` |
| Direct VISSIM UDP path | JSON and J3217_UPER TAM/TUM/TumAck paths are supported |
| J3217 UPER codec | Implemented for TollPlugin and Windows DriverModel runtime/testing |
| Windows DriverModel UPER runtime | `encoding_mode = J3217_UPER` sends and receives UPER UDP payloads |
| Security | `PHASE2_NO_SECURITY` is current; SCMS/IEEE 1609.2 remains future work |

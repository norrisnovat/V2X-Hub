# TumInjector — Synthetic TUM Injection Tool

Development-only tool for proving the `TAM → TUM → TumAck → transaction` path
without VISSIM. Publishes one `TumMessage` onto the V2X-Hub TMX bus and exits.

---

## Prerequisites

- tmxcore running inside the devcontainer
- TollPlugin registered, loaded, and broadcasting TAMs
- Note the current TAM `msgCount` from the TollPlugin log (needed as `TAM_SEQ_NUM`)

---

## Build

Inside the devcontainer:

```bash
cmake --build /workspace/src/build --target TumInjector -j$(nproc)
cmake --install /workspace/src/build --component TumInjector
```

---

## Register in the database

TumInjector must be registered in the `plugin` and `installedPlugin` tables
before tmxcore will accept its connection:

```bash
docker exec mysql mysql -u IVP "-pChangeMe123!" IVP -e "
INSERT INTO plugin (name, description, version)
  VALUES ('TumInjector', 'Synthetic TUM injection for dev testing', '1.0.0');
SET @pid = LAST_INSERT_ID();
INSERT INTO installedPlugin (pluginId, path, exeName, manifestName,
  commandLineParameters, enabled, maxMessageInterval)
  VALUES (@pid, '/var/www/plugins/TumInjector', 'TumInjector', '', '', 1, 5000);
"
```

---

## Run

```bash
# Check the TollPlugin log for a recent TAM msgCount, e.g. 42
TAM_SEQ_NUM=42 /var/www/plugins/TumInjector/TumInjector
```

---

## Expected logs

**TumInjector output:**
```
TumInjector waiting for registration (tamSeq=42)
TumInjector publishing TUM: tempID=ABCD1234 lane=3 class=passenger_car
TumInjector done — exiting
```

**TollPlugin log (in tmxcore output):**
```
TUM received: tempID=ABCD1234 lane=3 class=passenger_car
TUM accepted: tempID=ABCD1234 amount=5.00
TumAck broadcast: status=accepted transactionID=RSE-...
```

---

## Expected transaction output

```bash
cat /var/log/TollPlugin/v2xhub_toll_transactions.jsonl
```

```json
{"transactionID":"RSE-...","tempID":"ABCD1234","tollPointID":"camden-nj-001",
 "vehicleClass":"passenger_car","laneID":3,"amountUsd":5.00,"status":"accepted",...}
```

```bash
cat /var/log/TollPlugin/audit.jsonl
```

The audit log records every TUM (accepted and rejected). One entry expected.

---

## Expected TumAck

TumAck is broadcast on the TMX bus (type=J3217, subtype=TumAck). To observe it,
subscribe with a second plugin or check MessageProfiler via the V2X-Hub admin UI.

---

## Injecting a rejected TUM

To test rejection handling, use an invalid lane:

```bash
# laneID=99 is not in ValidLaneIDs — will be rejected
TAM_SEQ_NUM=42 \
  TOLL_LANE_OVERRIDE=99 \
  /var/www/plugins/TumInjector/TumInjector
```

*(Lane override requires modifying `TumInjector.cpp` `tum.set_laneID()` value and rebuilding.)*

---

## Blocker note

TumInjector requires a V2X-Hub plugin registration to connect to tmxcore.
It cannot inject messages via raw socket without speaking the full IVP framing
protocol (length-prefixed JSON). The C++ PluginClient approach is the smallest
viable injector for this environment.

/**
 * TumInjector — development-only V2X-Hub plugin that injects one synthetic
 * TumMessage onto the TMX bus, then exits cleanly.
 *
 * Purpose: prove the TAM → TUM → TumAck → transaction path without VISSIM.
 *
 * Usage (inside the devcontainer):
 *   1. Ensure tmxcore is running and TollPlugin is registered.
 *   2. Note the current TAM msgCount from the TollPlugin log (used as tamSequenceNum).
 *   3. Build this tool: cmake --build /workspace/src/build --target TumInjector
 *   4. Register it in the database (see README.md).
 *   5. Run: /var/www/plugins/TumInjector/TumInjector --tamSeq=<N>
 *
 * The injector:
 *   - Connects to tmxcore on 127.0.0.1:24601
 *   - Waits for IvpPluginState_registered
 *   - Publishes one TumMessage (type=J3217, subtype=TUM)
 *   - Exits after 500 ms (allowing the message to route)
 */

#include <PluginClientClockAware.h>
#include <unistd.h>
#include <cstdlib>
#include <string>

#include "../../include/TumMessage.hpp"
#include "../../include/J3217Codec.h"

using namespace tmx::utils;

namespace v2x { namespace toll { namespace tools {

class TumInjector : public PluginClientClockAware {
public:
    explicit TumInjector(const std::string& name)
        : PluginClientClockAware(name) {}

    int Main() override {
        // All fields configurable via environment variables.
        // Defaults match the active camden-nj-001 TollPlugin config.
        auto env = [](const char* key, const char* def) -> std::string {
            const char* v = std::getenv(key); return v ? v : def;
        };

        int    tamSeq       = std::atoi(env("TAM_SEQ_NUM",   "1").c_str());
        int    tumSeqBase   = std::atoi(env("TUM_SEQ_NUM",   "1").c_str());
        int    laneID       = std::atoi(env("LANE_ID",        "3").c_str());
        int    tumCount     = std::atoi(env("TUM_COUNT",          "1").c_str());
        // TUM_SEND_DELAY_MS: inter-TUM gap in ms. Default 20ms ≈ 50 TUM/s attempt.
        // Actual throughput is ~37 TUM/s due to IVP socket latency (see crash report).
        // Set to 50ms for conservative load testing, 5ms for max-throughput stress.
        int    sendDelayMs  = std::atoi(env("TUM_SEND_DELAY_MS", "20").c_str());
        if (sendDelayMs < 1) sendDelayMs = 1;   // floor: 1ms (1000 TUM/s attempt)
        std::string tempIDBase = env("TEMP_ID",        "ABCD1234");
        std::string vclass  = env("VEHICLE_CLASS",  "passenger_car");
        std::string tpID    = env("TOLL_POINT_ID",  "camden-nj-001");
        std::string tcID    = env("TOLL_CHARGER_ID","camden-rse-01");
        std::string encMode = env("MESSAGE_ENCODING_MODE", "JSON");

        PLOG(logINFO) << "TumInjector: count=" << tumCount
                      << " sendDelay=" << sendDelayMs << "ms"
                      << " tamSeq=" << tamSeq
                      << " mode=" << encMode;

        while (_plugin->state != IvpPluginState_registered &&
               _plugin->state != IvpPluginState_connected) {
            usleep(50000);
        }

        for (int i = 0; i < tumCount; ++i) {
            // Each TUM uses a unique tumSequenceNum and tempID to avoid
            // duplicate detection. tempID is 8 chars: base 4 chars + 4-digit index.
            int tumSeq = tumSeqBase + i;
            char tidBuf[9];
            snprintf(tidBuf, sizeof(tidBuf), "%.4s%04d", tempIDBase.c_str(), i % 10000);
            std::string tempID(tidBuf);

            TumMessage tum;
            tum.set_msgType("TUM");
            tum.set_tumSequenceNum(tumSeq % 65536);
            tum.set_tamSequenceNum(tamSeq);
            tum.set_tempID(tempID);
            tum.set_vehicleClass(vclass);
            tum.set_vehicleType(610);
            tum.set_tollPointID(tpID);
            tum.set_tollChargerID(tcID);
            tum.set_laneID(laneID);
            tum.set_eventTimeUtc("2026-06-03T01:00:00Z");
            tum.set_simTimeSec(static_cast<double>(i));
            tum.set_speedMps(15.0);
            tum.set_latDeg(39.910664);
            tum.set_lonDeg(-75.031281);

            if (encMode == "J3217_UPER") {
                auto bytes = J3217Codec::encodeTum(tum);
                if (bytes.empty()) continue;
                std::string hex = J3217Codec::toHex(bytes);
                tmx::routeable_message routeMsg;
                routeMsg.initialize(TumMessage::MessageType, TumMessage::MessageSubType, "", 0, IvpMsgFlags_None);
                routeMsg.set_payload(hex);
                routeMsg.set_encoding("asn.1-uper/hexstring");
                BroadcastMessage(routeMsg);
            } else {
                BroadcastMessage(tum);
            }
            usleep(static_cast<useconds_t>(sendDelayMs) * 1000);
        }

        // Allow last message to route
        usleep(500000);
        PLOG(logINFO) << "TumInjector done: sent " << tumCount << " TUMs";
        return 0;
    }
};

}}} // namespace v2x::toll::tools

int main(int argc, char** argv) {
    return run_plugin<v2x::toll::tools::TumInjector>("TumInjector", argc, argv);
}

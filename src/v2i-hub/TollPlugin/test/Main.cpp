/**
 * Main.cpp — Google Test entry point for TollPlugin unit tests.
 *
 * TO VERIFY: GTest package name in Ubuntu V2X-Hub CMake environment.
 * Typical find_package: find_package(GTest REQUIRED)
 */
#include <PluginLog.h>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
#ifndef NO_EVENTLOG_UDP
    tmx::utils::Output2Eventlog::Enable() = false;
#endif
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

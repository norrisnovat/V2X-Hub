##############################################################################
# TollPlugin CMakeLists.txt
#
# V2X-Hub Programming Guide pattern:
#   PROJECT(TollPlugin VERSION 3.0.0 LANGUAGES CXX)
#   SET(TMX_PLUGIN_NAME "Toll Plugin")
#   BuildTmxPlugin()
#   TARGET_LINK_LIBRARIES(${PROJECT_NAME} tmxutils)
#
# BuildTmxPlugin() macro is provided by the V2X-Hub TMX cmake infrastructure.
# It handles executable creation, include paths, and V2X-Hub linking.
#
# TO VERIFY on Ubuntu:
#   - tmx-plugin.cmake location after V2X-Hub build/install
#   - GTest package name for test target
#   - OpenSSL needed in Phase 3 for SHA-256 and ECDSA signing
##############################################################################

cmake_minimum_required(VERSION 3.16)

PROJECT ( TollPlugin VERSION 3.0.0 LANGUAGES CXX )

SET ( CMAKE_CXX_STANDARD 17 )
SET ( CMAKE_CXX_STANDARD_REQUIRED ON )

# V2X-Hub TMX plugin name — appears in Admin Portal and command line.
SET ( TMX_PLUGIN_NAME "Toll Plugin" )

# BuildTmxPlugin() macro — sets up the plugin executable, include dirs,
# and V2X-Hub linking. Provided by the TMX cmake infrastructure after
# running: cd tmx && cmake . && make && sudo make install
BuildTmxPlugin()

# Additional libraries beyond the base V2X-Hub plugin runtime.
TARGET_LINK_LIBRARIES ( ${PROJECT_NAME} tmxutils )

# Phase 3: add security library when SCMS integration is enabled.
# TARGET_LINK_LIBRARIES ( ${PROJECT_NAME} tmxutils scms-client ssl crypto )

# J3217 codec compiled in isolation — lives in j3217/ (NOT src/) so BuildTmxPlugin()
# glob of src/*.c* never touches these files, avoiding J2735 header version conflicts.
file ( GLOB J3217_SOURCES "j3217/*.c" )
list ( REMOVE_ITEM J3217_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/j3217/converter-sample.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/j3217/pdu_collection.c"
)
add_library ( j3217_codec STATIC ${J3217_SOURCES} j3217/J3217CodecImpl.c )
target_include_directories ( j3217_codec PRIVATE j3217 )
# NO link with tmxutils — must stay isolated from J2735 headers

target_sources ( ${PROJECT_NAME} PRIVATE
    src/J3217Codec.cpp
    src/UdpTollInterface.cpp
)
target_link_libraries ( ${PROJECT_NAME} j3217_codec pthread )

# Include paths. src/j3217 is NOT listed here — those headers are only for
# the isolated j3217_codec static library (different asn1c version from J2735).
TARGET_INCLUDE_DIRECTORIES ( ${PROJECT_NAME} PRIVATE
    include
)

##############################################################################
# Unit tests — compiled separately, do not require V2X-Hub runtime.
##############################################################################

# TO VERIFY: GTest package available on Ubuntu V2X-Hub build environment.
find_package ( GTest QUIET )

if ( GTest_FOUND OR GTEST_FOUND )
    enable_testing()

    add_executable ( TollPluginTests
        test/Main.cpp
        test/TollPluginTest.cpp
        test/TumValidationTest.cpp
        test/TumAckTest.cpp
        test/TollTransactionStoreTest.cpp
        test/J3217UperTest.cpp
        test/UdpTollInterfaceTest.cpp
        src/PluginConfig.cpp
        src/TollTransactionStore.cpp
        src/J3217Codec.cpp
        src/UdpTollInterface.cpp
    )

    TARGET_INCLUDE_DIRECTORIES ( TollPluginTests PRIVATE include )

    # GTest::GTest only — Main.cpp provides main(), so GTest::Main (gtest_main)
    # would cause a duplicate-main linker error.
    TARGET_LINK_LIBRARIES ( TollPluginTests
        GTest::GTest
        tmxutils
        j3217_codec
        pthread
    )

    add_test ( NAME TollPluginTests COMMAND TollPluginTests )
else()
    message ( WARNING "GTest not found — TollPlugin unit tests will not be built. "
                      "Install: sudo apt-get install libgtest-dev" )
endif()

##############################################################################
# Development tools — only built inside the devcontainer.
##############################################################################

add_subdirectory( tools/tum_injector )

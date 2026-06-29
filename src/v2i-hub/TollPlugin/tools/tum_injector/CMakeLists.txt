cmake_minimum_required(VERSION 3.16)

PROJECT(TumInjector VERSION 1.0.0 LANGUAGES CXX)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(tmx QUIET CONFIG)

add_executable(TumInjector TumInjector.cpp ../../src/J3217Codec.cpp)
SET_TARGET_PROPERTIES(TumInjector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

TARGET_INCLUDE_DIRECTORIES(TumInjector PRIVATE
    ../../include
)

TARGET_LINK_LIBRARIES(TumInjector PRIVATE tmxutils j3217_codec)

INSTALL(TARGETS TumInjector
    RUNTIME DESTINATION /var/www/plugins/TumInjector
)
INSTALL(FILES manifest.json
    DESTINATION /var/www/plugins/TumInjector
)

# Standalone, host-native test build (runs on your dev machine, not the MCU).
# Deliberately separate from the top-level CMakeLists.txt, which cross-compiles
# the firmware for arm-none-eabi via cmake/gcc-arm-none-eabi.cmake — mixing the
# two toolchains in one CMake project would break both.
cmake_minimum_required(VERSION 3.20)
project(EcohiveTests C)

enable_testing()

set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Unity/src)

add_executable(test_Math
  ${CMAKE_CURRENT_SOURCE_DIR}/test_Math.c
  ${UNITY_DIR}/unity.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../Core/Src/Math.c
)

target_include_directories(test_Math PRIVATE
  ${UNITY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../Core/Inc
)

target_link_libraries(test_Math PRIVATE m)

add_test(NAME Math COMMAND test_Math)

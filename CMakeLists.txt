cmake_minimum_required(VERSION 3.22.1)
project(filament_sdl3_test LANGUAGES CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "This starter currently targets Android only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED FILAMENT_SDK OR FILAMENT_SDK STREQUAL "")
    message(FATAL_ERROR "Set FILAMENT_SDK to the unpacked official Filament Android native distribution.")
endif()

if(NOT EXISTS "${FILAMENT_SDK}/include/filament/Engine.h")
    message(FATAL_ERROR "Invalid FILAMENT_SDK: missing include/filament/Engine.h")
endif()

set(FILAMENT_LIB_DIR "${FILAMENT_SDK}/lib/${ANDROID_ABI}")
if(NOT EXISTS "${FILAMENT_LIB_DIR}/libfilament.a")
    message(FATAL_ERROR "Invalid FILAMENT_SDK: missing ${FILAMENT_LIB_DIR}/libfilament.a")
endif()

# Official SDL3 Android AAR provides native libSDL3.so and headers via Prefab.
find_package(SDL3 REQUIRED CONFIG)

add_library(main SHARED src/main.cpp src/Forest.cpp src/Geometry.cpp src/FpsHud.cpp)
target_include_directories(main PRIVATE "${FILAMENT_SDK}/include")
target_compile_options(main PRIVATE -Wall -Wextra)

# Filament is distributed as a set of static libraries with circular references.
# Link as a single group, without rebuilding Filament. Dead stripping will discard
# archives / objects that the minimal test scene does not reference.
file(GLOB FILAMENT_STATIC_LIBS CONFIGURE_DEPENDS "${FILAMENT_LIB_DIR}/*.a")
if(NOT FILAMENT_STATIC_LIBS)
    message(FATAL_ERROR "No Filament static archives in ${FILAMENT_LIB_DIR}")
endif()
target_link_libraries(main PRIVATE
    SDL3::SDL3
    "-Wl,--start-group"
    ${FILAMENT_STATIC_LIBS}
    "-Wl,--end-group"
    android log EGL GLESv3 z dl
)
target_link_options(main PRIVATE "-Wl,--gc-sections")

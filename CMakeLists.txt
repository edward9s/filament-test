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

# Both backends must be present: do not silently ship an OpenGL-only build.
if(NOT EXISTS "${FILAMENT_LIB_DIR}/libbluevk.a")
    message(FATAL_ERROR "Filament SDK has no Vulkan loader library: ${FILAMENT_LIB_DIR}/libbluevk.a")
endif()

# Require our repository-prebuilt static SDK, never substitute a dynamic system SDL3.
if(NOT DEFINED SDL3_SDK OR SDL3_SDK STREQUAL "")
    message(FATAL_ERROR "Set SDL3_SDK to the unpacked SDL3 static SDK.")
endif()
if(NOT EXISTS "${SDL3_SDK}/lib/libSDL3.a" OR
   NOT EXISTS "${SDL3_SDK}/include/SDL3/SDL.h" OR
   NOT EXISTS "${SDL3_SDK}/lib/cmake/SDL3/SDL3Config.cmake")
    message(FATAL_ERROR "Invalid SDL3_SDK: missing static archive, headers or CMake config.")
endif()
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-static
    PATHS "${SDL3_SDK}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

add_library(main SHARED src/main.cpp src/Forest.cpp src/Geometry.cpp src/FpsHud.cpp src/Effects.cpp src/EffectsGeometry.cpp)
target_include_directories(main PRIVATE "${FILAMENT_SDK}/include")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/headers/include/bluevk/BlueVK.h")
    message(FATAL_ERROR "Missing pinned Filament platform headers. Run scripts/download-deps.sh.")
endif()
target_include_directories(main SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.deps/headers/include")
# The prebuilt Filament platform classes have no RTTI; derived platforms must match.
target_compile_options(main PRIVATE -Wall -Wextra -fno-rtti)

# Filament is distributed as a set of static libraries with circular references.
# Link as a single group, without rebuilding Filament. Dead stripping will discard
# archives / objects that the minimal test scene does not reference.
file(GLOB FILAMENT_STATIC_LIBS CONFIGURE_DEPENDS "${FILAMENT_LIB_DIR}/*.a")
if(NOT FILAMENT_STATIC_LIBS)
    message(FATAL_ERROR "No Filament static archives in ${FILAMENT_LIB_DIR}")
endif()
target_link_libraries(main PRIVATE
    SDL3::SDL3-static
    "-Wl,--start-group"
    ${FILAMENT_STATIC_LIBS}
    "-Wl,--end-group"
    android log EGL GLESv3 z dl
)
# Required to pull SDL3's JNI_OnLoad out of libSDL3.a and register Android JNI methods.
target_link_options(main PRIVATE "-Wl,-u,JNI_OnLoad" "-Wl,--gc-sections")

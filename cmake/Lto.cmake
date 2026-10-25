include_guard(GLOBAL)
if(NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    return()
endif()
if(NOT ANDROID)
    message(FATAL_ERROR "This LTO configuration requires Android")
endif()

# The Android NDK uses LLVM's lld. Prevent CMake's IPO options from
# selecting the obsolete gold linker if the NDK version is misdetected.
set(CMAKE_CXX_LINK_OPTIONS_IPO "-fuse-ld=lld")

function(verify_android_lto)
    # CMake 3.22's CheckIPOSupported checks a whole project and loses the
    # configured Android ABI. Source-file try_compile preserves the NDK ABI.
    set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ForestLTO")
    file(MAKE_DIRECTORY "${probe_dir}")
    file(WRITE "${probe_dir}/library.cpp" "int lto_value() { return 7; }\n")
    file(WRITE "${probe_dir}/main.cpp"
        "extern int lto_value(); int main() { return lto_value() == 7 ? 0 : 1; }\n")

    # Require a real link, not just a successful ThinLTO compile to .o or .a.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    try_compile(supported "${probe_dir}/build"
        SOURCES "${probe_dir}/library.cpp" "${probe_dir}/main.cpp"
        CMAKE_FLAGS "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION:BOOL=ON"
        LINK_OPTIONS "-fuse-ld=lld"
        OUTPUT_VARIABLE output)
    if(NOT supported)
        message(FATAL_ERROR "LTO is required but unavailable: ${output}")
    endif()
endfunction()

verify_android_lto()
message(STATUS "LTO is required and supported")

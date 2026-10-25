include_guard(GLOBAL)
if(NOT ANDROID)
    message(FATAL_ERROR "Android ThinLTO configuration requires the Android NDK")
endif()

# CMake 3.22's Android IPO path appends -fuse-ld=gold after linker flags,
# overriding lld. Use explicit ThinLTO flags instead of CMake's IPO property.
function(verify_android_thinlto)
    set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ForestLTO")
    file(MAKE_DIRECTORY "${probe_dir}")
    file(WRITE "${probe_dir}/library.cpp" "int lto_value() { return 7; }\n")
    file(WRITE "${probe_dir}/main.cpp"
        "extern int lto_value(); int main() { return lto_value() == 7 ? 0 : 1; }\n")

    # Compile both translation units to LLVM bitcode and link an executable.
    # Source-file try_compile preserves the selected Android ABI.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    try_compile(supported "${probe_dir}/build"
        SOURCES "${probe_dir}/library.cpp" "${probe_dir}/main.cpp"
        COMPILE_DEFINITIONS "-flto=thin"
        LINK_OPTIONS "-flto=thin" "-fuse-ld=lld"
        OUTPUT_VARIABLE output)
    if(NOT supported)
        message(FATAL_ERROR "Android ThinLTO is required but unavailable: ${output}")
    endif()
endfunction()

verify_android_thinlto()

function(enable_android_thinlto target)
    target_compile_options(${target} PRIVATE -flto=thin)
    target_link_options(${target} PRIVATE -flto=thin -fuse-ld=lld)
endfunction()
message(STATUS "Android ThinLTO is required and supported (LLVM lld)")

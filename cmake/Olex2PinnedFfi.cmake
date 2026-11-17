# Real libffi, configured by its unmodified release script and built by CMake.
# All C/ASM inputs remain visible to compile_commands and source provenance.
function(add_olex2_pinned_ffi project_root)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS" OR NOT OHOS_ARCH STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This libffi integration is qualified only for OHOS ARM64")
  endif()
  if(NOT CMAKE_ASM_COMPILER_LOADED)
    message(FATAL_ERROR "libffi requires the toolchain's real preprocessed assembler")
  endif()
  set(ffi_source "${project_root}/vendor/libffi")
  set(ffi_config "${CMAKE_CURRENT_BINARY_DIR}/pinned-libffi-config")
  file(MAKE_DIRECTORY "${ffi_config}")
  execute_process(COMMAND sh "${ffi_source}/config.guess"
    OUTPUT_VARIABLE ffi_build OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ffi_guess_status)
  if(NOT ffi_guess_status EQUAL 0)
    message(FATAL_ERROR "Cannot determine the configure build machine")
  endif()
  # Upstream config.sub does not yet recognize 'ohos'. The musl/Linux host
  # alias selects AArch64/Linux feature rules; the actual compiler and linker
  # are still explicitly aarch64-linux-ohos with the installed OHOS sysroot.
  set(ffi_cc "${CMAKE_C_COMPILER} --target=${CMAKE_C_COMPILER_TARGET}")
  set(ffi_cxx "${CMAKE_CXX_COMPILER} --target=${CMAKE_CXX_COMPILER_TARGET}")
  string(TOUPPER "${CMAKE_BUILD_TYPE}" ffi_build_type)
  set(ffi_flags "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${ffi_build_type}} --sysroot=${CMAKE_SYSROOT} -fPIC")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "CC=${ffi_cc}" "CXX=${ffi_cxx}" "CCAS=${ffi_cc}"
    "AR=${CMAKE_AR}" "RANLIB=${CMAKE_RANLIB}"
    "CFLAGS=${ffi_flags}" "CCASFLAGS=${ffi_flags}"
    "CXXFLAGS=${CMAKE_CXX_FLAGS} --sysroot=${CMAKE_SYSROOT}"
    "LDFLAGS=${CMAKE_EXE_LINKER_FLAGS} --sysroot=${CMAKE_SYSROOT}"
    sh "${ffi_source}/configure" "--build=${ffi_build}"
    --host=aarch64-unknown-linux-musl --disable-shared --enable-static
    --with-pic --disable-docs
    WORKING_DIRECTORY "${ffi_config}" RESULT_VARIABLE ffi_config_status
    OUTPUT_FILE "${ffi_config}/configure.stdout.log"
    ERROR_FILE "${ffi_config}/configure.stderr.log")
  if(NOT ffi_config_status EQUAL 0)
    message(FATAL_ERROR "libffi configure failed; retained logs in ${ffi_config}")
  endif()
  file(READ "${ffi_config}/include/ffitarget.h" ffi_public_config)
  if(NOT ffi_public_config MATCHES "#define FFI_CLOSURES 1")
    message(FATAL_ERROR "libffi closures must not be disabled")
  endif()
  add_library(olex2_ffi STATIC
    "${ffi_source}/src/prep_cif.c" "${ffi_source}/src/types.c"
    "${ffi_source}/src/raw_api.c" "${ffi_source}/src/java_raw_api.c"
    "${ffi_source}/src/closures.c" "${ffi_source}/src/tramp.c"
    "${ffi_source}/src/aarch64/ffi.c" "${ffi_source}/src/aarch64/sysv.S")
  set_target_properties(olex2_ffi PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(olex2_ffi PUBLIC "${ffi_config}/include"
    PRIVATE "${ffi_config}" "${ffi_source}/include" "${ffi_source}/src")
  target_compile_definitions(olex2_ffi PRIVATE FFI_BUILDING)
  target_compile_definitions(olex2_ffi INTERFACE _LIBFFI OLEX2_STATIC_LIBFFI=1)
  target_link_libraries(olex2_ffi PUBLIC dl)
  file(GLOB_RECURSE ffi_config_inputs CONFIGURE_DEPENDS "${ffi_source}/*")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ffi_config_inputs})
endfunction()

# Separate original main programs, never additional members of the HAP runtime.
# Call only when the opt-in option is enabled. No install() or add_test(): these
# upstream entries execute application macros / mixed scientific tests by default.
function(add_olex2_console_tools project_root core_target python_target)
  foreach(required_target IN ITEMS ${core_target} ${python_target})
    if(NOT TARGET ${required_target})
      message(FATAL_ERROR "Console tools require source-built ${required_target}")
    endif()
  endforeach()
  set(source_directory "${project_root}/upstream")
  add_executable(olex2_example_cchnp EXCLUDE_FROM_ALL
    "${source_directory}/examples/cchnp/main.cpp")
  add_executable(olex2_example_io EXCLUDE_FROM_ALL
    "${source_directory}/examples/io/example.cpp")
  add_executable(olex2_upstream_tests EXCLUDE_FROM_ALL
    "${source_directory}/tests/tests/tests.cpp"
    "${source_directory}/repository/shellutil.cpp")
  # The original updater/launcher has its own main and real native dependencies.
  add_executable(olex2_unirun EXCLUDE_FROM_ALL
    "${source_directory}/unirun/main.cpp"
    "${source_directory}/repository/cdsfs.cpp"
    "${source_directory}/repository/url.cpp"
    "${source_directory}/repository/httpfs.cpp"
    "${source_directory}/repository/updateapi.cpp"
    "${source_directory}/repository/patchapi.cpp"
    "${source_directory}/repository/shellutil.cpp")
  target_compile_definitions(olex2_unirun PRIVATE OLEX2_NATIVE_LAUNCHER=1)
  include("${project_root}/cmake/Olex2NativeFtp.cmake")
  add_olex2_native_ftp(olex2_unirun "${project_root}")
  foreach(console_target IN ITEMS
      olex2_example_cchnp olex2_example_io olex2_upstream_tests olex2_unirun)
    target_compile_features(${console_target} PRIVATE cxx_std_17)
    target_link_libraries(${console_target} PRIVATE
      ${core_target} ${python_target})
    target_link_options(${console_target} PRIVATE -Wl,--no-undefined)
    set_target_properties(${console_target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/console-tools")
  endforeach()
  add_custom_target(olex2_console_tools DEPENDS
    olex2_example_cchnp olex2_example_io olex2_upstream_tests olex2_unirun)
endfunction()

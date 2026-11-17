# Genuine non-GUI core, shared by host/target checks and the diagnostic HAP.
# Registration/empty startup do not qualify the scientific command behavior.
function(add_olex2_core_library target utility_target source_directory)
  if(NOT TARGET ${utility_target})
    message(FATAL_ERROR "Original utility target must exist before the core")
  endif()
  file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
    "${source_directory}/xlib/*.cpp"
    "${source_directory}/xlib/absorpc/*.cpp"
    "${source_directory}/xlib/henke/*.cpp"
    "${source_directory}/xlib/macro/*.cpp")
  add_library(${target} STATIC ${CORE_SOURCES}
    "${source_directory}/repository/fsext.cpp"
    "${source_directory}/repository/hkl_util.cpp"
    "${source_directory}/repository/refinement_listener.cpp")
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(${target} PUBLIC
    "${source_directory}/xlib" "${source_directory}/xlib/absorpc"
    "${source_directory}/repository")
  target_link_libraries(${target} PUBLIC ${utility_target})
endfunction()

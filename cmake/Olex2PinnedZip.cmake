# Real read-only ZIP support, built from the unmodified locked zlib tree.
# zlibstatic is supplied by Olex2PinnedPng; never add the same vendor twice.
function(add_olex2_pinned_zip core_target project_root)
  if(NOT TARGET ${core_target} OR NOT TARGET zlibstatic)
    message(FATAL_ERROR "ZIP backend requires the source core and pinned zlibstatic")
  endif()
  set(minizip_directory "${project_root}/vendor/zlib/contrib/minizip")
  add_library(olex2_minizip STATIC
    "${minizip_directory}/unzip.c" "${minizip_directory}/ioapi.c")
  set_target_properties(olex2_minizip PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(olex2_minizip PUBLIC "${minizip_directory}")
  target_compile_definitions(olex2_minizip PRIVATE _FILE_OFFSET_BITS=64)
  target_link_libraries(olex2_minizip PUBLIC zlibstatic)
  target_sources(${core_target} PRIVATE
    "${project_root}/upstream/repository/wxzipfs.cpp"
    "${project_root}/upstream/repository/filesystem.cpp")
  # fsext's class layout must agree in all consumers of the same source core.
  target_compile_definitions(${core_target} PUBLIC OLEX2_MINIZIP=1)
  target_link_libraries(${core_target} PUBLIC olex2_minizip)
endfunction()

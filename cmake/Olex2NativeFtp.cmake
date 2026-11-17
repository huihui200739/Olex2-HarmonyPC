# Actual native transport shared by the application updater and optional launcher.
function(add_olex2_native_ftp target project_root)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "A real consumer target is required for native FTP")
  endif()
  target_sources(${target} PRIVATE "${project_root}/native/native_ftp.cpp")
  target_compile_definitions(${target} PRIVATE OLEX2_NATIVE_FTP=1)
  target_include_directories(${target} PRIVATE "${project_root}/native")
endfunction()

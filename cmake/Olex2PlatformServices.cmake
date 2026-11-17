# Real upstream implementations; no fake wx headers or disabled-feature objects.
# These are private link inputs of the existing runtime, not extra main programs.
function(add_olex2_platform_services target source_directory)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Original graphics runtime must exist before platform services")
  endif()
  target_sources(${target} PRIVATE
    "${source_directory}/repository/cdsfs.cpp"
    "${source_directory}/repository/eprocess.cpp"
    "${source_directory}/repository/hkl_py.cpp"
    "${source_directory}/repository/httpfs.cpp"
    "${source_directory}/repository/patchapi.cpp"
    "${source_directory}/repository/py_core.cpp"
    "${source_directory}/repository/shellutil.cpp"
    "${source_directory}/repository/updateapi.cpp"
    "${source_directory}/repository/url.cpp"
    "${source_directory}/olex/imagep.cpp"
    "${source_directory}/olex/langdict.cpp"
    "${source_directory}/olex/html/htmlswitch.cpp"
    "${source_directory}/olex/html/htmlprep.cpp"
    "${source_directory}/olex/html/htmlext.cpp"
    "${source_directory}/olex/html/htmlmanager.cpp"
    "${source_directory}/olex/html/htmltags.cpp")
  target_include_directories(${target} PRIVATE
    "${source_directory}/olex" "${source_directory}/olex/html" "${source_directory}/native")
endfunction()

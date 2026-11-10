function(add_olex2_utility_library target source_directory)
  file(GLOB SDL_SOURCES CONFIGURE_DEPENDS
    "${source_directory}/sdl/*.cpp" "${source_directory}/sdl/smart/*.cpp"
    "${source_directory}/sdl/exparse/*.cpp" "${source_directory}/sdl/math/*.cpp")
  add_library(${target} STATIC ${SDL_SOURCES})
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(${target} PUBLIC "${source_directory}/sdl" "${source_directory}/sdl/math")
  target_compile_definitions(${target} PUBLIC _UNICODE)
  if(APPLE)
    target_compile_definitions(${target} PUBLIC __MAC__)
  endif()
  find_package(Threads REQUIRED)
  target_link_libraries(${target} PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endfunction()

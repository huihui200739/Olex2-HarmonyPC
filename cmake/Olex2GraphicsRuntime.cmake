# One genuine original runtime/glib DSO, using fixed unmodified GL4ES/GLU sources.
set(GL4ES_ROOT "${OLEX2_ROOT}/vendor/gl4es")
set(GLU_ROOT "${OLEX2_ROOT}/vendor/glu")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(NOX11 ON CACHE BOOL "" FORCE)
set(NOEGL ON CACHE BOOL "" FORCE)
set(NO_LOADER ON CACHE BOOL "" FORCE)
set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
set(STATICLIB ON CACHE BOOL "" FORCE)
set(DEFAULT_ES 2 CACHE STRING "" FORCE)
add_subdirectory("${GL4ES_ROOT}" gl4es)
target_compile_definitions(GL PRIVATE GL4ES_COMPILE_FOR_USE_IN_SHARED_LIB)
include("${OLEX2_ROOT}/cmake/GLUSources.cmake")
add_library(legacy_glu STATIC ${GLU_SOURCES})
# Original GLU uses the pre-C++17 register grammar; isolate its language level.
set_target_properties(legacy_glu PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(legacy_glu PRIVATE LIBRARYBUILD)
target_include_directories(legacy_glu PRIVATE "${GLU_ROOT}/include" "${GLU_ROOT}/src/include" "${GLU_ROOT}/src/libnurbs/internals" "${GLU_ROOT}/src/libnurbs/interface" "${GLU_ROOT}/src/libnurbs/nurbtess" "${GL4ES_ROOT}/include")
include("${OLEX2_ROOT}/cmake/Olex2Utility.cmake")
add_olex2_utility_library(legacy_sdl "${OLEX2_ROOT}/upstream")
include("${OLEX2_ROOT}/cmake/Olex2Core.cmake")
add_olex2_core_library(legacy_core legacy_sdl "${OLEX2_ROOT}/upstream")
file(GLOB GLIB_SOURCES CONFIGURE_DEPENDS "${OLEX2_ROOT}/upstream/glib/*.cpp")
add_library(olex2core_graphics SHARED "${OLEX2_ROOT}/native/original_renderer_bridge.cpp" ${GLIB_SOURCES}
 "${OLEX2_ROOT}/native/application_runtime.cpp" "${OLEX2_ROOT}/native/original_log.cpp"
 "${OLEX2_ROOT}/native/original_interpreter.cpp" "${OLEX2_ROOT}/native/desktop_commands.cpp" "${OLEX2_ROOT}/upstream/olex/mainform.cpp" "${OLEX2_ROOT}/upstream/olex/main_gui.cpp" "${OLEX2_ROOT}/upstream/olex/xpprocs.cpp")
target_include_directories(olex2core_graphics PRIVATE "${GLU_ROOT}/include" "${GL4ES_ROOT}/include" "${OLEX2_ROOT}/upstream/glib" "${OLEX2_ROOT}/native")
target_link_libraries(olex2core_graphics PRIVATE legacy_core legacy_glu GL m dl)
set(OLEX2_EXPORT_SCRIPT "${OLEX2_ROOT}/cmake/original_renderer.exports" CACHE FILEPATH "Private shared-runtime export list")
set_property(TARGET olex2core_graphics APPEND PROPERTY LINK_DEPENDS "${OLEX2_EXPORT_SCRIPT}")
target_link_options(olex2core_graphics PRIVATE -Wl,--no-undefined -Wl,-Bsymbolic-functions "-Wl,--version-script=${OLEX2_EXPORT_SCRIPT}")

# Keep all generated libraries in the binary tree, never the source snapshot.
set_target_properties(GL PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vendor")

# Source-built interpreter in the same original runtime DSO.
include("${OLEX2_ROOT}/cmake/Olex2PythonRuntime.cmake")
target_compile_definitions(legacy_sdl PUBLIC _PYTHON)
target_include_directories(legacy_sdl PUBLIC "${OLEX2_ROOT}/vendor/cpython/Include" "${OLEX2_ROOT}/native/python-build")
target_sources(legacy_core PRIVATE "${OLEX2_ROOT}/upstream/repository/pyext.cpp" "${OLEX2_ROOT}/upstream/repository/olxvar.cpp")
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/python_runtime.cpp" "${OLEX2_ROOT}/upstream/repository/py_reg.cpp")
target_link_libraries(olex2core_graphics PRIVATE "-Wl,--whole-archive" olex2_python "-Wl,--no-whole-archive" ohpreferences bundle_ndk.z)

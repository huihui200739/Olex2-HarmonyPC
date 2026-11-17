cmake_minimum_required(VERSION 3.24)
project(olex2_harmonypc_app LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(OLEX2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
include("${OLEX2_ROOT}/cmake/Olex2GraphicsRuntime.cmake")
add_library(olex2native SHARED napi_init.cpp renderer.cpp application_view.cpp shared_original_probe.cpp polygon_probe.cpp "${OLEX2_ROOT}/native/original_matrices.cpp" "${OLEX2_ROOT}/native/lit_row_reference.cpp" "${OLEX2_ROOT}/native/material_commands.cpp" "${OLEX2_ROOT}/native/upstream_material.cpp" "${OLEX2_ROOT}/native/upstream_color.cpp" "${OLEX2_ROOT}/native/utility_checks.cpp" "${OLEX2_ROOT}/native/upstream_mesh.cpp" "${OLEX2_ROOT}/native/primitive_batch.cpp" "${OLEX2_ROOT}/native/primitive_triangles.cpp" "${OLEX2_ROOT}/native/primitive_lines.cpp" "${OLEX2_ROOT}/native/primitive_line_paths.cpp" line_path_probe.cpp "${OLEX2_ROOT}/native/primitive_polygons.cpp")
target_include_directories(olex2native PRIVATE "${OLEX2_ROOT}/native")
target_include_directories(olex2native PRIVATE "${OLEX2_ROOT}/upstream/glib")
target_link_libraries(olex2native PRIVATE olex2core_graphics ace_napi.z ace_ndk.z hilog_ndk.z EGL GLESv3 native_window dl)
target_link_options(olex2native PRIVATE -Wl,--no-undefined)
target_compile_definitions(olex2native PRIVATE OLEX2_MATERIAL_VALUES_ONLY)

target_include_directories(olex2native PRIVATE "${OLEX2_ROOT}/upstream/sdl" "${OLEX2_ROOT}/upstream/sdl/math" "${OLEX2_ROOT}/upstream/xlib" "${OLEX2_ROOT}/upstream/xlib/absorpc" "${OLEX2_ROOT}/upstream/repository")
target_compile_definitions(olex2native PRIVATE _UNICODE)

# Candidate-only genuine graphics application layer, not a precompiled input.
file(GLOB GX_APPLICATION_SOURCES CONFIGURE_DEPENDS "${OLEX2_ROOT}/upstream/gxlib/*.cpp")
list(REMOVE_ITEM GX_APPLICATION_SOURCES "${OLEX2_ROOT}/upstream/gxlib/wglscene.cpp" "${OLEX2_ROOT}/upstream/gxlib/wxglscene.cpp")
target_sources(olex2core_graphics PRIVATE ${GX_APPLICATION_SOURCES} "${OLEX2_ROOT}/upstream/repository/IsoSurface.cpp" "${OLEX2_ROOT}/native/gx_lifecycle.cpp")
target_include_directories(olex2core_graphics PRIVATE "${OLEX2_ROOT}/upstream/gxlib")
target_sources(olex2native PRIVATE "${OLEX2_ROOT}/native/image_decode_probe.cpp" "${OLEX2_ROOT}/native/gx_platform_evaluation.cpp")
target_link_libraries(olex2native PRIVATE image_source pixelmap)

# Native opaque-image adapter evaluated through the real TGXApp virtual seam.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/texture_image.cpp" "${OLEX2_ROOT}/native/ohos_texture_image.cpp" "${OLEX2_ROOT}/native/gx_texture_lifecycle.cpp")
target_link_libraries(olex2core_graphics PRIVATE image_source pixelmap)

# Candidate-only exact PNG sample decoder. ImageKit capability probes remain
# separate and unchanged; no test writer, synthetic mock GL or prebuilt decoder.
include("${OLEX2_ROOT}/cmake/Olex2PinnedPng.cmake")
add_olex2_pinned_png("${OLEX2_ROOT}")
include("${OLEX2_ROOT}/cmake/Olex2PinnedZip.cmake")
add_olex2_pinned_zip(legacy_core "${OLEX2_ROOT}")
include("${OLEX2_ROOT}/cmake/Olex2PinnedFfi.cmake")
add_olex2_pinned_ffi("${OLEX2_ROOT}")
# Publish the real FFI header and feature macros to every original-core consumer.
# PythonExt has FFI-dependent layout, so private per-source flags are unsafe.
target_link_libraries(legacy_sdl PUBLIC olex2_ffi)
target_sources(legacy_core PRIVATE "${OLEX2_ROOT}/upstream/repository/olxffi.cpp")
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/png_texture_decoder.c" "${OLEX2_ROOT}/native/png_texture_image.cpp")
target_link_libraries(olex2core_graphics PRIVATE png_static)

# Actual-window synthetic 1D texture sampling, no test-owned GL mocks.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/window_texture_1d_probe.cpp")
target_compile_definitions(olex2core_graphics PRIVATE OLEX2_HARMONY_GL4ES_TEXTURE_1D_READBACK=1)

# Persistent GX window implementation remains inside the existing core DSO.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/persistent_gx_window.cpp")

# Original renderer-owned resources, retained across actual window contexts.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/persistent_texture_window_probe.cpp")

# Genuine original named UI texture loading into the long-lived renderer.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/original_texture_load_probe.cpp")

# NativeDrawing cells are consumed by genuine TGlFont display lists.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/harmony_glyph_raster.cpp" "${OLEX2_ROOT}/native/original_font_cells.cpp" "${OLEX2_ROOT}/native/platform_font_factory.cpp" "${OLEX2_ROOT}/native/original_font_window_probe.cpp" "${OLEX2_ROOT}/native/original_vector_font_window_probe.cpp" "${OLEX2_ROOT}/native/olex_font_resource.cpp" "${OLEX2_ROOT}/native/original_resource_font_window_probe.cpp" "${OLEX2_ROOT}/native/resource_font_resample.cpp" "${OLEX2_ROOT}/native/resampled_resource_font_window_probe.cpp" "${OLEX2_ROOT}/native/system_font_catalog.cpp" "${OLEX2_ROOT}/native/family_font_window_probe.cpp")
set_source_files_properties("${OLEX2_ROOT}/native/resource_font_resample.cpp" PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
target_link_libraries(olex2core_graphics PRIVATE native_drawing)

# Fixed GL4ES bitmap batches must not erase ink under adjacent glyph padding.
target_compile_definitions(olex2core_graphics PRIVATE OLEX2_PLATFORM_RASTER_GLYPH_FLUSH=1)

# Original document platform view: same unique core owner and private ABI.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/original_document_view.cpp")

# Additional genuine upstream platform services; keep the strict link gate.
include("${OLEX2_ROOT}/cmake/Olex2PlatformServices.cmake")
add_olex2_platform_services(olex2core_graphics "${OLEX2_ROOT}/upstream")
include("${OLEX2_ROOT}/cmake/Olex2NativeFtp.cmake")
add_olex2_native_ftp(olex2core_graphics "${OLEX2_ROOT}")

# Optional source-built upstream main programs. Keep them outside the HAP and
# out of the default build; qualifying a link never runs these executables.
option(OLEX2_BUILD_UPSTREAM_CONSOLE_TOOLS
  "Enable separate upstream console targets (not run or packaged)" OFF)
if(OLEX2_BUILD_UPSTREAM_CONSOLE_TOOLS)
  include("${OLEX2_ROOT}/cmake/Olex2ConsoleTools.cmake")
  add_olex2_console_tools("${OLEX2_ROOT}" legacy_core olex2_python)
endif()

# Original controls, real ArkUI node backend, and the existing viewport zoom host.
include("${OLEX2_ROOT}/cmake/Olex2NativeWidgets.cmake")
add_olex2_native_widgets(olex2core_graphics "${OLEX2_ROOT}")
target_sources(olex2native PRIVATE native_viewport_zoom_napi.cpp)

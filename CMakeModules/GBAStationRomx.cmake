# GBAStation ROMX integration for the 3DS core.
#
# Keep the dependency, adapter sources, and target wiring out of Azahar's
# upstream-maintained CMake files. The root CMakeLists retains only the module
# include and one post-target setup call.

set(ROMX_BUILD_TESTS OFF CACHE BOOL "Disable embedded libromx tests" FORCE)
set(ROMX_BUILD_EXAMPLES OFF CACHE BOOL "Disable embedded libromx examples" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/libromx"
                 "${CMAKE_BINARY_DIR}/libromx" EXCLUDE_FROM_ALL)

function(gbastation_enable_romx)
    target_sources(citra_common PRIVATE
        "${CMAKE_SOURCE_DIR}/src/common/romx_io_file.cpp"
        "${CMAKE_SOURCE_DIR}/src/common/romx_io_file.h")
    target_include_directories(citra_common PUBLIC
        "${CMAKE_SOURCE_DIR}/third_party/libromx/include")
    target_link_libraries(citra_common PRIVATE ROMX::romx)
endfunction()

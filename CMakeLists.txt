cmake_minimum_required(VERSION 3.24)
project(future)

set(CMAKE_CXX_STANDARD 17)

set(IMGUI_DIR extern/imgui)
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)

file (GLOB future__imgui
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp
)


file (GLOB future__imgui_backend_win
extern/imgui/backends/imgui_impl_dx9.cpp
extern/imgui/backends/imgui_impl_win32.cpp
)

source_group("ImGui" REGULAR_EXPRESSION "extern/imgui/*")

file (GLOB future__main RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "main.cpp"
)

file (GLOB future__windows
        windows/*.cpp
)

file (GLOB future__windows_headers
        windows/*.h
)

include_directories(${future__windows_headers})
source_group("Windows" REGULAR_EXPRESSION "windows/")

file (GLOB future__utils
    utils/*.cpp
)

file (GLOB future__utils_headers
    utils/*.h
)

include_directories(${future__utils_headers})
source_group("Utils" REGULAR_EXPRESSION "utils/*")


set(ALL_FILES
    ${future__imgui}
    ${future__main}
    ${future__imgui_backend_win}
    ${future__windows}
    ${future__utils}
)

################################################################################
# VCPKG dependencies for Windows
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
include(CMake/automate-vcpkg.cmake)

if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
	set(VCPKG_TRIPLET x86-windows-static)
	set(VCPKG_TARGET_TRIPLET x86-windows-static)
elseif ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
	set(VCPKG_TRIPLET x64-windows-static)
	set(VCPKG_TARGET_TRIPLET x64-windows-static)
elseif ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "arm64")
    set(VCPKG_TRIPLET arm64-windows-static)
    set(VCPKG_TARGET_TRIPLET arm64-windows-static)
endif()

vcpkg_bootstrap()
vcpkg_install_packages(zlib bzip2 libzip)
endif()



set(LIBS d3d9 Dwmapi)
add_executable(future ${ALL_FILES})
#target_link_libraries(future ${LIBS})

find_package(libzip REQUIRED)
target_link_libraries(future PRIVATE libzip::zip)

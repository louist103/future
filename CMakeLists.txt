cmake_minimum_required(VERSION 3.24.0)
project(future)

set(CMAKE_CXX_STANDARD 20)

set(IMGUI_DIR extern/imgui)
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)

file (GLOB future__imgui
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    file (GLOB future__imgui_backend
    extern/imgui/backends/imgui_impl_dx11.cpp
    extern/imgui/backends/imgui_impl_win32.cpp)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file (GLOB future__imgui_backend
    extern/imgui/backends/imgui_impl_sdl2.cpp
    extern/imgui/backends/imgui_impl_opengl3.cpp)
endif()

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

include_directories(windows/)
source_group("Windows" REGULAR_EXPRESSION "windows/")

file (GLOB future__utils
    utils/*.cpp
)

file (GLOB future__utils_headers
    utils/*.h
)

include_directories(utils/)
source_group("Utils" REGULAR_EXPRESSION "utils/*")


set(ALL_FILES
    ${future__imgui}
    ${future__main}
    ${future__imgui_backend}
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


add_executable(future ${ALL_FILES})
#target_link_libraries(future ${LIBS})


include(FetchContent)
FetchContent_Declare(
    StormLib
    GIT_REPOSITORY https://github.com/ladislav-zezula/StormLib.git
    GIT_TAG v9.25
    PATCH_COMMAND ${stormlib_apply_path_if_needed}
)
FetchContent_MakeAvailable(StormLib)
list(APPEND ADDITIONAL_LIB_INCLUDES extern/StormLib/src)
include_directories(future, extern/StormLib/src)
target_link_libraries(future PUBLIC storm)


find_package(libzip REQUIRED)
target_link_libraries(future PRIVATE libzip::zip)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
find_package(SDL2 REQUIRED)
target_link_libraries(future PRIVATE SDL2::SDL2)
find_package(OpenGL)
target_link_libraries(future PRIVATE OpenGL::GL)
endif()

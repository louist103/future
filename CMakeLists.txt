cmake_minimum_required(VERSION 3.28)
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

set(ALL_FILES
    ${future__imgui}
    ${future__main}
    ${future__imgui_backend_win}
    ${future__windows}
)

set(LIBS d3d9 d3d11 Dwmapi)
add_executable(future ${ALL_FILES})
target_link_libraries(future ${LIBS})
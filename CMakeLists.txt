set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.16)
project(DisasterEvacPlanner CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(CMAKE_TLS_VERIFY OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "Raylib platform target" FORCE)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

add_executable(DisasterEvacPlanner
    src/main.cpp
    src/core/solvers.cpp
    src/gui/app.cpp
)

target_include_directories(DisasterEvacPlanner PRIVATE src)
target_link_libraries(DisasterEvacPlanner PRIVATE raylib)

if(EMSCRIPTEN)
    target_link_options(DisasterEvacPlanner PRIVATE
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets@assets"
    )
elseif(WIN32)
    target_link_libraries(DisasterEvacPlanner PRIVATE opengl32 gdi32 winmm)
endif()

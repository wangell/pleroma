cmake_minimum_required(VERSION 3.14)

project(pleroma VERSION 1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_BUILD_TYPE Debug)

find_package(Threads REQUIRED)

link_directories(/usr/local/lib/)
include_directories(/usr/local/include/)

add_compile_options (-fcolor-diagnostics)
add_compile_options (-Wshadow)

# Client
file(GLOB_RECURSE pleroma_SRC
    "pleroma_src/*.cpp"
    "pleroma_src/*.h"

    "shared_src/*.cpp"
    "shared_src/*.h"
    "shared_src/*.cc"
)

file(GLOB_RECURSE allo_SRC
    "allo_src/*.cpp"
    "allo_src/*.h"

    "shared_src/*.cpp"
    "shared_src/*.h"
    "shared_src/*.cc"
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")

# To make sure GLAD doesn't complain
add_compile_definitions(GLFW_INCLUDE_NONE)

add_executable(pleroma ${pleroma_SRC})
add_executable(allo ${allo_SRC})

# For SoLoud

target_link_libraries(allo pthread enet protobuf)
target_link_libraries(pleroma pthread enet protobuf)

target_compile_features(pleroma PUBLIC cxx_std_17)
target_compile_features(allo PUBLIC cxx_std_17)

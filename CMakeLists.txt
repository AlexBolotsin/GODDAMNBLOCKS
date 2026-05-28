cmake_minimum_required(VERSION 3.20)

message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")

message(STATUS "C Compiler ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CXX Compiler ID: ${CMAKE_CXX_COMPILER_ID}")

message(STATUS "CXX Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")

project(MazeGame)

set(CMAKE_CXX_STANDARD 17)

add_executable(MazeGame WIN32   # WIN32 = uses WinMain, not main()
    Source/WinMain.cpp
)

target_include_directories(MazeGame PRIVATE include)

target_link_libraries(MazeGame PRIVATE
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)
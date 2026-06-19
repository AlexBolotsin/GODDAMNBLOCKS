cmake_minimum_required(VERSION 3.20)

project(MazeGame LANGUAGES CXX)
 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ---------------------------------------------------------------
#  Sources
# ---------------------------------------------------------------
set(SOURCES
    Source/GrmWindowWrapper.cpp
    Source/WinMain.cpp
    Source/DX12Context.cpp
)
 
set(HEADERS
    Source/GrmWindowWrapper.h
    Source/DX12Context.h
)

# ---------------------------------------------------------------
#  Target  (WIN32 = WinMain entry point, no console window)
# ---------------------------------------------------------------
add_executable(MazeGame WIN32 ${SOURCES} ${HEADERS})
 
target_include_directories(MazeGame PRIVATE Source)

# ---------------------------------------------------------------
#  Windows / DX12 libraries
# ---------------------------------------------------------------
target_link_libraries(MazeGame PRIVATE
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)
 
# ---------------------------------------------------------------
#  Compiler flags
# ---------------------------------------------------------------
if (MSVC)
    target_compile_options(MazeGame PRIVATE
    
        /W4         # high warning level
        /WX         # warnings as errors
        /MP         # parallel compilation
        /permissive-
    )
 
    # Define Unicode and target Windows 10+
    target_compile_definitions(MazeGame PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00   # Windows 10
    )
endif()
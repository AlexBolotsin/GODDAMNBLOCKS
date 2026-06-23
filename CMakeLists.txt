cmake_minimum_required(VERSION 3.20)

project(MazeGame LANGUAGES CXX)

set(CMAKE_VS_PLATFORM_TOOLSET v143 CACHE STRING "Visual Studio platform toolset")
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ---------------------------------------------------------------
#  Sources
# ---------------------------------------------------------------
set(SOURCES
    Source/GrmWindowWrapper.cpp
    Source/WinMain.cpp
    Source/DX12Context.cpp
    Source/Transform.cpp
    Source/Mesh.cpp
    Source/Material.cpp
    Source/Entity.cpp
    Source/Scene.cpp
)
 
set(HEADERS
    Source/GrmWindowWrapper.h
    Source/DX12Context.h
    Source/EngineMath.h
    Source/Transform.h
    Source/Mesh.h
    Source/Material.h
    Source/Entity.h
    Source/Scene.h
)

# ---------------------------------------------------------------
#  Target  (WIN32 = WinMain entry point, no console window)
# ---------------------------------------------------------------
add_executable(MazeGame WIN32 ${SOURCES} ${HEADERS})

target_precompile_headers(MazeGame PRIVATE Source/pch.h)

target_include_directories(MazeGame PRIVATE Source)

# Special compiler settings for Material.cpp to handle d3dcompiler.h
set_source_files_properties(Source/Material.cpp PROPERTIES 
    COMPILE_FLAGS "/wd4530 /wd4577"
)

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
        /EHsc       # Enable C++ exceptions
        /fp:fast    # fast floating point
        /Zc:wchar_t
        /Zc:inline
        /Zc:forScope
        /MT         # Static link C runtime
    )
 
    # Define Unicode and target Windows 10+
    target_compile_definitions(MazeGame PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING
        _HAS_EXCEPTIONS=1
        VC_EXTRA_WARNINGS
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/pch.h")
    message(STATUS "PCH found: ${CMAKE_CURRENT_SOURCE_DIR}/Source/pch.h")
else()
    message(FATAL_ERROR "PCH not found at: ${CMAKE_CURRENT_SOURCE_DIR}/Source/pch.h")
endif()
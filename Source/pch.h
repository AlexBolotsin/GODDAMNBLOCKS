#pragma once

#pragma warning(push)
#pragma warning(disable: 4530 4577)

// cmath MUST be the absolute first include
#include <cmath>
#include <cstdlib>

// Windows headers after
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// DX12 after windows
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// STL
#include <string>
#include <vector>
#include <memory>

#pragma warning(pop)
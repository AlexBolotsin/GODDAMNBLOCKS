# Architecture Notes

## Big Picture
This project is a small Direct3D 12 forward-rendering engine prototype.
The runtime flow is:
1. OS window and message pump
2. DX12 context initialization
3. Scene setup (entities, meshes, material)
4. Per-frame update (animation, camera)
5. Frame rendering (main pass + projected shadow pass)

## Main Systems

### Window Layer
- File: Source/GrmWindowWrapper.*
- Responsibilities:
  - Create native window
  - Handle resize/minimize/destroy events
  - Drive message pump

### Render Context
- File: Source/DX12Context.*
- Responsibilities:
  - Device and swap chain creation
  - Command allocator/list lifecycle
  - RTV/DSV creation and binding
  - BeginFrame / RenderScene / EndFrame orchestration
  - Camera state storage and matrix construction

### Scene Graph (Simple)
- Files: Source/Scene.*, Source/Entity.*
- Responsibilities:
  - Own entities
  - Iterate and draw entities each frame

### Transform + Math
- Files: Source/Transform.*, Source/Math.h
- Responsibilities:
  - Position, rotation, scale
  - Build world matrices
  - Quaternion helpers

### Mesh
- Files: Source/Mesh.*
- Responsibilities:
  - Vertex/index GPU buffer upload
  - Input layout compatibility with shader
  - Draw call emission

### Material
- Files: Source/Material.*
- Responsibilities:
  - Root signature creation
  - Pipeline state creation
  - Runtime HLSL compile (VS/PS)
  - Shader-side lighting/fog/floor-detail logic

## Data Flow Summary
1. WinMain updates entity transforms and camera state.
2. DX12Context builds frame camera constants.
3. Entity uploads per-frame root constants (slot 0).
4. Entity uploads per-object root constants (slot 1).
5. Mesh submits geometry.
6. Pixel shader computes lighting and atmospheric blending.
7. Optional projected shadow pass redraws non-ground entities with shadow mode flag.

## Why This Design Works For Learning
- Minimal abstraction overhead
- Easy to reason about order of operations
- Explicit GPU state transitions and binding points
- Good stepping stone toward descriptor-driven, multi-pass architecture

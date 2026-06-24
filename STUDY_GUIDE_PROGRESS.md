# ProtoStudyGameEngine Study Guide

## Purpose
This document summarizes what has been implemented so far, why the changes were made, and what to study next to keep improving the renderer.

## Current Engine Snapshot
The project is currently a custom Direct3D 12 forward renderer with:
- Window creation and frame loop
- DX12 device, swap chain, command queue/list setup
- Depth buffer creation and binding
- Runtime HLSL compilation from C++ string source
- Shared root signature and PSO model
- Per-frame camera constants and per-object draw constants
- Animated cubes and a visible ground plane
- Directional, fill, ambient lighting with fog and sky blend
- Projected soft shadows using extra draw passes

## Core File Map
- Source/WinMain.cpp
  - Scene setup
  - Cube and ground creation
  - Animation and camera orbit
  - Main loop driving BeginFrame, RenderScene, EndFrame

- Source/DX12Context.cpp
  - Device and swap chain lifecycle
  - RTV and DSV setup
  - Frame begin/end transitions and clears
  - Camera matrix build helpers
  - Scene rendering and projected shadow pass

- Source/Material.cpp
  - Root signature creation
  - PSO creation
  - Embedded HLSL VS and PS
  - Main lighting, floor detail, fog, and shadow mode branch

- Source/Entity.cpp
  - Draw submission path
  - Root constants upload for frame and object data
  - Optional draw overrides for shadow pass

## Rendering Pipeline Today
1. BeginFrame
- Transition back buffer to render target
- Bind RTV and DSV
- Clear color and depth
- Set viewport, scissor, and topology

2. Main scene draw
- Build view and projection from camera state
- Upload per-frame constants (view, projection, camera position)
- For each entity, upload per-object constants (world, tint, render params)
- Draw mesh

3. Projected shadow pass
- Build directional projection onto floor plane
- Re-draw non-ground entities with shadow tint and shadow flag
- Use small per-pass offsets to soften and reduce z-fighting

4. EndFrame
- Transition back buffer to present
- Execute command list
- Present and advance fence/frame index

## Data Layout and Constant Buffers
Current shader constant blocks:
- b0 PerObject in shader naming, used as per-frame data in practice
  - viewMatrix
  - projMatrix
  - cameraPosition

- b1 PerDraw
  - worldMatrix
  - tintColor
  - renderParams

Important note:
- The naming in HLSL is historically inconsistent with usage (PerObject name stores per-frame values).
- Functionality works because register indices and struct layout match upload order.

## Major Milestones Reached
1. Build and project bring-up
- CMake configure/build stabilized in VS Code
- Executable runs through the custom window wrapper

2. Crash and render path stabilization
- Fixed invalid and null pipeline usage scenarios
- Added guard paths around draw setup

3. Camera and perspective correctness
- Added RH LookAt and perspective projection
- Corrected projection behavior and transform issues

4. Depth buffer integration
- Created D32 depth texture
- Bound depth target every frame
- Clear depth each frame and use standard depth test

5. Per-frame and per-object split
- Root constants split to avoid per-object camera duplication
- Entity draw path now only injects object-specific state plus frame state

6. Scene and visual context
- Ground plane entity added
- Animated cubes with color variation and motion
- Lighting extended to key plus fill plus ambient
- Atmospheric fog and sky gradient blending added

7. Shadow quality jump
- Lightweight projected soft shadows added
- Layered offsets used for penumbra-like softness
- Z-fighting mitigation added by slight plane and layer offsets

## Problems Solved During Iteration
- PCH confusion around generated cmake_pch source
- Math and quaternion issues in transform path
- Crash in D3D12 startup/draw sequence
- Invisible geometry from projection or winding mismatches
- Culling and front-face orientation inconsistencies
- Floor visibility and winding direction issues
- Shadow flicker from coplanar depth conflict
- Shader compile failures caused by symbol naming collisions

## Known Technical Debt
- Runtime shader source is embedded in C++ string, which is hard to maintain
- No separate shadow-map pass yet, only projected shadows
- Root constants are convenient but less scalable than CBV descriptor workflow
- HLSL cbuffer naming should be cleaned for clarity
- No post-processing pipeline yet (tone mapping, AA)

## What To Study Next (Recommended Order)
1. Color management and tone mapping
- Learn linear workflow and gamma-correct output
- Add filmic or ACES tone mapping

2. Real-time shadows
- Build depth-only light pass
- Sample shadow map in main pass with PCF
- Learn depth bias tuning and stability techniques

3. Material system expansion
- Move shader source to separate .hlsl files
- Add roughness and normal map support
- Move toward structured per-material data

4. Anti-aliasing and temporal stability
- Start with FXAA
- Then study TAA concepts for moving-camera stability

5. Renderer architecture
- Shared PSO cache by material features
- Descriptor heap management patterns
- Frame graph style pass organization

## Practical Reading Checklist
For each file, read in this order:

1. Source/WinMain.cpp
- Understand scene creation and animation
- Track how camera state is updated each frame

2. Source/DX12Context.cpp
- Follow init order and frame lifecycle
- Trace barriers and target bindings
- Study RenderScene flow including shadow pass

3. Source/Entity.cpp
- Understand root constant upload sizes and ordering
- Map object draw data to shader registers

4. Source/Material.cpp
- Verify root signature constants match C++ structs
- Follow VS transform chain and PS lighting pipeline
- Study fog and floor detail logic

## Validation Workflow Used In This Project
Typical safe loop:
1. Edit one subsystem at a time
2. Build with cmake --build ./build --config Debug
3. Launch and visually verify
4. If a shader issue appears, inspect D3DCompile failure text
5. Re-check root constant layout alignment whenever buffers change

## Suggested Immediate Cleanup Tasks
1. Rename HLSL cbuffers to match usage:
- b0 PerFrame
- b1 PerObject

2. Split shader string into external HLSL files
- Improve debugging and iteration speed

3. Add explicit in-app error popup on shader compile failure
- This avoids silent material init failures

4. Add a short docs folder structure
- docs/architecture.md
- docs/render-pipeline.md
- docs/shader-layout.md

## Quick Glossary
- PSO: Pipeline State Object
- Root signature: Resource/constant binding contract for shaders
- RTV: Render target view
- DSV: Depth stencil view
- NdotV: Dot product between normal and view direction
- PCF: Percentage closer filtering for soft shadow sampling

## Final Summary
You have moved from startup and build issues to a functioning mini engine with camera, depth, animation, lighting, atmosphere, and projected soft shadows. The strongest next quality jump is a proper color pipeline and true shadow mapping.

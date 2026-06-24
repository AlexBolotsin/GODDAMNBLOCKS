# Implementation Log

## Why This File Exists
This is a practical changelog for the engine work completed so far.
It is meant to answer:
- what was added
- what problems were fixed
- which files changed conceptually
- what the current state is
- what should be checked next

Use this together with `STUDY_GUIDE_PROGRESS.md`.
The study guide explains the architecture; this file records the actual implementation progress.

## Current State Snapshot
The project is currently a custom Direct3D 12 renderer with:
- a working window and frame loop
- swap chain, command queue, command list, fences, RTVs, and DSV
- runtime-compiled HLSL from `Source/Material.cpp`
- shared material / PSO setup
- per-frame and per-object root constants
- animated cubes and a visible ground plane
- directional lighting, fill lighting, ambient light, fog, and sky blending
- projected soft shadows
- billboard-style actors for 2D sprites in a 3D scene
- a texture sampling path for sprite-sheet-backed billboard actors

## Major Work Completed

### 1. Engine Bring-Up And Stability
Completed:
- stabilized the CMake + MSVC build flow
- fixed startup and draw-path crashes
- added safer handling around invalid material / pipeline state usage
- got the basic scene rendering reliably

Key impact:
- the project now builds and launches consistently in Debug
- render setup failures are easier to diagnose than before

Relevant files:
- `CMakeLists.txt`
- `Source/DX12Context.cpp`
- `Source/Entity.cpp`
- `Source/Material.cpp`

### 2. Camera, Projection, And Depth
Completed:
- added right-handed camera helpers
- corrected projection and transform behavior
- added a depth buffer and depth testing
- cleaned up frame begin/end render target handling

Key impact:
- 3D geometry renders with correct depth ordering
- camera orbit and scene perspective behave as expected

Relevant files:
- `Source/DX12Context.cpp`
- `Source/WinMain.cpp`

### 3. Lighting And Scene Readability
Completed:
- added multi-term lighting instead of flat color only
- added fog and sky color blending for distance readability
- added visible ground plane and animated cubes for spatial context

Key impact:
- the scene is much easier to read visually
- the renderer now has a more complete forward-lighting baseline

Relevant files:
- `Source/Material.cpp`
- `Source/WinMain.cpp`

### 4. Projected Soft Shadows
Completed:
- added a projected shadow pass using extra draw calls
- added offsets and bias to reduce z-fighting and flicker
- tuned layered shadow drawing to create a softer result

Key impact:
- the scene now has contact-style directional shadows
- earlier flickering caused by coplanar depth conflict was reduced significantly

Relevant files:
- `Source/DX12Context.cpp`
- `Source/Entity.cpp`
- `Source/Material.cpp`

Notes:
- this is not a true shadow-map system yet
- it is a lightweight projected-shadow approach for learning and iteration speed

### 5. Shader Experiments And Rollbacks
Completed:
- added a toon / cel shading experiment
- tuned it multiple times for stability and edge behavior
- later removed toon shading when it was no longer desired
- added a black outline / silhouette effect and tuned it to avoid over-darkening top faces

Key impact:
- the renderer supported fast shader iteration
- the project returned to smooth lighting while keeping lessons from the experiment

Relevant files:
- `Source/Material.cpp`

Notes:
- the final state is smooth lighting, not toon shading

### 6. Billboard Actors For 2D-In-3D Sprites
Completed:
- added billboard actor support so selected entities face the camera
- added per-entity flags for billboard behavior
- created a sprite quad mesh and spawned sprite actors in the scene
- fixed winding / culling issues that initially made sprites invisible

Key impact:
- actors can now behave like 2D sprites while still existing in a 3D world

Relevant files:
- `Source/Entity.h`
- `Source/DX12Context.cpp`
- `Source/WinMain.cpp`
- `Source/Mesh.h`

### 7. Sprite Asset Preparation
Completed:
- detected and prepared the sprite sheet asset in `Sprites/19338.png`
- created an atlas metadata file
- created a PowerShell helper to regenerate atlas data
- documented the sprite asset workflow

Key impact:
- the project now has a repeatable path for extracting sprite frames from the sprite sheet

Relevant files:
- `Sprites/19338.atlas.json`
- `Sprites/README.md`
- `Tools/Build-SpriteAtlas.ps1`

### 8. Texture-Backed Sprite Rendering Path
Completed:
- expanded vertex data to include UV coordinates
- added per-entity sprite UV rectangle data
- added a texture sampling path in the pixel shader
- added an SRV descriptor table and static sampler to the root signature
- implemented WIC-based PNG loading
- created GPU texture upload and SRV creation logic
- bound the texture descriptor heap during draw calls
- wired sprite actors to atlas frame rectangles in scene setup

Key impact:
- billboard actors can now render textured sprite frames instead of only flat tint color

Relevant files:
- `Source/Material.h`
- `Source/Material.cpp`
- `Source/Entity.h`
- `Source/Entity.cpp`
- `Source/Mesh.h`
- `Source/WinMain.cpp`
- `CMakeLists.txt`

## Important Problems Solved

### Root Signature Budget Overflow
Problem:
- material initialization failed after the texture path was added because the root signature exceeded the D3D12 64-DWORD budget

Fix:
- removed unnecessary per-frame camera position root constants and used view-space logic in shader code instead

Result:
- the material and PSO path could initialize again without overflowing the root signature budget

### Shader / Material Init Failures
Problem:
- shader iteration introduced failures caused by naming collisions and stale shader-side assumptions

Fix:
- corrected shader variable naming conflicts
- aligned shader bindings with the actual root signature layout
- improved stage-specific material initialization diagnostics

Result:
- material setup failures became easier to identify and recover from

### Billboard Visibility Problems
Problem:
- billboard quads initially did not appear correctly because of winding and culling mismatches

Fix:
- corrected quad winding and related scene-side mesh setup

Result:
- billboard actors became visible in the scene

### Textured Billboard Runtime Crash
Problem:
- the textured sprite path caused an access violation during development

Likely cause that was fixed:
- COM lifetime management for WIC image loading was being handled inside the texture loader in a way that could destabilize the runtime path

Fix:
- moved COM ownership to application lifetime in `Source/WinMain.cpp`
- removed per-load COM init / uninit behavior from the texture loader in `Source/Material.cpp`
- repaired a follow-up brace error introduced while patching that function

Result:
- the project rebuilt successfully
- smoke runs completed successfully after the fix

## Current Texture Path Status
The texture-backed billboard path is currently enabled in scene setup.
That means the code is presently configured so sprite actors can sample texture frames from the prepared sprite sheet.

Current expectations:
- sprite actors should face the camera
- sprite actors should use atlas UVs
- the pixel shader should sample the sprite texture when the per-draw flag enables textured sprite mode
- chroma-key style transparency handling should remove the background around extracted sprite content

Current validation status:
- build validation: passed
- short runtime smoke test: passed
- final visual confirmation in the scene: still worth checking manually

## Files Worth Reading First
If you want to understand the work in code order, read these files in this sequence:

1. `Source/WinMain.cpp`
- scene setup
- sprite actor creation
- atlas frame assignment
- COM lifetime initialization

2. `Source/DX12Context.cpp`
- render flow
- projected shadow pass
- billboard transform handling

3. `Source/Entity.cpp`
- draw submission
- root constant upload
- SRV heap binding for textured sprites

4. `Source/Material.cpp`
- root signature layout
- pipeline state creation
- shader source
- texture loading and SRV creation

5. `Source/Material.h`
- material interface and texture-related members

## Known Limitations
These are still true in the current build:
- shaders are embedded in C++ strings instead of separate `.hlsl` files
- shadows are projected shadows, not shadow maps
- sprite frame selection is wired in code rather than driven by a runtime animation system
- sprite transparency uses a simple chroma-key style approach rather than authored alpha
- temporary debug traces may still exist from the texture-path investigation

## Recommended Next Checks
1. Launch the build and visually confirm the billboard actors are showing sprite-sheet imagery instead of flat color.
2. If the sprites appear wrong, check atlas UV rect assignment in `Source/WinMain.cpp` first.
3. If transparency looks wrong, inspect the texture sampling branch and chroma-key logic in `Source/Material.cpp`.
4. If you want cleaner maintenance, move the shader source out of `Source/Material.cpp` into external `.hlsl` files.
5. If you want better shadows later, replace projected shadows with a real shadow-map pass.

## Related Docs
- `STUDY_GUIDE_PROGRESS.md`
- `docs/README.md`
- `docs/architecture.md`
- `docs/render-pipeline.md`
- `docs/shader-layout.md`
- `docs/debug-playbook.md`
- `docs/learning-roadmap.md`

## Short Summary
The engine progressed from a basic DX12 scene into a more complete learning renderer with depth, lighting, fog, projected shadows, billboard actors, sprite asset preparation, and a texture-based sprite rendering path. The latest technical milestone was stabilizing the textured billboard pipeline so sprite actors can use frames from the sprite sheet at runtime.

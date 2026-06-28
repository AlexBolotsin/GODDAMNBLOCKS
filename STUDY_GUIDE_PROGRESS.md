# Study Guide and Progress

This is your working notebook. Update it as you explore and experiment.
The architecture and pipeline docs in `docs/` are the reference; this file is personal notes.

---

## Current Engine Snapshot

**Build:** CMake + MSVC, VS 2022 toolset, C++17  
**Config:** Debug (`Ctrl+Shift+B`) and Release (`Tasks: Run Task → Build Release`)  
**Output:** `build/Debug/MazeGame.exe` or `build/Release/MazeGame.exe`

### What the engine renders each frame

```
Shadow pass     → shadow map 2048×2048 (depth only)
MSAA main pass  → geometry + 5000 instanced sprites + particles → 8×MSAA HDR target
  └ shockwave CB pre-filled here (camera + world data both in scope)
Resolve         → MSAA target → HDR R16G16B16A16_FLOAT
Bloom           → bright-pass → blur-H → blur-V (half resolution)
Distort         → world-space shockwave ring → distortTarget (full resolution)
Tonemap         → distortTarget + bloom → backbuffer, ACES curve, optional scanlines/dither
```

### Active scene objects

| Group | Count | Notes |
|---|---|---|
| Ground plane | 1 | Large flat geometry, checker/normal detail in shader |
| Animated cubes | 3 | Simple test geometry, colored, sinusoidally animated |
| Hero billboard sprites | 3 | Full per-entity draw, sprite atlas texture, shadow cast |
| Blob shadows | 3 | One per hero sprite, flat oval mesh |
| Targeting ring | 1 | Unlit flat ring, follows mouse cursor on ground |
| Bulk instanced sprites | 5 000 | GPU-instanced, GPU hover animation, per-sprite tint |
| Meteors | dynamic | Sphere mesh, PhysicsComp, spawned on left-click |
| Explosion spheres | dynamic | Expand over ~1s, trigger scorch zone, then removed |
| Fire particles | dynamic | CPU-sim, GPU-additive-billboard, emitted by meteors + burning sprites |
| Burning sprites | dynamic | BurningComp + PhysicsComp, tint-shift to red then disabled |

---

## File Map — Where to Look for What

| I want to understand... | Read this |
|---|---|
| Full frame sequence | `DX12Context::RenderScene` + `EndFrame` in `Source/DX12Context.cpp` |
| All GPU pipelines and their shaders | `docs/shader-layout.md` |
| All render passes in order | `docs/render-pipeline.md` |
| How entities are created and accessed | `Source/ECS.h`, `Source/Components.h`, `Source/World.h` |
| How component pools work internally | `ComponentPool<T>` in `Source/ECS.h` (~70 lines) |
| How meteors, explosions, particles work | `Game::Update` in `Source/Game.cpp` |
| How the burning system works | Burning section near end of `Game::Update` |
| How the targeting ring follows the mouse | Ray cast section in `Game::Update` |
| How 5000 sprites avoid a CPU loop | Instanced sprite section in `DX12Context::RenderScene` |
| How the hover animation works on GPU | Instanced sprite VS in `DX12Context.cpp` |
| How shadows are cast and received | Shadow pass in `DX12Context::RenderScene` + `Material.hlsl` PS |
| How the shockwave distortion works | `CreateDistortPipeline` + distort pass in `EndFrame` |
| Why the ring is an ellipse, not a circle | Distort PS in `DX12Context.cpp` (ray-plane intersection) |
| How bloom and tonemap work | Post-process pipelines in `DX12Context.cpp` |
| How the camera orbit/pan/zoom works | `Game::Update` camera section |
| Math helpers | `Source/EngineMath.h` |
| Controls reference | `CONTROLS.txt` |

---

## Core Concepts Checklist

Use this to track what you have understood. Mark each when you can explain it without looking.

### D3D12 Fundamentals
- [ ] What a command list is and why you must reset it each frame
- [ ] What a command allocator is and why there is one per frame (double-buffering)
- [ ] What a fence is and how `MoveToNextFrame` uses it to prevent the CPU from overwriting GPU resources
- [ ] The difference between a committed resource and an upload buffer
- [ ] What a resource barrier is and when you need one
- [ ] What an RTV, DSV, and SRV are and where each type of heap lives

### Root Signatures and Bindings
- [ ] What a root signature is (the parameter contract between CPU draw calls and shader registers)
- [ ] The difference between a root CBV, a root SRV, and a descriptor table
- [ ] Why `Num32BitValues` must match the C++ struct size exactly
- [ ] How the instanced sprite pipeline uses a root SRV instead of a vertex buffer

### Math
- [ ] Row-major vs column-major: which convention this engine uses and where it matters
- [ ] How `MatrixLookAtRH` builds the view matrix from eye, target, up
- [ ] How `MatrixPerspectiveRH` maps 3D space to clip space
- [ ] How to extract camera basis vectors (right, up, back) from the view matrix columns
- [ ] What the NDC (normalized device coordinates) are and how to unproject a screen pixel to a world ray
- [ ] What `MatrixBillboard` does and why it is cylindrical (Y-locked) rather than spherical
- [ ] Why `proj00 = projMatrix.m[0]` and `proj11 = projMatrix.m[5]` are the unproject denominators

### Rendering
- [ ] Why the main pass renders to an MSAA target and not directly to the back buffer
- [ ] What MSAA resolve does and why it needs a `ResolveSubresource` call
- [ ] How a depth-only shadow pass produces a shadow map
- [ ] What PCF shadow sampling is and why it gives softer shadows than a hard depth compare
- [ ] Why additive particles need depth-write OFF but depth-test ON
- [ ] Why the bloom pass works at half resolution
- [ ] What ACES tone mapping does and why you need it in an HDR pipeline
- [ ] Why the tonemap samples `distortTarget` instead of the raw HDR target
- [ ] Why a screen-space ring is always circular but a world-space ray-plane ring is an ellipse

### ECS
- [ ] Why `EntityID` is just a uint32_t (data lives in pools, not in objects)
- [ ] How `ComponentPool<T>` uses a hash-map index into dense parallel arrays
- [ ] Why `Remove` uses swap-with-last to keep arrays dense
- [ ] How `DestroyEntity(id)` removes an entity from every pool at once
- [ ] How the render loop iterates `world->renders` by index without iterating every pool

### Game Logic
- [ ] How the LCG (`m_meteorRng`, `m_particleRng`) produces pseudo-random numbers
- [ ] How the mouse ray is cast from screen pixel to world position
- [ ] How the scorch zone decides which sprites get `BurningComp` (radius check per sprite)
- [ ] How iterating `world.burning` pool directly avoids the old `BurningSprite` pointer lifetime problem
- [ ] How `FireParticle` (CPU sim) maps to `SceneParticle` (GPU upload) each frame
- [ ] How `world->shockwaves` is built from live explosions and consumed by the distort pass

---

## Experiments Log

Use this section to record what you tried, what you observed, and what you learned.

### Template entry
```
Date: YYYY-MM-DD
Change: [what you modified]
File: [which file]
Observed: [what happened at runtime]
Learned: [the concept this confirmed or revealed]
```

---

## Quick Glossary

| Term | Meaning |
|---|---|
| PSO | Pipeline State Object — the compiled pipeline configuration (shaders, blend, rasterizer, etc.) |
| Root signature | The binding contract: declares what registers the shaders expect and how the CPU supplies them |
| RTV | Render Target View — GPU handle to a texture that can be rendered into |
| DSV | Depth Stencil View — GPU handle to a depth buffer |
| SRV | Shader Resource View — GPU handle to a texture or buffer readable by shaders |
| CBV | Constant Buffer View — GPU handle to a small uniform buffer |
| MSAA | Multi-Sample Anti-Aliasing — samples coverage at N points per pixel, averages at resolve |
| NDC | Normalized Device Coordinates — clip space after perspective divide: X/Y ∈ [-1,1], Z ∈ [0,1] |
| PCF | Percentage Closer Filtering — soft shadow by averaging multiple shadow map samples |
| ACES | Academy Color Encoding System — a standard filmic tone mapping curve |
| LCG | Linear Congruential Generator — a cheap PRNG using multiply-add-modulo |
| Billboard | A quad that always faces the camera; cylindrical = Y-axis locked (sprites), spherical = fully facing |
| Additive blend | `result = src + dest` — particles accumulate brightness, never darken |
| Scorch zone | The ring between the kill radius and ~1.8× explosion radius where sprites start burning |
| Upload buffer | A CPU-writable, GPU-readable D3D12 resource in `UPLOAD` heap — used for streaming data each frame |
| Root SRV | An SRV bound directly in the root signature (not via descriptor heap) — simpler, 1 DWORD cost |
| Structured buffer | A buffer with a fixed per-element stride, readable in HLSL via `StructuredBuffer<T>` |
| EntityID | A uint32_t that names an entity — data lives in component pools, not in the ID itself |
| ComponentPool | Sparse-set: hash-map from EntityID → dense array index; arrays are iterated cache-linearly |
| Sparse-set | Data structure combining O(1) lookup (via hash-map) with O(N) cache-friendly iteration (dense arrays) |
| Ray-plane intersection | Finding where a view ray hits the ground plane: `t = (planeY - eye.y) / dir.y`, `hit = eye + t*dir` |
| Foreshortening | Perspective compression: a ring on the ground looks like an ellipse from an oblique angle |

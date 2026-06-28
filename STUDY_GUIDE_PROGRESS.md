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
Resolve         → MSAA target → HDR R16G16B16A16_FLOAT
Bloom           → bright-pass → blur-H → blur-V (all at half resolution)
Tonemap         → HDR + bloom → backbuffer, ACES curve, optional scanlines/dither
```

### Active scene objects

| Group | Count | Notes |
|---|---|---|
| Ground plane | 1 | Large flat geometry, checker/normal detail in shader |
| Animated cubes | ~4 | Simple test geometry, colored, animated |
| Hero billboard sprites | 3 | Full per-entity draw, sprite atlas texture, shadow receive |
| Blob shadows | 3 | One per hero sprite, flat oval mesh |
| Targeting ring | 1 | Unlit flat ring, follows mouse cursor on ground |
| Bulk instanced sprites | 5 000 | GPU-instanced, GPU hover animation, per-sprite tint |
| Meteors | dynamic | Sphere mesh, downward velocity, spawned on left-click |
| Explosion spheres | dynamic | Expand over ~1s, trigger scorch zone, then removed |
| Fire particles | dynamic | CPU-sim, GPU-additive-billboard, emitted by meteors + burning sprites |

---

## File Map — Where to Look for What

| I want to understand... | Read this |
|---|---|
| Full frame sequence | `DX12Context::RenderScene` in `Source/DX12Context.cpp` |
| All GPU pipelines and their shaders | `docs/shader-layout.md` |
| All render passes in order | `docs/render-pipeline.md` |
| How meteors, explosions, particles work | `Game::Update` in `Source/Game.cpp` |
| How the targeting ring follows the mouse | Ray cast section in `Game::Update` |
| How 5000 sprites avoid a CPU loop | Instanced sprite section in `DX12Context::RenderScene` |
| How the hover animation works on GPU | Instanced sprite VS in `DX12Context.cpp` |
| How shadows are cast and received | Shadow pass in `DX12Context::RenderScene` + `Material.hlsl` PS |
| How bloom and tonemap work | Post-process pipelines in `DX12Context.cpp` |
| How the camera orbit/pan/zoom works | `Game::Update` camera section |
| How entities are drawn | `Entity::Draw` in `Source/Entity.cpp` |
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
- [ ] How to extract camera basis vectors (right, up, back) from the view matrix
- [ ] What the NDC (normalized device coordinates) are and how to unproject a screen pixel to a world ray
- [ ] What `MatrixBillboard` does and why it is cylindrical (Y-locked) rather than spherical

### Rendering
- [ ] Why the main pass renders to an MSAA target and not directly to the back buffer
- [ ] What MSAA resolve does and why it needs a `ResolveSubresource` call
- [ ] How a depth-only shadow pass produces a shadow map
- [ ] What PCF shadow sampling is and why it gives softer shadows than a hard depth compare
- [ ] Why additive particles need depth-write OFF but depth-test ON
- [ ] Why the bloom pass works at half resolution
- [ ] What ACES tone mapping does and why you need it in an HDR pipeline

### Game Logic
- [ ] How the LCG (`m_meteorRng`, `m_particleRng`) produces pseudo-random numbers
- [ ] How the mouse ray is cast from screen pixel to world position
- [ ] How the scorch zone decides which sprites start burning (radius check per sprite)
- [ ] How `BurningSprite` shifts tint over time and disables the entity on burnout
- [ ] How `FireParticle` (CPU sim) maps to `SceneParticle` (GPU upload)

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
| Scorch zone | The ring between the kill radius and 1.8× explosion radius where sprites start burning |
| Upload buffer | A CPU-writable, GPU-readable D3D12 resource in `UPLOAD` heap — used for streaming data each frame |
| Root SRV | An SRV bound directly in the root signature (not via descriptor heap) — simpler, 1 DWORD cost |
| Structured buffer | A buffer with a fixed per-element stride, readable in HLSL via `StructuredBuffer<T>` |

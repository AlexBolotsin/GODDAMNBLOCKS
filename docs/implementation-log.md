# Implementation Log

Chronological record of what was built, what broke, and why decisions were made.
For the current state snapshot see `STUDY_GUIDE_PROGRESS.md`.

---

## Stage 1 — Engine Bring-Up and Stability

**What was built:**
- Win32 window via `GrmWindowWrapper`
- D3D12 device, command queue, command list, command allocators
- DXGI swap chain with two back buffers
- Basic RTV heap and frame loop
- CMake + MSVC build pipeline

**Problems solved:**
- CMake precompiled-header confusion with generated `cmake_pch` sources
- D3D12 startup crash from null pipeline / missing resource state
- Draw path crash from incorrect root constant upload sizes

**Key lesson:**  
D3D12 does not validate much by default. The debug layer (`_DEBUG` build) is the only way to get
useful error messages. Always develop in Debug config with the debug layer active.

---

## Stage 2 — Camera, Depth, and Correct Projection

**What was built:**
- `MatrixLookAtRH` and `MatrixPerspectiveRH` in `EngineMath.h`
- D32_FLOAT depth buffer; depth test and depth clear each frame
- Orbit camera controlled from `WinMain`
- Ground plane mesh and animated cube entities

**Problems solved:**
- Invisible geometry from CCW/CW winding mismatch — fixed by setting `FrontCounterClockwise=TRUE`
- Projection artifacts from incorrect near/far clip values

**Key lesson:**  
Row-major vs column-major layout is a permanent source of bugs. The convention here is row-major
in C++ (`m[row*4+col]`) with matrices multiplied left-to-right (`vertex × world × view × proj`).
HLSL receives the same row-major data and multiplies in the same order. Never change this without
updating every matrix multiply in both C++ and HLSL.

---

## Stage 3 — Lighting and Atmosphere

**What was built:**
- Per-pixel directional key light, fill light, ambient light in `Material.hlsl`
- Distance fog with sky-gradient horizon/zenith color blend
- Floor detail: checker pattern and normal perturbation
- Edge darkening (`NdotV` pow term)

**Key lesson:**  
Fog that blends to a flat grey looks wrong at any camera angle because the sky is not flat grey.
The fix is to lerp the fog color between a horizon color and a zenith color based on the NDC-Y of
the pixel — the higher up in screen space, the more zenith color. This requires passing `clip.y/w`
from VS to PS (the `clipYW` semantic).

---

## Stage 4 — Billboard Sprites and Texture Atlas

**What was built:**
- `MatrixBillboard` — cylindrical Y-locked transform (sprites face camera but don't tilt)
- `usesSpriteTexture` flag on Entity — enables texture sampling branch in PS
- WIC PNG loader in `Material.cpp` (COM lifetime moved to `WinMain` for stability)
- SRV descriptor table in material root signature (t1 = sprite texture)
- Atlas UV rect per entity (`spriteUVRect`)
- Hero actor sprites in the scene

**Problems solved:**
- Billboard sprites invisible: winding was CCW but billboard geometry was CW → flipped quad vertex order
- Access violation during texture load: WIC COM objects were being initialized and destroyed inside
  the loader rather than once at application startup → moved `CoInitializeEx` to `WinMain`
- Root signature budget overflow: early design put per-frame camera position in a root constant
  slot that pushed past the 64-DWORD D3D12 budget → moved camera to a separate region

**Key lesson:**  
The D3D12 root signature has a 64-DWORD budget (256 bytes of root constants + descriptor table
slots). Each inline root constant uses 1 DWORD. Each descriptor table range uses 1 DWORD.
Once you exceed the budget, `CreateRootSignature` fails.

---

## Stage 5 — GPU-Instanced Sprites

**What was built:**
- Separate `m_instancedSpritePso` with its own root signature
- `SpriteInstanceData` struct (world matrix + UV rect + tint + hoverData = 112 bytes)
- Double-buffered upload buffer for instance data (`FrameCount × 5000 × 112` bytes)
- Root SRV bound to instance buffer (avoids descriptor heap slot for structured buffer)
- `DrawInstanced(6, count, 0, 0)` — no vertex buffer; VS reads instance data by `SV_InstanceID`
- 5 000 sprite entities

**Problems solved:**
- PSO format mismatch: instanced sprite PSO had `RTVFormats[0] = R8G8B8A8_UNORM` but the MSAA
  target is `R16G16B16A16_FLOAT` — sprites rendered blank. Fixed by matching formats.
- Stride constant in three places: buffer allocation, per-frame slot offset, and root SRV
  address. Missing any one causes every sprite to read wrong data.

**Key lesson:**  
GPU instancing with a structured buffer SRV is simpler than an instanced vertex buffer when your
data is large. The vertex shader reads `g_instances[SV_InstanceID]` and the root SRV address is
just `bufferGpuAddress + frameIndex * maxInstances * stride`.

---

## Stage 6 — GPU Hover Animation

**What was built:**
- `hoverPhase` per entity (per-entity random offset, assigned at spawn)
- `vec4 hoverData` in `SpriteInstanceData` (`.x` = phase)
- `time` field added to `PerFrameCbData`, written from `world->time` each frame
- Vertex shader: `wp.y += sin(time * 1.45 + inst.hoverData.x) * 0.10`

**What was removed:**
- Per-frame CPU loop calling `SetPosition(Y)` on 5 000 entities

**Key lesson:**  
Any per-vertex or per-instance animation that depends only on time and a static per-instance value
belongs on the GPU, not the CPU.

---

## Stage 7 — Shadow Map

**What was built:**
- 2048×2048 `m_shadowMap` resource (R32_TYPELESS, used as D32_FLOAT DSV and R32_FLOAT SRV)
- Separate DSV heap for shadow map
- `m_shadowPso` — depth-only geometry pass
- `m_shadowSpriteRootSig` / `m_shadowSpritePso` — depth-only billboard pass
- Light view-projection matrix built in `RenderScene`
- PCF sampling in `Material.hlsl`: `SampleCmpLevelZero` with `LESS_EQUAL` comparison sampler

**Why this replaced the old projected shadow system:**  
Projected shadows only cast onto a flat plane. A shadow map costs one extra draw pass and then
samples a single texture — far more general.

---

## Stage 8 — Targeting Ring and Click-to-Cast

**What was built:**
- `CreateTargetRingMesh` — 48-segment flat ring (inner=0.82, outer=1.0)
- Mouse ray casting: unproject NDC through perspective frustum, intersect with Y=-1 ground plane
- Targeting ring entity with `isUnlit=true`
- Left-click spawns 3–5 sphere-mesh meteors scattered within `kTargetRadius = 2.5`

**Key lesson — view matrix basis extraction:**  
`MatrixLookAtRH` stores camera axes as column vectors across the rows of the matrix. To extract:
```cpp
camRight = { view.m[0], view.m[4], view.m[8]  };
camUp    = { view.m[1], view.m[5], view.m[9]  };
camBack  = { view.m[2], view.m[6], view.m[10] };
```
Then: `rayDir = normalize(camRight * vx + camUp * vy - camBack)`

---

## Stage 9 — VFX: Particle System and Burning Sprites

**What was built:**
- `m_particlePso` — additive billboard particles, procedural soft-circle PS
- `SceneParticle` struct (32 bytes: position, size, RGBA)
- Double-buffered GPU upload buffer
- `FireParticle` CPU-side sim state (position, velocity, age, lifetime, size)
- Fire trail from meteors; fire particles from burning sprites
- Burning sprite system: tint shifts from original → orange → dark red; entity disabled at burnout

**Key lesson — additive blend particle depth:**  
With additive blend (`DestBlend=ONE`), depth write must be OFF. If ON, the first particle written
blocks all particles behind it. Depth test stays ON so particles correctly hide behind walls.

---

## Stage 10 — Post-Processing: Bloom, Tonemap, Scanlines, Dithering

**What was built:**
- HDR intermediate target `m_hdrTarget` (R16G16B16A16_FLOAT)
- MSAA resolve pass: MSAA → HDR
- Half-resolution bloom: bright-pass → blur-H → blur-V
- Tonemap PSO: ACES filmic curve
- Scanline toggle (`C` key), Bayer dithering toggle (`V` key)
- FPS / frame time / entity count / draw count HUD

**Key lesson — HDR pipeline order:**  
All scene rendering happens in HDR space. Additive particles can push values above 1.0 — intentional.
The tonemap pass compresses to [0,1] at the very end. Never clamp during scene rendering.

---

## Stage 11 — Camera Polish: Pan, Orbit Confinement

**What was built:**
- Middle-mouse drag: translate `m_camera.target` along camera-relative ground axes
- Cursor confinement (`ClipCursor`) during right-mouse drag
- Camera target persists between frames

**Key lesson:**  
Camera pan direction must use azimuth-derived basis vectors, not world axes:
```cpp
camRight     = { -sin(azimuth), 0,  cos(azimuth) };
camFwdGround = { -cos(azimuth), 0, -sin(azimuth) };
```

---

## Stage 12 — Shockwave Air Distortion

**What was built:**
- `m_distortTarget` — full-resolution R16G16B16A16_FLOAT render target
- `m_distortRootSig` / `m_distortPso` — fullscreen distort PSO
- Double-buffered shockwave CBs (`m_shockwaveCb[FrameCount]`)
- Distort pass inserted between bloom-V and tonemap in `EndFrame`
- Tonemap SRV updated to read `distortTarget` instead of raw HDR

**First attempt (rejected):** Rings were computed in screen UV space — a perfect circle regardless
of camera angle. Shockwaves at ground level looked like they were painted on the screen, not on the ground.

**Second implementation (current):** Ray-plane intersection per fragment.
- CB carries camera basis vectors (`cameraRight`, `cameraUp`, `cameraBack` from view matrix columns),
  projection parameters (`proj00`, `proj11`), ground Y, and per-wave world-space data
  (`worldX`, `worldZ`, `worldRadius` in metres, `uvStrength`).
- CB is filled in `RenderScene` where `frameData` and `camera` are in scope.
- Shader reconstructs a world-space view ray, intersects with `groundY = -1`,
  measures ring distance in world metres, projects the outward direction back into UV space.
- Sky pixels (`rayDir.y >= 0`) early-out with zero distortion.

**Key lessons:**
- Screen-space rings can only be circles. World-space rings naturally foreshorten into ellipses.
- The CB fill must happen where both the camera and the scene data are in scope. In this engine that
  is the end of `RenderScene`'s main pass block, even though the draw itself is in `EndFrame`.
- Camera basis vectors come from the **columns** of the view matrix (because the engine uses
  row-vector multiplication `v * M`): `cameraRight = (vm[0], vm[4], vm[8])`.
- `proj00 = projMatrix.m[0]`, `proj11 = projMatrix.m[5]` — these are the diagonal entries of the
  D3D12 RH perspective matrix, equal to `cot(fovY/2)/aspect` and `cot(fovY/2)` respectively.

---

## Stage 13 — ECS Migration

**What was built:**
- `Source/ECS.h` — `EntityID` (uint32_t) + `ComponentPool<T>` (sparse-set, ~70 lines)
- `Source/Components.h` — `TransformComp`, `RenderComp`, `SpriteComp`, `PhysicsComp`, `BurningComp`
- `Source/World.h` / `World.cpp` — `World` class with typed pool members, replaces `Scene`
- `Entity.h` / `Entity.cpp` reduced to thin redirects (no `Entity` class, no `Entity::Draw`)
- `Scene.h` / `Scene.cpp` reduced to `using Scene = World` alias
- `DX12Context.h` — `RenderScene` signature changed to `World*`; sort buffer changed to `pair<float, EntityID>`
- `DX12Context.cpp` — render loops rewritten to iterate `world->renders` pool by index
- `Game.h` — all `Entity*` tracking replaced with `EntityID`; `BurningSprite` struct removed
- `Game.cpp` — all entity creation/mutation/destruction ported to ECS API

**Why free-function systems instead of a system class hierarchy:**  
System classes add boilerplate (registration, ordering, virtual dispatch) without benefit at this
engine's scale. Each subsystem is a self-contained block inside `Game::Update` that iterates one
or two pools. The ECS gives data locality without introducing framework complexity.

**Key lessons:**
- Swap-with-last in `ComponentPool::Remove` is the key to keeping arrays dense without gaps.
  After removing index `i`: copy last element to `i`, update the hash-map entry for the moved ID.
- The burning system changed from a separate `vector<BurningSprite>` (entity pointers) to
  iterating `world.burning` pool directly. This eliminates the pointer lifetime problem — if an
  entity is destroyed, its pool entry is already gone.
- The shadow and main-pass render loops both iterate `world->renders.Size()` and call
  `world->transforms.Get(id)` per entity. The cost is one hash-map lookup per draw call, which
  is negligible compared to the GPU work.

---

## Known Limitations

- No audio.
- No scene serialization — the world is fully recreated from code each run.
- Frustum culling only applies to the instanced sprite batch. Hero sprites, cubes, and explosion
  spheres are always submitted regardless of visibility.
- Particle count resets to 0 each frame and is rebuilt from `m_fireParticles`. Intentional —
  avoids stale particles persisting when entities are removed.
- The distort pass ring width (`kRingWidth = 0.75` metres) is a constant. Very large explosions
  produce a proportionally thinner-looking ring than small ones.

# Architecture

## Bird's-Eye View

```
WinMain.cpp
  │
  ├─► GrmWindowWrapper   — OS window, message pump, cursor, scroll
  ├─► DX12Context        — GPU device, all pipelines, all render passes
  └─► Game               — world content, game logic, camera
        └─► World        — EntityID allocator + typed component pools
              ├─ ComponentPool<TransformComp>   (position / rotation / scale)
              ├─ ComponentPool<RenderComp>      (mesh, material, tint, flags)
              ├─ ComponentPool<SpriteComp>      (atlas UV, hover phase, animation)
              ├─ ComponentPool<PhysicsComp>     (velocity — meteors, ragdoll)
              ├─ ComponentPool<BurningComp>     (age, duration, original tint)
              ├─ std::vector<SceneParticle>     (CPU-sim fire particles for GPU)
              └─ std::vector<SceneShockwave>    (active explosion rings for distort pass)
```

Every frame the loop runs:
1. `GrmWindowWrapper` pumps OS messages and produces an `InputState`.
2. `Game::Update` runs game logic and writes into `World`.
3. `DX12Context::BeginFrame` → `RenderScene(world, camera)` → `EndFrame` reads from `World` and drives the GPU.

---

## ECS Core (`Source/ECS.h`, `Source/Components.h`)

### EntityID

```cpp
using EntityID = uint32_t;
static constexpr EntityID kNullEntity = 0;
```

An entity is just a number. There is no `Entity` class. All data lives in component pools.

### ComponentPool\<T\>

A sparse-set backed by two parallel dense arrays:

```cpp
template<typename T>
class ComponentPool {
    std::unordered_map<EntityID, uint32_t> m_index; // O(1) lookup
    std::vector<EntityID>                  m_ids;   // dense — for iteration
    std::vector<T>                         m_data;  // dense — cache-friendly
};
```

| Operation | Cost |
|---|---|
| `Has(id)` | O(1) |
| `Get(id)` | O(1) |
| `Add(id, comp)` | O(1) amortized |
| `Remove(id)` | O(1) — swap-with-last |
| Iteration (`Size()`, `IdAt(i)`, `DataAt(i)`) | O(N), cache-friendly |

`Remove` swaps the target entry with the last entry and pops, keeping the arrays dense.

### Component structs (`Source/Components.h`)

| Struct | Fields | Who uses it |
|---|---|---|
| `TransformComp` | `Transform transform` | Every positioned entity |
| `RenderComp` | `mesh, material, tint, visible, isBillboard, isInstanced, …` | Every drawn entity |
| `SpriteComp` | `uvRect, hoverPhase, animFrames, animSpeed, animTimer` | Billboard sprites only |
| `PhysicsComp` | `velocity` | Meteors + burning ragdoll |
| `BurningComp` | `age, duration, origTint` | Sprites hit by explosion scorch zone |

---

## Systems

### GrmWindowWrapper  (`Source/GrmWindowWrapper.*`)
Creates the Win32 window and owns the message loop. Exposes:
- `PumpMessages()` — returns false on WM_QUIT.
- `ConsumeInput()` — returns delta mouse, scroll, resize, then resets internal accumulators.
- `ConfineCursor(bool)` — clips cursor to the client rect during right-drag orbit.
- Callbacks: `OnResize`, `OnDestroy`.

`InputState` (`Source/InputState.h`) is a plain struct filled by `WinMain` each frame:

| Field | Meaning |
|---|---|
| `mouseDeltaX/Y` | Raw delta since last frame (from WM_MOUSEMOVE) |
| `mouseAbsX/Y` | Client-space cursor position |
| `screenW/H` | Current client dimensions |
| `scrollDelta` | Accumulated scroll ticks |
| `rightMouseHeld` | Right button currently down |
| `middleMouseHeld` | Middle button currently down |
| `leftMouseClick` | Left button pressed this frame (edge) |
| `cinematicToggled` | Space bar pressed this frame (edge) |

---

### DX12Context  (`Source/DX12Context.*`)
Owns every GPU resource and drives the frame lifecycle. Key responsibilities:

**Initialization (one-time):**
- `CreateDevice` — enumerate adapters, create `ID3D12Device`.
- `CreateCommandObjects` — command queue, two command allocators, one command list.
- `CreateSwapChain` — DXGI swap chain with two back buffers.
- `CreateRenderTargetViews` — back-buffer RTVs + MSAA color target (R32_TYPELESS, 8× MSAA).
- `CreateDepthStencilBuffer` — D32_FLOAT depth, shared by main and MSAA resolve.
- `CreateShadowMapResources` — 2048×2048 R32_TYPELESS shadow map, its own DSV heap.
- `CreateShadowPipeline` — depth-only PSO for geometry.
- `CreateShadowSpritePipeline` — depth-only PSO for hero billboard sprites.
- `CreateInstancedSpritePipeline` — GPU-instanced billboards with per-instance data SRV.
- `CreateParticlePipeline` — additive billboard particles, depth-test on, depth-write off.
- `CreatePostProcessResources` — HDR target + two half-res bloom targets + full-res distort target + RTV/SRV heaps.
- `CreatePostProcessPipelines` — bright-pass, blur-H, blur-V, tonemap PSOs.
- `CreateDistortPipeline` — shockwave air-distortion PSO + double-buffered shockwave CB.

**Frame lifecycle:**
```
BeginFrame()
  reset allocator/list

RenderScene(world, camera)
  [Shadow pass]         — depth-only to shadow map; iterates world->renders
  [MSAA main pass]      — geometry + instanced sprites + particles → MSAA color target
    ↳ pre-fill shockwave CB here (camera data available)
  [MSAA resolve]        — MSAA target → HDR target (R16G16B16A16_FLOAT)

EndFrame()
  [Bloom bright-pass]   — HDR → half-res bright pixels
  [Bloom blur-H]        — bright → bloomA
  [Bloom blur-V]        — bloomA → bloomB
  [Distort pass]        — HDR → distortTarget (world-space ring using pre-filled CB)
  [Tonemap composite]   — distortTarget + bloomB → back-buffer; applies scanlines/dither
  barrier → PRESENT, present, fence sync
```

**Double buffering:**
- `FrameCount = 2`. Each frame index has its own command allocator, fence value, and shockwave CB slot.
- `MoveToNextFrame` waits for the older frame's fence before reusing its allocator.

---

### World  (`Source/World.*`)
Owns all entities and their component pools. Replaces the old `Scene` class.

```cpp
class World {
public:
    EntityID CreateEntity();       // returns next ID (starts at 1)
    void     DestroyEntity(id);    // removes from every pool
    void     Clear();              // wipes everything, resets IDs

    ComponentPool<TransformComp> transforms;
    ComponentPool<RenderComp>    renders;
    ComponentPool<SpriteComp>    sprites;
    ComponentPool<PhysicsComp>   physics;
    ComponentPool<BurningComp>   burning;

    std::vector<SceneParticle>   particles;  // CPU-written each frame, GPU-uploaded
    std::vector<SceneShockwave>  shockwaves; // active rings for the distort pass
    float                        time = 0.0f;
};
```

`Scene.h` is kept as a one-line alias (`using Scene = World`) for backward compatibility.

`SceneParticle` — 32 bytes: `float x,y,z,size,r,g,b,a`.  
`SceneShockwave` — `float x,y,z,age,maxAge,maxRadius` — one entry per live explosion during its distortion window.

---

### Game  (`Source/Game.*`)
All gameplay and scene-management logic. Runs entirely on the CPU; writes results into `World`.

**Entity tracking:** Game maintains `std::vector<EntityID>` lists for each logical group.
Parallel-index lists (e.g. `m_spriteActors` / `m_blobActors`) are synced by position — index 0 of blobs belongs to index 0 of sprites.

**Subsystems inside Game::Update:**

| Subsystem | What it does |
|---|---|
| Camera orbit/pan/zoom | Spherical coordinates → eye position |
| Cinematic mode | Sine-wave animated azimuth, elevation, radius |
| Targeting ring | Mouse ray cast to Y=-1 plane; updates `m_targetRing` transform and tint |
| Meteor spawning | Left-click adds entities with `PhysicsComp`, `RenderComp` |
| Meteor physics | `PhysicsComp` velocity applied per-frame, gravity accumulated |
| Explosion system | On impact: sphere entity expands; nearby sprites get `BurningComp` + `PhysicsComp` |
| Burning system | Iterates `world.burning` pool directly; shifts tint, applies ragdoll physics, emits particles |
| Fire particle emission | `FireParticle` CPU structs → `world.particles` for GPU upload |
| Shockwave data | Fills `world.shockwaves` from live explosions for the distort pass |
| Bulk sprite animation | Iterates `world.sprites` for atlas frame selection each frame |

**Systems are free functions / inline loops** — there is no system class. Each subsystem is a block inside `Game::Update`.

**LCG random number generators:**
- `m_meteorRng` — seeded `0xCAFEBABE`, used for meteor scatter and count.
- `m_particleRng` — seeded `0xFEDCBA98`, used for fire particle velocity and lifetime.

---

### Math  (`Source/EngineMath.h`)
Header-only: `vec2`, `vec3`, `vec4`, `mat4`, all operator overloads, and:
- `MatrixLookAtRH(eye, target, up)` — right-handed view matrix.
- `MatrixPerspectiveRH(fovY, aspect, nearZ, farZ)` — right-handed projection.
- `MatrixBillboard(worldPos, camEye, worldUp)` — cylindrical Y-locked billboard.
- `MatrixRotationY/X/Z`, `MatrixScale`, `MatrixTranslation`.

**Row-major layout (important for shader uploads):**  
`mat4.m[row*4 + col]`. The view matrix stores camera basis vectors in its columns:

```
m[0]=xAxis.x  m[1]=yAxis.x  m[2]=zAxis.x  m[3]=0
m[4]=xAxis.y  m[5]=yAxis.y  m[6]=zAxis.y  m[7]=0
m[8]=xAxis.z  m[9]=yAxis.z  m[10]=zAxis.z m[11]=0
...
```

So to extract camera basis (used in Game.cpp ray casting and in the shockwave CB fill):
```cpp
cameraRight = { vm[0], vm[4], vm[8]  };  // view col 0 = xAxis
cameraUp    = { vm[1], vm[5], vm[9]  };  // view col 1 = yAxis
cameraBack  = { vm[2], vm[6], vm[10] };  // view col 2 = zAxis (normalize(eye-target))
```

---

### Mesh  (`Source/Mesh.*`)
Creates vertex and index buffers as committed D3D12 resources. Exposes `Draw(commandList)`.
Vertex layout: `float3 Position, float3 Normal, float4 Color, float2 UV`.

Built-in mesh factories (anonymous namespace in `Game.cpp`):
- `CreateCubeMesh` — unit cube.
- `CreateGroundPlaneMesh` — large flat quad.
- `CreateSpriteQuadMesh` — unit billboard quad.
- `CreateBlobShadowMesh` — flat oval for blob shadows.
- `CreateSphereMesh` — UV sphere for explosions and meteors.
- `CreateTargetRingMesh` — 48-segment flat ring (inner=0.82, outer=1.0) for the targeting gizmo.

---

### Material  (`Source/Material.*`)
Wraps the hero-entity pipeline: root signature, PSO, WIC texture load, SRV.
The HLSL lives in `Source/Shaders/Material.hlsl` (external file, copied next to the exe at build time).
Compiled at runtime with `D3DCompile` so you can edit and relaunch without rebuilding C++.

---

## Why This Design is Good for Learning

- **Flat, explicit** — no engine abstractions hiding GPU calls. Every barrier, every bind, every upload is visible.
- **ECS is minimal and readable** — `ComponentPool<T>` is ~70 lines. There are no macros, no type-erasure, no reflection.
- **Systems are plain loops** — iterate `world->renders.Size()` and call `world->transforms.Get(id)`. No framework required.
- **CPU-readable frame graph** — the entire render order lives in `DX12Context::RenderScene` + `EndFrame`, top to bottom.
- **Game logic separated from rendering** — `Game.cpp` is pure C++ math/logic; `DX12Context.cpp` is pure GPU.

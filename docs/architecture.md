# Architecture

## Bird's-Eye View

```
WinMain.cpp
  │
  ├─► GrmWindowWrapper   — OS window, message pump, cursor, scroll
  ├─► DX12Context        — GPU device, all pipelines, all render passes
  └─► Game               — scene content, game logic, camera
        └─► Scene        — entity list, particle list, elapsed time
              └─► Entity — transform, mesh, material, flags, tint
```

Every frame the loop runs:
1. `GrmWindowWrapper` pumps OS messages and produces an `InputState`.
2. `Game::Update` runs game logic and writes into `Scene`.
3. `DX12Context::BeginFrame` → `RenderScene` → `EndFrame` reads from `Scene` and drives the GPU.

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
- `CreatePostProcessResources` — HDR target + two half-res bloom targets + RTV/SRV heaps.
- `CreatePostProcessPipelines` — bright-pass, blur-H, blur-V, tonemap PSOs.

**Frame lifecycle:**
```
BeginFrame()
  reset allocator/list
  barrier: back-buffer PRESENT → RENDER_TARGET (for resolve later)

RenderScene(scene, camera)
  [Shadow pass]         — depth-only to shadow map
  [MSAA main pass]      — geometry + instanced sprites + particles → MSAA color target
  [MSAA resolve]        — MSAA target → HDR target (R16G16B16A16_FLOAT)
  [Bloom bright-pass]   — HDR → half-res bright pixels
  [Bloom blur-H]        — bright → bloomA
  [Bloom blur-V]        — bloomA → bloomB
  [Tonemap composite]   — HDR + bloomB → back-buffer; applies scanlines/dither

EndFrame()
  barrier: back-buffer RENDER_TARGET → PRESENT
  close list, execute, present, MoveToNextFrame (fence sync)
```

**Double buffering:**
- `FrameCount = 2`. Each frame index has its own command allocator and fence value.
- `MoveToNextFrame` waits for the older frame's fence before reusing its allocator.

---

### Scene  (`Source/Scene.*`)
A flat container. No hierarchy, no spatial acceleration.

```cpp
std::vector<std::unique_ptr<Entity>> m_entities;
std::vector<SceneParticle>           m_particles;  // CPU-written each frame, uploaded to GPU
float                                m_time;       // seconds since startup, for GPU hover sin()
```

`SceneParticle` is 32 bytes: `float x,y,z,size,r,g,b,a`. The GPU particle buffer is a plain upload
buffer that `RenderScene` copies into before the particle draw call.

---

### Entity  (`Source/Entity.*`)
One renderable object. All fields are public — no setter overhead.

| Field | Purpose |
|---|---|
| `transform` | Position, rotation, scale — builds the world matrix |
| `mesh` | Shared vertex/index buffer |
| `material` | Shared PSO, root signature, texture SRV |
| `tint` | Per-entity RGBA multiplier |
| `enabled` | Skipped in the render loop if false |
| `isBillboardActor` | Cylindrical (Y-locked) billboard transform |
| `usesSpriteTexture` | Enables texture sampling path in pixel shader |
| `isBlobShadow` | Tells shadow pass to skip this entity |
| `useInstancing` | Drawn via the instanced sprite PSO instead of per-entity |
| `isUnlit` | Bypasses all lighting, fog, and shadow receive |
| `isBurning` | Marks entity as claimed by the BurningSprite system |
| `hoverPhase` | Per-entity random offset for GPU hover sin wave |
| `spriteUVRect` | Atlas rectangle (u0,v0,u1,v1) passed to shader |
| `velocity` | Used by Game for meteor physics |
| `animFrames` / `animTimer` | Sprite sheet animation (CPU-side frame selection) |

`Entity::Draw` uploads per-object root constants (world matrix, tint, renderParams) and emits a draw call. The shadow pass calls it with `worldOverride` (projected matrix) and `shadowPass=true`.

`renderParams` packing:

| Component | Shadow pass | Main pass |
|---|---|---|
| `.x` | `1.0` (shadow mode) | `1.0` if blob shadow entity |
| `.y` | `1.0` if sprite texture | `1.0` if sprite texture |
| `.z` | `0.0` | `1.0` if isUnlit |
| `.w` | unused | unused |

---

### Game  (`Source/Game.*`)
All gameplay and scene-management logic. Runs entirely on the CPU; writes results into `Scene`.

**Subsystems inside Game:**

| Subsystem | What it does |
|---|---|
| Camera orbit/pan/zoom | Spherical coordinates → eye position; right-mouse drag rotates, middle-mouse drag pans target, scroll zooms radius |
| Cinematic mode | Sine-wave animated azimuth, elevation, radius — overrides manual input |
| Targeting ring | Mouse ray cast to Y=-1 plane; scales a ring mesh entity to `kTargetRadius` |
| Meteor spawning | Left-click fires 3–5 meteors, scattered around the target position; each is a sphere entity with downward velocity |
| Explosion system | When a meteor hits Y=-1, sphere expands over time; sprites inside the scorch radius start burning |
| Fire particle emission | Each burning sprite and each active meteor emits CPU-side `FireParticle` structs each frame |
| Burning sprite system | `BurningSprite` lerps tint orange→dark-red, kills entity on burnout |
| Scene particle write | All live `FireParticle` structs are converted to `SceneParticle` and written to `scene.GetParticles()` |
| Bulk sprite instancing | 5 000 `Entity` objects with `useInstancing=true`; only animation frame is updated per-frame (Y-hover runs on GPU) |

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
`mat4.m[row*4 + col]`. Column vectors of the view matrix (right, up, back) live in rows, not columns:
```
m[0]=xAxis.x  m[1]=yAxis.x  m[2]=zAxis.x  m[3]=0
m[4]=xAxis.y  m[5]=yAxis.y  m[6]=zAxis.y  m[7]=0
...
```
When extracting camera basis in `Game.cpp` for ray casting:
```cpp
camRight = { viewMat.m[0], viewMat.m[4], viewMat.m[8]  };
camUp    = { viewMat.m[1], viewMat.m[5], viewMat.m[9]  };
camBack  = { viewMat.m[2], viewMat.m[6], viewMat.m[10] };
```

---

### Mesh  (`Source/Mesh.*`)
Creates vertex and index buffers as committed D3D12 resources. Exposes `Draw(commandList)`.
Vertex layout: `float3 Position, float3 Normal, float2 UV`.

Built-in mesh factories (anonymous namespace in `Game.cpp`):
- `CreateBoxMesh` — unit cube.
- `CreateGroundMesh` — large flat quad.
- `CreateSpriteMesh` — unit billboard quad.
- `CreateBlobMesh` — flat oval for blob shadows.
- `CreateSphereMesh` — UV sphere for explosions.
- `CreateTargetRingMesh` — 48-segment flat ring (inner=0.82, outer=1.0) for the targeting gizmo.

---

### Material  (`Source/Material.*`)
Wraps the hero-entity pipeline: root signature, PSO, WIC texture load, SRV.
The HLSL lives in `Source/Shaders/Material.hlsl` (external file, copied next to the exe at build time).
Compiled at runtime with `D3DCompile` so you can edit and relaunch without rebuilding C++.

---

## Why This Design is Good for Learning

- **Flat, explicit** — no engine abstractions hiding GPU calls. Every barrier, every bind, every upload is visible.
- **One file per concern** — follow one system at a time without hunting across an abstraction stack.
- **CPU-readable frame graph** — the entire render order lives in `DX12Context::RenderScene`, top to bottom.
- **Game logic separated from rendering** — `Game.cpp` is pure C++ math/logic; `DX12Context.cpp` is pure GPU.

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
- 5 000 sprite entities with `useInstancing=true`

**Problems solved:**
- PSO format mismatch: instanced sprite PSO had `RTVFormats[0] = R8G8B8A8_UNORM` but the MSAA
  target is `R16G16B16A16_FLOAT` — sprites rendered blank. Fixed by matching formats.
- Stride constant 96→112 in three places: buffer allocation, per-frame slot offset, and root SRV
  address. Missing any one of the three caused every sprite to read wrong data.

**Key lesson:**  
GPU instancing with a structured buffer SRV is simpler than an instanced vertex buffer when your
data is large. The vertex shader reads `g_instances[SV_InstanceID]` and the root SRV address is
just `bufferGpuAddress + frameIndex * maxInstances * stride`.

---

## Stage 6 — GPU Hover Animation

**What was built:**
- `hoverPhase` float on `Entity` (per-entity random offset, assigned at spawn)
- `vec4 hoverData` in `SpriteInstanceData` (`.x` = phase)
- `time` field added to `PerFrameCbData`, written from `Scene::GetTime()` each frame
- Vertex shader: `wp.y += sin(time * 1.45 + inst.hoverData.x) * 0.10`

**What was removed:**
- Per-frame CPU loop calling `SetPosition(Y)` on 5 000 entities — this was 5 000 function calls
  and matrix rebuilds per frame for a purely cosmetic effect

**Key lesson:**  
Any per-vertex or per-instance animation that depends only on time and a static per-instance value
belongs on the GPU, not the CPU. Moving it to the vertex shader costs essentially nothing and frees
the CPU entirely.

---

## Stage 7 — Shadow Map

**What was built:**
- 2048×2048 `m_shadowMap` resource (R32_TYPELESS, used as D32_FLOAT for rendering and R32_FLOAT SRV)
- Separate DSV heap for shadow map
- `m_shadowPso` — depth-only geometry pass
- `m_shadowSpriteRootSig` / `m_shadowSpritePso` — depth-only billboard pass
- Light view-projection matrix built in `RenderScene`
- Shadow map SRV bound at t2 in the geometry root signature
- PCF sampling in `Material.hlsl`: `SampleCmpLevelZero` with `LESS_EQUAL` comparison sampler
- Shadow coordinate projection in VS: `shadowCoord = mul(worldPos, lightViewProj)`

**Why this replaced the old projected shadow system:**  
Projected shadows only cast onto a flat plane. They cannot self-shadow, cannot cast onto walls, and
the projection matrix must be recomputed per caster per frame. A shadow map costs one extra draw
pass for the whole scene and then samples a single texture — far more general.

---

## Stage 8 — Targeting Ring and Click-to-Cast

**What was built:**
- `CreateTargetRingMesh` — 48-segment flat ring (inner=0.82, outer=1.0, Y=0 local)
- Mouse ray casting: unproject NDC through perspective frustum, intersect with Y=-1 ground plane
- `m_targetRing` entity with `isUnlit=true` (ring doesn't receive shadow or lighting)
- Left-click spawns 3–5 sphere-mesh meteors scattered within `kTargetRadius = 2.5` around the ring
- Meteors have downward velocity, physics updated in `Game::Update`

**Key lesson — view matrix basis extraction:**  
`MatrixLookAtRH` stores camera axes (right, up, back) as column vectors spread across the rows of
the 4×4 matrix. To extract them for ray casting:
```cpp
camRight = { view.m[0], view.m[4], view.m[8]  };  // xAxis
camUp    = { view.m[1], view.m[5], view.m[9]  };  // yAxis
camBack  = { view.m[2], view.m[6], view.m[10] };  // zAxis
```
Then the view-space ray direction for a screen pixel at NDC `(vx, vy)` is:
```cpp
rayDir = normalize(camRight * vx + camUp * vy - camBack)
```

---

## Stage 9 — VFX: Particle System and Burning Sprites

**What was built:**

*Particle pipeline:*
- `m_particlePso` — additive billboard particles, procedural soft-circle PS
- `SceneParticle` struct (32 bytes: position, size, RGBA)
- Double-buffered GPU upload buffer (`FrameCount × 8000 × 32` bytes)
- `scene.GetParticles()` — Game writes, DX12Context reads and uploads each frame

*Game-side particle logic:*
- `FireParticle` struct — CPU-side sim state (position, velocity, age, maxAge, startSize)
- Each active meteor emits 3 trail particles per frame
- Each burning sprite emits fire particles per frame
- Particles apply gravity-decel upward drift, age out, write to `SceneParticle` list

*Burning sprites:*
- `BurningSprite` struct — entity pointer, age, duration, original tint
- When an explosion reaches an entity within scorch radius, entity gets `isBurning=true`
- Tint lerps from original → orange → dark red over `duration`
- Entity is disabled at burnout

**Key lesson — additive blend particle depth:**  
With additive blend (`DestBlend=ONE`), depth write must be OFF. If it is ON, particle A writes
its depth value, and particle B behind A fails the depth test and is discarded, even though
additive particles are meant to accumulate regardless of order. Depth test stays ON so particles
correctly hide behind solid geometry.

---

## Stage 10 — Post-Processing: Bloom, Tonemap, Scanlines, Dithering

**What was built:**
- HDR intermediate target `m_hdrTarget` (R16G16B16A16_FLOAT)
- MSAA resolve pass: MSAA → HDR
- Half-resolution bloom targets `m_bloomA`, `m_bloomB`
- Bright-pass PSO: extract pixels above luminance threshold
- Blur-H and blur-V PSOs: separable 9-tap Gaussian
- Tonemap PSO: sample HDR + bloom, apply ACES filmic curve, output to back buffer
- Scanline toggle (`C` key): darkens every other row
- Bayer dithering toggle (`V` key): 4×4 ordered dither quantizes to 8 levels per channel
- FPS, frame time, entity count, draw call count passed to tonemap CB for HUD overlay

**Key lesson — HDR pipeline order:**  
All scene rendering (geometry, sprites, particles) happens in HDR space (R16G16B16A16_FLOAT values
can exceed 1.0). Additive particles can push pixel values well above 1.0 — this is intentional.
The tonemap pass compresses the result to [0,1] at the very end. Never clamp to [0,1] during
scene rendering; doing so loses the HDR information that bloom and tonemap need.

---

## Stage 11 — Camera Polish: Pan, Orbit Confinement

**What was built:**
- Middle-mouse drag: translate `m_camera.target` along camera-relative ground axes
- Cursor confinement (`ClipCursor`) during right-mouse drag — cursor can't leave the window
- Camera target persists between frames (removed the per-frame reset to `{0,0,-5}`)
- `m_camera.target` initialized in `Game::Init` to `{0,0,-5}`

**Key lesson:**  
Camera pan direction must use azimuth-derived basis vectors, not world axes. If you pan along
world X/Z, the pan direction changes relative to the view as the user orbits — it feels broken.
Correct:
```cpp
camRight     = { -sin(azimuth), 0,  cos(azimuth) };
camFwdGround = { -cos(azimuth), 0, -sin(azimuth) };
```
These always move the target in the direction the camera is facing regardless of orbit angle.

---

## Stage 12 — Fog Tuning and Unlit Flag

**What was built:**
- Fog start pushed from ~5 to 20, fog end from ~30 to 65 — scene is much more visible
- Geometry fog blend capped at `fogFactor × 0.7` — prevents geometry from washing out completely
- `isUnlit` flag on Entity → `renderParams.z = 1.0` → early-out in PS before any lighting
- Targeting ring uses `isUnlit=true` — always visible as a pure tint color, no fog or shadows

---

## Known Limitations

- Hero entity sprites do not receive shadow (shadow SRV is bound but the billboard path in
  `Material.hlsl` exits before the shadow sample when in sprite mode).
- No audio.
- No scene serialization — the scene is fully recreated from code each run.
- No frustum culling — all 5 000 instanced sprites are uploaded even if off-screen.
- Particle count resets to 0 each frame (fire particles re-added from `m_fireParticles` each
  frame). This is intentional — it avoids stale particles persisting when entities are removed.

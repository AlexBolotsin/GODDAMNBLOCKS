# Render Pipeline Walkthrough

Every frame executes the passes below in strict order. All GPU work goes through a single
`ID3D12GraphicsCommandList` that is closed, executed, and reset once per frame.

---

## Pass 0 — BeginFrame

**Code:** `DX12Context::BeginFrame`

1. Reset the current frame's command allocator and the command list.
2. Set the MSAA render target, clear color and depth.
3. Set viewport and scissor rect.

Nothing is drawn here. This just opens the command list and prepares the MSAA target.

---

## Pass 1 — Shadow Map

**Code:** `DX12Context::RenderScene` — shadow section  
**Output:** `m_shadowMap` — R32_TYPELESS resource used as D32_FLOAT DSV and R32_FLOAT SRV  
**Size:** 2048 × 2048

**What happens:**
1. Barrier: shadow map `PIXEL_SHADER_RESOURCE` → `DEPTH_WRITE`.
2. Set shadow DSV, clear depth to 1.0.
3. Set viewport and scissor to `kShadowMapSize × kShadowMapSize`.
4. Build light view-projection matrix (`m_lightViewProj`) from a directional light position above and to the side.
5. Iterate `world->renders` pool: for each entity with `castsProjectedShadow=true`, draw with `m_shadowPso` (depth-only geometry) or `m_shadowSpritePso` (depth-only billboard).
6. Barrier: shadow map `DEPTH_WRITE` → `PIXEL_SHADER_RESOURCE` (ready for main pass sampling).

**Why a separate shadow map instead of projected shadows?**  
Projected shadows only work for flat ground planes and can't self-shadow. A real shadow map lets any
geometry cast shadows onto any other geometry and is the industry standard.

---

## Pass 2 — MSAA Main Pass

**Code:** `DX12Context::RenderScene` — main pass section  
**Output:** `m_msaaColorTarget` — R32_TYPELESS / R16G16B16A16_FLOAT view, 8× MSAA  
**Depth:** `m_depthStencil` — D32_FLOAT, 8× MSAA

**Why MSAA?**  
Hard geometry edges without anti-aliasing look jagged, especially on moving sprites. 8× MSAA
resolves subpixel coverage by running the pixel shader once per pixel but testing coverage at 8
sub-pixel sample points.

**Sub-pass 2a — Geometry + non-instanced sprites**

Iterates `world->renders` pool. For each entity with `isInstanced=false`:
1. Set PSO and root signature from the entity's `RenderComp::material`.
2. Bind SRV heap, set per-frame CB at b0.
3. Upload `PerObjectData` (world matrix, tint, renderParams, spriteUVRect) as root constants at b1.
4. For billboards (`isBillboard=true`): world matrix is `MatrixBillboard(pos, scale, cameraEye)`.
5. `mesh->Draw(commandList)`.

`renderParams` packing:

| Component | Value |
|---|---|
| `.x` | `1.0` if blob shadow entity |
| `.y` | `1.0` if `usesSpriteTexture` |
| `.z` | `1.0` if `isUnlit` |
| `.w` | unused |

**Sub-pass 2b — Instanced Sprites**

Uses the instanced sprite PSO (`m_instancedSpritePso`).

1. Frustum-cull all entities with `isInstanced=true` using extracted VP planes.
2. Sort surviving entities by camera distance (front-to-back for early-Z).
3. Fill the mapped upload buffer with `SpriteInstanceData` per entity:
   - `float4x4 worldMatrix` — billboard transform
   - `float4 uvRect` — atlas region from `SpriteComp::uvRect`
   - `float4 tint` — from `RenderComp::tint`
   - `float4 hoverData` — `.x` = `SpriteComp::hoverPhase`
4. `DrawInstanced(6, instanceCount, 0, 0)` — 6 vertices (2 triangles), no index buffer.

Vertex shader computes hover in GPU:  
`worldPos.y += sin(time * 1.45 + inst.hoverData.x) * 0.10`

**Sub-pass 2c — Additive Particles**

`Game::Update` writes `SceneParticle` structs into `world->particles`. Each struct is 32 bytes:
`float x, y, z, size, r, g, b, a`.

1. `memcpy` particle data into the current frame's slot in `m_particleBuffer`.
2. `DrawInstanced(6, particleCount, 0, 0)`.

Blend state: additive (`DestBlend = ONE`). Depth test ON, depth write OFF.

**Shockwave CB pre-fill (end of main pass block)**

While `frameData.viewMatrix`, `frameData.projMatrix`, and `camera.eye` are in scope, the shockwave
constant buffer for the distort pass (Pass 5) is filled here. It stores camera basis vectors
(`cameraRight`, `cameraUp`, `cameraBack` from view matrix columns), projection parameters
(`proj00`, `proj11`), ground Y, and per-wave world-space data from `world->shockwaves`.
The actual draw happens later in `EndFrame`.

---

## Pass 3 — MSAA Resolve

**Code:** `DX12Context::RenderScene` — resolve section

1. Barrier: MSAA target `RENDER_TARGET` → `RESOLVE_SOURCE`.
2. Barrier: HDR target `PIXEL_SHADER_RESOURCE` → `RESOLVE_DEST`.
3. `ResolveSubresource(m_hdrTarget, m_msaaColorTarget, DXGI_FORMAT_R16G16B16A16_FLOAT)`.
4. Barrier: MSAA target `RESOLVE_SOURCE` → `RENDER_TARGET`.
5. Barrier: HDR target `RESOLVE_DEST` → `PIXEL_SHADER_RESOURCE`.

Restores shadow map to `DEPTH_WRITE` at end of `RenderScene` for next frame.

---

## Pass 4 — Bloom Bright Pass

**Code:** `DX12Context::EndFrame`  
**Input:** HDR target (SRV)  
**Output:** `m_bloomA` — half-resolution R16G16B16A16_FLOAT

Fullscreen triangle shader: extracts pixels brighter than luminance ~1.0, writes black otherwise.
Working at half resolution makes the blur 4× cheaper.

---

## Pass 5 — Bloom Blur H and Blur V

**Input/Output:** `m_bloomA` ↔ `m_bloomB`

Separable 9-tap Gaussian blur (horizontal then vertical). Result: `m_bloomA` holds the blurred bloom.

---

## Pass 6 — Shockwave Air Distortion

**Code:** `DX12Context::EndFrame` — distort section  
**Input:** `m_hdrTarget` (SRV at t0)  
**Output:** `m_distortTarget` — full-resolution R16G16B16A16_FLOAT  
**CB:** `m_shockwaveCb[frameIndex]` — filled in `RenderScene` (Pass 2)

**What happens:**
1. Barrier: `m_distortTarget` `PIXEL_SHADER_RESOURCE` → `RENDER_TARGET`.
2. Bind distort PSO (`m_distortPso`) and root signature.
3. Set shockwave CB at b0 (root CBV), set HDR SRV at t0 (descriptor table).
4. `DrawInstanced(3, 1, 0, 0)` — one fullscreen triangle.
5. Barrier: `m_distortTarget` `RENDER_TARGET` → `PIXEL_SHADER_RESOURCE`.

**Why not screen-space UV rings?**  
A ring computed purely in UV space is always a perfect circle on screen regardless of camera angle.
A real shockwave on the ground should look like an ellipse when viewed obliquely — the foreshortening
of the ground plane must be respected.

**How the shader produces a ground-aligned ring:**

For each fragment:
1. Convert UV to NDC and build a world-space view ray:
   ```hlsl
   float3 rayDir = normalize(
       cameraRight * (ndc.x / proj00) +
       cameraUp    * (ndc.y / proj11) -
       cameraBack
   );
   ```
2. Early-out if `rayDir.y >= 0` (pixel is above the horizon — sky pixels cannot show the ring).
3. Intersect with the ground plane: `t = (groundY - cameraEye.y) / rayDir.y`, `worldHit = eye + t*rayDir`.
4. Measure world-space distance from `worldHit.xz` to the explosion center.
5. Ring function: `ring = (1 - |dist - worldRadius| / kRingWidth)²` — smooth falloff on both sides.
6. Project the outward world direction into screen UV space via the camera basis to get the UV offset direction.
7. Accumulate `uvDir * ring * strength` across all active waves.
8. Sample HDR at `uv + totalOffset`.

---

## Pass 7 — Tonemap + Composite → Back Buffer

**Input:** `m_distortTarget` (t0) + `m_bloomA` (t1)  
**Output:** Back buffer (LDR, R8G8B8A8_UNORM)

1. Samples the distorted scene color (not raw HDR — distortion is baked in).
2. Adds the bloom contribution.
3. Applies ACES filmic tone mapping.
4. Optionally applies scanline darkening (`C` key) and Bayer 4×4 dithering (`V` key).
5. Outputs FPS/frame-time/entity/draw-call HUD via text quads.

---

## Pass 8 — EndFrame Sync

1. Barrier: back buffer `RENDER_TARGET` → `PRESENT`.
2. Close and execute command list.
3. `Present(1, 0)` — vsync.
4. `MoveToNextFrame` — signal fence, advance frame index, wait for older frame if needed.

---

## Resource State Summary

| Resource | Start-of-frame state | Used as | End-of-frame state |
|---|---|---|---|
| Back buffer | PRESENT | RENDER_TARGET (tonemap) | PRESENT |
| MSAA color | RENDER_TARGET | RENDER_TARGET, RESOLVE_SOURCE | RENDER_TARGET |
| Depth | DEPTH_WRITE | DEPTH_WRITE | DEPTH_WRITE |
| Shadow map | PIXEL_SHADER_RESOURCE | DEPTH_WRITE → PSR | PIXEL_SHADER_RESOURCE |
| HDR target | PIXEL_SHADER_RESOURCE | RESOLVE_DEST → PSR (bloom + distort input) | PIXEL_SHADER_RESOURCE |
| bloomA | PIXEL_SHADER_RESOURCE | RTV (bright) → RTV (blurV) → PSR | PIXEL_SHADER_RESOURCE |
| bloomB | PIXEL_SHADER_RESOURCE | RTV (blurH) → PSR | PIXEL_SHADER_RESOURCE |
| distortTarget | PIXEL_SHADER_RESOURCE | RTV (distort) → PSR (tonemap input) | PIXEL_SHADER_RESOURCE |

Every transition is an explicit `D3D12_RESOURCE_BARRIER` of type `TRANSITION`.

---

## Important GPU State Rules

- **Depth test:** `LESS`, front face `CCW` (right-handed convention).
- **MSAA:** All main-pass draw calls must target `m_msaaColorTarget`, not the back buffer.
  The back buffer is only written by the tonemap pass.
- **PSO format must match render target format:** The instanced sprite PSO uses `R16G16B16A16_FLOAT`
  to match the MSAA target.
- **Particle depth-write off:** Particles use additive blend. With depth-write on, the first
  particle would block all particles behind it.
- **Tonemap reads distort, not raw HDR:** The distort pass sits between bloom and tonemap. The tonemap
  SRV table points at `distortTarget` (slot 3 in the post SRV heap), not `m_hdrTarget` (slot 0).

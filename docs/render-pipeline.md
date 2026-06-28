# Render Pipeline Walkthrough

Every frame executes the passes below in strict order. All GPU work goes through a single
`ID3D12GraphicsCommandList` that is closed, executed, and reset once per frame.

---

## Pass 0 — BeginFrame

**Code:** `DX12Context::BeginFrame`

1. Reset the current frame's command allocator and the command list.
2. Barrier: back buffer `PRESENT` → `RENDER_TARGET` (needed for the tonemap composite at the end).

Nothing is drawn here. This just opens the command list and makes the back buffer writable.

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
5. For each entity: draw with `m_shadowPso` (depth-only, no pixel shader output).
6. For each instanced sprite batch: draw with `m_shadowSpritePso`.
7. Barrier: shadow map `DEPTH_WRITE` → `PIXEL_SHADER_RESOURCE` (ready for main pass sampling).

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

**Sub-pass 2a — Geometry (hero entities)**

Uses `Material.hlsl` (PSO from `Material::CreatePipeline`).

For each non-instanced, non-blob-shadow entity with a mesh and material:
1. Set PSO and root signature.
2. Upload per-frame constants (view, proj, camera eye, light view-proj, shadow map SRV, time).
3. Upload per-object constants (world matrix, tint, renderParams).
4. Bind the material's SRV heap (sprite texture at t1, shadow map at t2).
5. `mesh->Draw(commandList)`.

**Sub-pass 2b — Instanced Sprites**

Uses the instanced sprite PSO (`m_instancedSpritePso`).

1. Sort all `useInstancing` entities by camera distance (back-to-front for correct alpha).
2. Fill the mapped upload buffer with `SpriteInstanceData` per entity:
   - `float4x4 worldMatrix` — billboard transform
   - `float4 uvRect` — atlas region
   - `float4 tint`
   - `float4 hoverData` — `.x` = `hoverPhase` (per-entity random, used in vertex shader)
3. Upload per-frame constants (same CB as geometry pass).
4. Set root SRV to the instance buffer for this frame (double-buffered by frame index).
5. `DrawInstanced(6, instanceCount, 0, 0)` — 6 vertices (2 triangles), no index buffer, instance ID selects the data row.

Vertex shader computes hover in GPU:  
`worldPos.y += sin(time * 1.45 + inst.hoverData.x) * 0.10`  
This replaces the old CPU loop that called `SetPosition` on 5 000 entities per frame.

**Sub-pass 2c — Additive Particles**

Uses the particle PSO (`m_particlePso`).

`Game::Update` writes `SceneParticle` structs into `scene.GetParticles()`. Each struct is 32 bytes:
`float x, y, z, size, r, g, b, a`.

1. `memcpy` particle data into the current frame's slot in `m_particleBuffer` (double-buffered upload buffer, `FrameCount × 8000 × 32` bytes).
2. Set PSO and root signature.
3. Set per-frame CB at b0.
4. Set particle buffer as root SRV at t0.
5. `DrawInstanced(6, particleCount, 0, 0)`.

Vertex shader expands each particle into a camera-facing billboard quad using the camera right/up vectors extracted from the view matrix. Pixel shader computes a soft circle and blends hot-center (bright white/yellow) with cooler edge (orange/red).

Blend state: `SrcBlend = SRC_ALPHA, DestBlend = ONE` — additive. Depth test ON, depth write OFF
(particles occlude behind geometry but don't occlude each other).

---

## Pass 3 — MSAA Resolve

**Code:** `DX12Context::RenderScene` — resolve section

1. Barrier: MSAA target `RENDER_TARGET` → `RESOLVE_SOURCE`.
2. Barrier: HDR target `PIXEL_SHADER_RESOURCE` → `RESOLVE_DEST`.
3. `ResolveSubresource(m_hdrTarget, m_msaaColorTarget, DXGI_FORMAT_R16G16B16A16_FLOAT)` — averages the 8 MSAA samples per pixel into a single HDR pixel.
4. Barrier: MSAA target `RESOLVE_SOURCE` → `RENDER_TARGET` (reset for next frame).
5. Barrier: HDR target `RESOLVE_DEST` → `PIXEL_SHADER_RESOURCE` (ready for bloom).

The MSAA color target uses `R32_TYPELESS` so it can be interpreted as both `R16G16B16A16_FLOAT`
(for rendering) and passed to `ResolveSubresource`. The resolved HDR target is a plain
`R16G16B16A16_FLOAT` resource.

---

## Pass 4 — Bloom Bright Pass

**Code:** `DX12Context::RenderScene` — post-process section  
**Input:** HDR target (SRV)  
**Output:** `m_bloomA` — half-resolution R16G16B16A16_FLOAT

A fullscreen triangle shader extracts pixels brighter than a threshold (luminance > ~1.0 in HDR
space) and writes them at half resolution. Pixels below threshold write black.

This is the input to the blur passes. By working at half resolution the blur is 4× cheaper.

---

## Pass 5 — Bloom Blur H and Blur V

**Code:** two sequential fullscreen passes  
**Input/Output:** `m_bloomA` ↔ `m_bloomB`

A separable 9-tap Gaussian blur kernel. Running horizontal then vertical gives a 2D Gaussian in
two cheap 1D passes (9+9 samples instead of 81).

Blur H: `m_bloomA` (SRV) → `m_bloomB` (RTV)  
Blur V: `m_bloomB` (SRV) → `m_bloomA` (RTV)

Result: `m_bloomA` holds the blurred bloom contribution.

---

## Pass 6 — Tonemap + Composite → Back Buffer

**Code:** `DX12Context::RenderScene` — tonemap section  
**Input:** HDR target + bloom (`m_bloomA`)  
**Output:** Back buffer (LDR, sRGB)

The tonemap PSO runs a fullscreen triangle that:
1. Samples the HDR scene color.
2. Adds the bloom contribution (scaled by a bloom strength constant).
3. Applies ACES filmic tone mapping to compress HDR to [0,1].
4. Optionally applies scanline darkening (every other row at ~15% strength) — toggled with `C`.
5. Optionally applies Bayer 4×4 ordered dithering to 8 levels per channel — toggled with `V`.

Scanlines and dithering are passed as float flags in the post-process constant buffer alongside FPS,
frame time, entity count, and draw call count (displayed in the HUD overlay).

---

## Pass 7 — EndFrame

**Code:** `DX12Context::EndFrame`

1. Barrier: back buffer `RENDER_TARGET` → `PRESENT`.
2. Close the command list.
3. Execute on the command queue.
4. `m_swapChain->Present(1, 0)` — present with vsync.
5. `MoveToNextFrame` — signal fence for this frame, advance frame index, wait for the older frame's fence if it hasn't completed yet.

---

## Resource State Summary

| Resource | Start-of-frame state | Used as | End-of-frame state |
|---|---|---|---|
| Back buffer | PRESENT | RENDER_TARGET (tonemap) | PRESENT |
| MSAA color | RENDER_TARGET | RENDER_TARGET, RESOLVE_SOURCE | RENDER_TARGET |
| Depth | DEPTH_WRITE | DEPTH_WRITE | DEPTH_WRITE |
| Shadow map | PIXEL_SHADER_RESOURCE | DEPTH_WRITE → PSR | PIXEL_SHADER_RESOURCE |
| HDR target | PIXEL_SHADER_RESOURCE | RESOLVE_DEST → PSR (bloom input) | PIXEL_SHADER_RESOURCE |
| bloomA | PIXEL_SHADER_RESOURCE | RTV (bright) → RTV (blurV) → PSR | PIXEL_SHADER_RESOURCE |
| bloomB | PIXEL_SHADER_RESOURCE | RTV (blurH) → PSR | PIXEL_SHADER_RESOURCE |

Every transition is an explicit `D3D12_RESOURCE_BARRIER` of type `TRANSITION`. Missing a barrier
causes the GPU validation layer to report an error, or silently corrupt output on release hardware.

---

## Important GPU State Rules

- **Depth test:** `LESS`, front face `CCW` (right-handed convention).
- **MSAA:** All main-pass draw calls must target `m_msaaColorTarget`, not the back buffer directly.
  The back buffer is only written by the tonemap pass.
- **PSO format must match render target format:** The instanced sprite PSO uses `R16G16B16A16_FLOAT`
  to match the MSAA target. A mismatch silently produces blank output on some GPUs and a D3D12
  debug error on others.
- **Particle depth-write off:** Particles use additive blend. With depth-write on, the first
  particle would block all particles behind it. Depth-test stays on so particles hide behind walls.

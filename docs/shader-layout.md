# Shader and Root Signature Layout

This engine has five rendering pipelines. Each has its own root signature.
The golden rule: **C++ struct layout and HLSL cbuffer field order must match exactly.**
Any mismatch silently corrupts output or causes a D3D12 validation error.

---

## Pipeline 1 — Main Geometry (Material.hlsl)

Used for: hero entities, ground plane, explosion spheres, targeting ring.

### Root Signature

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV | b0 | PerFrame constants |
| 1 | Root CBV | b1 | PerObject constants |
| 2 | Descriptor table | t1, t2 | Sprite texture SRV + shadow map SRV |

Static samplers: `s0` POINT (sprite), `s1` comparison LESS_EQUAL (shadow PCF).

### PerFrame CB (b0) — `PerFrameCbData` in DX12Context.cpp

```cpp
struct PerFrameCbData {
    mat4  viewMatrix;          // 64 bytes
    mat4  projMatrix;          // 64 bytes
    mat4  lightViewProjMatrix; // 64 bytes
    vec3  cameraEyeWS;         // 12 bytes
    float time;                // 4 bytes  — seconds, drives GPU hover sin()
    // 8 bytes padding to 256-byte CB alignment
};
```

### PerObject CB (b1) — uploaded per draw call

```cpp
struct PerObjectCbData {
    mat4 worldMatrix;    // 64 bytes
    vec4 tintColor;      // 16 bytes
    vec4 renderParams;   // 16 bytes
};
```

`renderParams` packing:

| Component | Shadow pass | Main pass |
|---|---|---|
| `.x` | `1.0` (shadow mode flag) | `1.0` if blob shadow |
| `.y` | `1.0` if sprite texture | `1.0` if sprite texture |
| `.z` | `0.0` | `1.0` if `isUnlit` |
| `.w` | unused | unused |

### VS (VSMain) — Material.hlsl

```hlsl
float4 clipPos = mul(mul(mul(float4(pos,1), worldMatrix), viewMatrix), projMatrix);
```

Outputs to PS: `worldPos`, `worldNormal`, `uv`, `clipYW` (for fog), `shadowCoord` (projected into light space).

### PS (PSMain) — Material.hlsl

Branch tree:

```
if renderParams.x > 0.5  →  shadow mode: dark translucent output, fog-aware, early return
if renderParams.z > 0.5  →  unlit mode: return rgb directly, no lighting/fog, early return
if renderParams.y > 0.5  →  sprite mode: sample texture, clip on alpha < 0.1, apply fog
else                     →  geometry mode: full lighting pipeline
```

**Geometry lighting (in order):**
1. Sample shadow map with PCF (`SampleCmpLevelZero`) — `shadow ∈ [0,1]`
2. Key light (directional, warm): `max(0, dot(N, L)) × shadowFactor`
3. Fill light (directional, cool, opposite side): `max(0, dot(N, Lfill))`
4. Ambient: flat constant
5. Edge darkening: `pow(1 - NdotV, 3)` darkens silhouettes
6. Floor-only detail: checker pattern + normal perturbation
7. Sky-gradient fog: `fogT = (camDist - 20) / (65 - 20)`, blend toward horizon/zenith color,
   geometry blend capped at `fogT × 0.7` to preserve some geometry through fog

---

## Pipeline 2 — GPU-Instanced Sprites

Used for: 5 000 bulk sprite entities (`useInstancing = true`).

### Root Signature (instanced sprite)

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV | b0 | PerFrame CB (same struct as geometry) |
| 1 | Descriptor table | t1 | Sprite atlas texture SRV |
| 2 | Root SRV | t0 | Instance data buffer (structured) |

Static sampler: `s0` POINT.

### SpriteInstanceData — C++ struct (112 bytes per instance)

```cpp
struct SpriteInstanceData {
    mat4 worldMatrix;  // 64 bytes — billboard world matrix
    vec4 uvRect;       // 16 bytes — atlas region (u0,v0,u1,v1)
    vec4 tint;         // 16 bytes
    vec4 hoverData;    // 16 bytes — .x = hoverPhase
};
```

Stride: `112` bytes. Three places in `DX12Context.cpp` must agree on this value:
buffer allocation size, per-frame slot offset, and root SRV address calculation.

### HLSL Struct (must match above field-for-field)

```hlsl
struct SpriteInstance {
    float4x4 world;
    float4   uvRect;
    float4   tint;
    float4   hoverData;
};
StructuredBuffer<SpriteInstance> g_instances : register(t0);
```

### VS

```hlsl
SpriteInstance inst = g_instances[instanceId];
float4 wp = mul(float4(localPos, 1), inst.world);
wp.y += sin(time * 1.45 + inst.hoverData.x) * 0.10;  // GPU hover
float4 vp = mul(mul(wp, viewMatrix), projMatrix);
```

### PS

```hlsl
float4 col = g_sprite.Sample(g_smp, uv) * inst.tint;
clip(col.a - 0.1);                   // alpha clip
// sky-gradient fog blend
// base ambient occlusion (bottom darkening)
// shadow receive (PCF sample)
return col;
```

---

## Pipeline 3 — Additive Particles

Used for: fire trail, burning sprite VFX.

### Root Signature (particle)

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV (ALL) | b0 | PerFrame CB |
| 1 | Root SRV (VERTEX) | t0 | Particle data buffer |

No texture, no static sampler — particles are procedural soft circles.

### Particle Data Layout — 32 bytes per particle

```cpp
struct SceneParticle {
    float x, y, z;  // world position
    float size;      // world-space radius
    float r, g, b, a;
};
```

This is what `Game.cpp` fills in `scene.GetParticles()`. It is `memcpy`'d directly to the GPU
upload buffer — the HLSL `ParticleData` struct must match exactly.

### VS

Reads `ParticleData` via `StructuredBuffer<ParticleData> g_particles : register(t0)`.
Expands each particle into a camera-facing quad using camera right/up extracted from the view matrix:

```hlsl
float3 camRight = float3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
float3 camUp    = float3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
float3 wp = center + (camRight * offset.x + camUp * offset.y) * p.size;
```

### PS

```hlsl
float2 uv  = input.localUV * 2 - 1;       // [-1,1]
float  r2  = dot(uv, uv);
clip(1 - r2);                              // discard outside circle
float  edge = 1 - r2;
float3 hot  = float3(1.4, 1.1, 0.3);      // bright yellow center
float3 cool = float3(0.9, 0.2, 0.05);     // orange-red edge
float3 col  = lerp(cool, hot, edge * edge) * p.color.rgb;
return float4(col * p.color.a, p.color.a);
```

Blend: additive (`DestBlend = ONE`). Depth test ON, depth write OFF.

---

## Pipelines 4 & 5 — Shadow (Depth Only)

Two depth-only PSOs:
- `m_shadowPso` — for geometry (reads world matrix from root constants).
- `m_shadowSpritePso` — for hero billboard sprites (cylindrical billboard in VS).

Both write only to the depth buffer. No pixel shader. `DepthBias` is set to reduce acne.

---

## Pipelines 6–9 — Post-Process (Fullscreen Triangles)

All use the same pattern: no vertex buffer, `DrawInstanced(3,1,0,0)`, VS generates positions
from `vertexId` into a fullscreen triangle. SRVs bound via descriptor table.

| PSO | Input SRV | Output RTV | What it does |
|---|---|---|---|
| `m_brightPassPso` | HDR target | bloomA (half-res) | Extract bright pixels |
| `m_blurHPso` | bloomA | bloomB | 9-tap horizontal Gaussian |
| `m_blurVPso` | bloomB | bloomA | 9-tap vertical Gaussian |
| `m_tonemapPso` | HDR + bloomA | back buffer | ACES tonemap + composite + scanlines/dither |

### Post-Process CB

```hlsl
cbuffer PostCB : register(b0) {
    float2 texelSize;
    float2 _pad;
    float  fps;
    float  scanlinesEnabled;
    float  ditherEnabled;
    float  frameTimeMs;
    float  entityCount;
    float  drawCallCount;
    float2 _pad2;
};
```

---

## Common Mistakes

| Mistake | Symptom |
|---|---|
| C++ struct size ≠ HLSL cbuffer size | Corrupt constants, wrong colors/transforms |
| Sprite PSO `RTVFormats[0]` wrong | Blank sprite output (no error on some hardware) |
| Instanced sprite stride wrong (not 112) | Every sprite uses wrong instance slot — all show instance 0 or garbage |
| Missing barrier before shadow map SRV use | GPU validation error or corrupted shadows |
| Root SRV slot offset not multiplied by frame index | Both frames share same particle/instance data — tearing artifact |
| Particle depth-write ON | Particles block each other based on draw order — opaque-looking fire |
| `Num32BitValues` in `SetGraphicsRoot32BitConstants` not matching struct field count | First N fields correct, rest are zero or previous frame's data |

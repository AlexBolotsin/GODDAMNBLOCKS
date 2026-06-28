# Shader and Root Signature Layout

This engine has seven rendering pipelines. Each has its own root signature.
The golden rule: **C++ struct layout and HLSL cbuffer field order must match exactly.**
Any mismatch silently corrupts output or causes a D3D12 validation error.

---

## Pipeline 1 — Main Geometry (Material.hlsl)

Used for: hero entities, ground plane, explosion spheres, targeting ring, blob shadows.

### Root Signature

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV | b0 | PerFrame constants |
| 1 | Root 32-bit constants | b1 | PerObject constants (28 DWORDs) |
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

Double-buffered: `m_perFrameCb` is 2×256 bytes; each frame writes to `256 * frameIndex` offset.

### PerObject constants (b1) — uploaded per draw call via `SetGraphicsRoot32BitConstants`

```cpp
struct PerObjectData {
    mat4 worldMatrix;  // 64 bytes (16 floats)
    vec4 color;        // 16 bytes  — tint RGBA
    vec4 renderParams; // 16 bytes
    vec4 spriteUVRect; // 16 bytes  — atlas sub-rect (u0,v0,u1,v1)
};                     // 112 bytes total = 28 DWORDs
```

`renderParams` packing:

| Component | Value |
|---|---|
| `.x` | `1.0` if blob shadow |
| `.y` | `1.0` if `usesSpriteTexture` |
| `.z` | `1.0` if `isUnlit` |
| `.w` | unused |

### VS (VSMain) — Material.hlsl

```hlsl
float4 clipPos = mul(mul(mul(float4(pos,1), worldMatrix), viewMatrix), projMatrix);
```

Outputs to PS: `worldPos`, `worldNormal`, `uv`, `clipYW` (for fog), `shadowCoord`.

### PS (PSMain) — Material.hlsl

Branch tree:

```
if renderParams.x > 0.5  →  blob shadow mode: dark translucent, fog-aware, early return
if renderParams.z > 0.5  →  unlit mode: return rgb directly, no lighting/fog, early return
if renderParams.y > 0.5  →  sprite mode: sample texture, alpha clip, apply fog
else                     →  geometry mode: full lighting pipeline
```

**Geometry lighting (in order):**
1. Shadow map PCF (`SampleCmpLevelZero`) — `shadow ∈ [0,1]`
2. Key light (directional, warm): `max(0, dot(N, L)) × shadow`
3. Fill light (directional, cool, opposite side)
4. Ambient: flat constant
5. Edge darkening: `pow(1 - NdotV, 3)`
6. Floor-only detail: checker pattern + normal perturbation
7. Sky-gradient fog: blend toward horizon/zenith color based on `clipY/clipW`

---

## Pipeline 2 — GPU-Instanced Sprites

Used for: 5 000 bulk sprite entities (`isInstanced = true`).

### Root Signature

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV | b0 | PerFrame CB (same struct as geometry) |
| 1 | Root SRV | t0 | Instance data buffer (structured) |
| 2 | Descriptor table | t1 | Sprite atlas texture SRV |

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
SpriteInstance inst = g_instances[SV_InstanceID];
float4 wp = mul(float4(localPos, 1), inst.world);
wp.y += sin(time * 1.45 + inst.hoverData.x) * 0.10;  // GPU hover
float4 vp = mul(mul(wp, viewMatrix), projMatrix);
```

### PS

```hlsl
float4 col = g_sprite.Sample(g_smp, uv) * inst.tint;
clip(col.a - 0.1);
// sky-gradient fog
// base ambient occlusion (bottom darkening)
// shadow map receive (PCF)
return col;
```

---

## Pipeline 3 — Additive Particles

Used for: fire trail, burning sprite VFX.

### Root Signature

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

Filled by `Game.cpp` into `world->particles`, `memcpy`'d to the GPU upload buffer each frame.
HLSL `ParticleData` must match exactly.

### VS

Reads `ParticleData` via `StructuredBuffer<ParticleData> g_particles : register(t0)`.
Expands each particle into a camera-facing quad using camera right/up from the view matrix:

```hlsl
float3 camRight = float3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
float3 camUp    = float3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
float3 wp = center + (camRight * offset.x + camUp * offset.y) * p.size;
```

### PS

```hlsl
float2 uv  = input.localUV * 2 - 1;
float  r2  = dot(uv, uv);
clip(1 - r2);                         // discard outside circle
float  edge = 1 - r2;
float3 hot  = float3(1.4, 1.1, 0.3); // bright yellow center
float3 cool = float3(0.9, 0.2, 0.05);
float3 col  = lerp(cool, hot, edge * edge) * p.color.rgb;
return float4(col * p.color.a, p.color.a);
```

Blend: additive (`DestBlend = ONE`). Depth test ON, depth write OFF.

---

## Pipelines 4 & 5 — Shadow (Depth Only)

Two depth-only PSOs:
- `m_shadowPso` — geometry: world matrix from root constants.
- `m_shadowSpritePso` — hero billboard sprites: cylindrical billboard in VS.

Both write only to the depth buffer. No pixel shader.

---

## Pipeline 6 — Shockwave Air Distortion

Used for: post-process fullscreen distortion ring over live explosion shockwaves.

### Root Signature

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV (PIXEL) | b0 | ShockwaveCB (256 bytes) |
| 1 | Descriptor table (PIXEL) | t0 | HDR scene color SRV |

Static sampler: `s0` LINEAR CLAMP.

### ShockwaveCB (b0) — 64 floats = 256 bytes

```
Offset  Field             Type      Notes
[0]     waveCount         float     number of active waves (≤ 8)
[1]     proj00            float     projMatrix.m[0] = cot(fovY/2)/aspect
[2]     proj11            float     projMatrix.m[5] = cot(fovY/2)
[3]     groundY           float     world-space Y of the ground plane (-1.0)
[4-6]   cameraEye.xyz     float3    camera world position
[7]     pad
[8-10]  cameraRight.xyz   float3    view matrix col 0
[11]    pad
[12-14] cameraUp.xyz      float3    view matrix col 1
[15]    pad
[16-18] cameraBack.xyz    float3    view matrix col 2  (normalize(eye - target))
[19]    pad
[20-51] waves[8]          float4×8  .x=worldX .y=worldZ .z=worldRadius(m) .w=uvStrength
[52-63] pad
```

Filled in `RenderScene` (where `frameData` and `camera` are in scope). Camera basis is extracted
from the view matrix columns: `cameraRight = (vm[0], vm[4], vm[8])`, etc. (row-major `m[row*4+col]`).

### Shader logic

```hlsl
// Reconstruct world-space ray
float2 ndc    = float2(uv.x*2-1, 1-uv.y*2);
float3 rayDir = normalize(cameraRight*(ndc.x/proj00) + cameraUp*(ndc.y/proj11) - cameraBack);

if (rayDir.y < -0.001f)  // pixel hits the ground plane
{
    float  t_hit    = (groundY - cameraEye.y) / rayDir.y;
    float3 worldHit = cameraEye + rayDir * t_hit;

    for each wave:
        float2 delta      = worldHit.xz - wave.xy;
        float  dist       = length(delta);
        float  ring       = (1 - |dist - worldRadius| / 0.75)²;

        // UV offset direction: project world outward into screen space
        float3 worldOut = normalize(float3(delta.x, 0, delta.y));
        float2 uvDir    = normalize(float2(dot(worldOut, cameraRight),
                                          -dot(worldOut, cameraUp)));
        totalOffset    += uvDir * ring * strength;
}

return hdrInput.Sample(linearSmp, uv + totalOffset).rgb;
```

The ring is computed in world-space metres, so it automatically foreshortens correctly at any
camera angle. Sky pixels (where `rayDir.y >= 0`) receive no distortion.

---

## Pipelines 7–10 — Post-Process (Fullscreen Triangles)

All use the same pattern: no vertex buffer, `DrawInstanced(3,1,0,0)`, VS generates positions
from `SV_VertexID` into a fullscreen triangle. SRVs bound via descriptor table.

| PSO | Input SRV | Output RTV | What it does |
|---|---|---|---|
| `m_brightPassPso` | HDR target | bloomA (half-res) | Extract bright pixels |
| `m_blurHPso` | bloomA | bloomB | 9-tap horizontal Gaussian |
| `m_blurVPso` | bloomB | bloomA | 9-tap vertical Gaussian |
| `m_tonemapPso` | distortTarget + bloomA | back buffer | ACES tonemap + composite + scanlines/dither |

Note: the tonemap reads `distortTarget` (not raw HDR) — the distortion is baked in before tonemap.

### Post-Process Root Signature

| Slot | Type | Register | Content |
|---|---|---|---|
| 0 | Root CBV (ALL) | b0 | PostCB |
| 1 | Descriptor table (PIXEL) | t0, t1 | Two SRVs (varies per pass) |

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
| Instanced sprite stride wrong (not 112) | Every sprite uses wrong instance slot |
| Missing barrier before shadow map SRV use | GPU validation error or corrupted shadows |
| Root SRV slot offset not multiplied by frame index | Both frames share same particle/instance data |
| Particle depth-write ON | Particles block each other — opaque-looking fire |
| Tonemap SRV pointing at HDR instead of distortTarget | Distortion pass has no visible effect |
| ShockwaveCB camera basis extracted from wrong matrix rows/cols | Ring direction rotates incorrectly with camera |
| `worldRadius` passed as normalized (0–1) instead of metres | Ring size scales with window aspect ratio, not world geometry |

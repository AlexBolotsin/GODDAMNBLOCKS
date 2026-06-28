# Learning Roadmap

## What Has Already Been Built

This is not a tutorial engine — it is a working renderer with real features. Before planning what to
learn next, understand what is already here:

| Feature | Where |
|---|---|
| D3D12 device, swap chain, command queue/list, fences | `DX12Context.cpp` |
| Double-buffered frame pipeline | `DX12Context.cpp` |
| 8× MSAA with R32_TYPELESS target | `DX12Context.cpp` |
| D32_FLOAT depth buffer | `DX12Context.cpp` |
| Shadow map — 2048×2048 depth-only pass | `DX12Context.cpp`, `Material.hlsl` |
| PCF shadow sampling in pixel shader | `Material.hlsl` |
| Forward-lit geometry (key + fill + ambient) | `Material.hlsl` |
| Sky-gradient fog with distance blend | `Material.hlsl` |
| Billboard sprite rendering with atlas UVs | `Game.cpp`, `Material.hlsl` |
| GPU-instanced sprites (5 000, no CPU loop) | `DX12Context.cpp` |
| GPU hover animation via vertex shader sin() | Instanced sprite shader in `DX12Context.cpp` |
| Per-sprite shadow receive and base AO | Instanced sprite shader |
| Additive billboard particle system (8 000 max) | `DX12Context.cpp`, `Game.cpp` |
| HDR render target (R16G16B16A16_FLOAT) | `DX12Context.cpp` |
| Bloom (bright-pass → separable Gaussian blur) | `DX12Context.cpp` |
| ACES filmic tone mapping | `DX12Context.cpp` |
| Scanline post effect | `DX12Context.cpp` |
| Bayer 4×4 ordered dithering | `DX12Context.cpp` |
| Shockwave air distortion — world-space ray-plane ring | `DX12Context.cpp` |
| Orbit / pan / zoom camera (spherical coords) | `Game.cpp` |
| Mouse ray cast to ground plane | `Game.cpp` |
| Targeting ring gizmo (unlit, scaled to scatter radius) | `Game.cpp` |
| Meteor spawning and physics | `Game.cpp` |
| Explosion VFX (expanding sphere, scorch zone) | `Game.cpp` |
| Fire particle trail on meteors and burning sprites | `Game.cpp` |
| Burning sprite system (tint shift → ragdoll → death) | `Game.cpp` |
| Cinematic auto-orbit mode | `Game.cpp` |
| ECS: EntityID + ComponentPool\<T\> (sparse-set) | `ECS.h`, `Components.h`, `World.h` |
| WIC texture loading (PNG) | `Material.cpp` |
| Runtime HLSL compilation (editable without rebuilding) | `Material.cpp` |
| Archive / distribution build task | `.vscode/tasks.json` |

---

## Possible Next Steps

These are independent — pick any one based on interest. They are ordered within each category
from simpler to more involved.

### Rendering Quality

**A — Normal maps**  
Add a tangent-space normal map texture and a `TBN` matrix in the vertex shader. Bump the lighting
normal in the pixel shader. Visible impact: surfaces gain micro-detail without extra geometry.  
Files to change: `Mesh.h` (add tangent to vertex layout), `Material.hlsl`, texture loading in `Material.cpp`.

**B — Point lights**  
Add a small array of point lights to the per-frame CB (position, color, radius). Sum their
contribution in the pixel shader. Good for explosions — the fireball could emit light.  
Files to change: `DX12Context.cpp` (CB), `Material.hlsl`, `Game.cpp` (emit lights on explosion).

**C — Screen-space ambient occlusion (SSAO)**  
After the main pass, run a fullscreen post pass that samples the depth buffer around each pixel
and darkens areas where nearby geometry is close.  
Files to add: new PSO in `DX12Context.cpp`, new HLSL shader, new intermediate render target.

**D — Roughness / metalness material**  
Add roughness and metalness floats per entity. Use a GGX BRDF instead of the current Phong-style
lighting. Real-time PBR.  
Files to change: `Components.h` (`RenderComp`), `Material.hlsl`.

**E — Temporal anti-aliasing (TAA)**  
Replace MSAA with TAA. Accumulate jittered frames into a history buffer; each frame blend the
current frame with the accumulated result.

---

### Gameplay and Simulation

**F — Scene reset**  
Pressing R returns the world to its initial state: `world.Clear()`, then re-run `Game::Init`.  
Files to change: `Game.cpp`, `Game.h`.

**G — Explosion screen shake**  
When an explosion fires, translate `m_camera.target` by a small sinusoidal offset that decays
over ~0.3s. Pure CPU math, no GPU changes needed.  
Files to change: `Game.cpp`, `Game.h`.

**H — Sound effects**  
Use XAudio2 (available on Windows, no extra dependencies). Play impact, crackle, and rumble sounds.  
New file: `Source/Audio.cpp/h`.

**I — New component type**  
Add a `HealthComp { float hp; float maxHp; }` to `Components.h`. Add a pool to `World`.
Build a system in `Game::Update` that reduces HP when inside an explosion radius.  
This is the smallest possible ECS extension to practice the pattern.

---

### Engine Architecture

**J — External HLSL hot-reload**  
The hero entity shader already loads from disk. Extend: poll file modification time each frame,
recompile and swap the PSO if changed. Iterate on shaders while the app runs.  
Files to change: `Material.cpp` (add mtime tracking and hot-swap logic).

**K — System functions**  
Extract the burning, physics, and particle systems from `Game::Update` into standalone free functions:
```cpp
void PhysicsSystem(World& world, float dt);
void BurningSystem(World& world, float dt, std::vector<FireParticle>& particles);
```
Each takes `World&` and any external state it needs. No behaviour change, just better separation.

**L — Descriptor heap consolidation**  
Consolidate shader-visible SRVs into one large heap and use offsets. This is how large engines
manage bindless resources and is a good D3D12 architecture exercise.

**M — Frame graph**  
Replace the sequential pass blocks in `EndFrame` with a declared graph of passes, each with
explicit inputs and outputs. The graph compiler inserts barriers automatically. This is how
Frostbite FrameGraph and Unreal RDG work.

---

## Definition of Done For Any Change

- `Build Debug` succeeds with no new warnings at `/W4`.
- The feature works visually as intended.
- No regressions in existing passes (shadows, particles, bloom, distortion, tonemap).
- If any struct size or root signature changes: update `docs/shader-layout.md` to match.
- If any component is added: add a pool member to `World` and document it in `docs/architecture.md`.

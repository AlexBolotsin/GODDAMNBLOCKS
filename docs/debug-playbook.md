# Debug Playbook

## Build and Run Loop

```
Ctrl+Shift+B   Build Debug (incremental)
F5             Launch with debugger attached
```

For a clean build from scratch:  
`Tasks: Run Task → Rebuild Debug`

Check the Output panel (`Terminal` tab or `Debug Console`) for compile errors.
`$msCompile` problem matcher highlights them inline in the editor.

---

## Diagnosing Startup Failures

### Black window / immediate crash

1. Check the Debug Console for `OutputDebugString` messages — the engine logs failures at each init step.
2. The likely culprit is one of the `Create*` functions in `DX12Context::Init` returning false.
3. Run with the **D3D12 Debug Layer** enabled:
   - In `DX12Context::CreateDevice`, the debug layer is enabled when the `_DEBUG` preprocessor is defined (Debug config does this automatically).
   - Attach the debugger and watch for `D3D12 ERROR:` or `D3D12 WARNING:` in the Output window.

### `Material::Init` fails / sprites invisible

The hero entity shader lives in `Source/Shaders/Material.hlsl` and is compiled at runtime.

1. Check for `D3DCompile failed:` in the debug output — the error string shows line and column.
2. Common causes:
   - HLSL syntax error from a recent edit.
   - Local variable name collision inside a function (HLSL is strict about redeclaration).
   - `cbuffer` field added in HLSL but not in the matching C++ struct (or vice versa).
   - Root signature `Num32BitValues` not updated after adding a cbuffer field.

### Post-process pipeline fails to init

Post pipelines use inline HLSL strings in `DX12Context.cpp`.
Errors appear in the debug output as `[BrightPass/BlurH/BlurV/Tonemap] shader error:`.
The PSO creation for a later pass can also fail if a resource format is wrong —
confirm `m_hdrTarget` and bloom targets are all `R16G16B16A16_FLOAT`.

---

## Diagnosing Rendering Problems

### Sprites are blank / wrong color

- Check that the instanced sprite PSO `RTVFormats[0]` is `DXGI_FORMAT_R16G16B16A16_FLOAT`.
  It must match the MSAA target format. An `R8G8B8A8_UNORM` mismatch produces blank sprites
  (no D3D12 error on some GPUs).
- Check that `SpriteInstanceData` C++ struct stride = 112 bytes. If a field was added or removed
  without updating the three stride constants in `DX12Context.cpp`, every sprite reads the wrong
  instance slot.
- Confirm the root SRV slot for the frame: `perFrameBase + frameIndex * kMaxSpriteInstances * 112`.

### Shadows missing or wrong

- Confirm the shadow map barrier sequence: shadow map must be in `DEPTH_WRITE` during pass 1
  and in `PIXEL_SHADER_RESOURCE` before the main pass samples it.
- If shadow acne (self-shadowing noise on surfaces): increase depth bias in `m_shadowPso`.
- If shadow map is entirely white (no depth written): check that entities are inside the
  light frustum. The light is positioned well above the scene — verify `m_lightPos` and the
  projection range cover all geometry.
- Confirm SRV at slot t2 in the geometry root signature points to the shadow map, not the sprite texture.

### Particles not visible

- Confirm `world->particles` is non-empty before the draw (put a breakpoint in `RenderScene`).
- Confirm `memcpy` size: `pCount * 32` bytes (32 bytes per `SceneParticle`).
- Confirm the particle PSO blend state: `DestBlend = ONE`. If it's `ZERO`, particles overwrite
  the scene instead of adding to it.
- Confirm depth write is OFF for particles. If ON, the first particle in the draw kills all
  depth behind it.

### Bloom is too strong / too weak / missing

- Bright-pass threshold too high → no bloom (all pixels below threshold). Lower the luminance cutoff.
- Bloom blend weight in tonemap shader too high → blown-out glow. Lower the bloom scale constant.
- If bloom targets are the wrong size or format, `RenderScene` will fail silently on the resolve.
  Confirm `m_bloomWidth/m_bloomHeight` are half of the swapchain resolution.

### Fog looks wrong or doesn't match between geometry and sprites

- Geometry pass uses `fogStart=20`, `fogEnd=65` with geometry blend capped at `0.7×`.
- Instanced sprite pass uses the same fog formula — confirm the instanced shader's fog parameters
  match `Material.hlsl`.
- The fog color in the instanced shader is a sky-gradient (horizon/zenith blend) derived from
  `clipYW`. If the clip-space Y passthrough is wrong, the instanced sprites will show flat fog.

### Camera feels wrong / targeting ring jumps

- The ray cast for the targeting ring reads `mouseAbsX/Y` from `InputState`. If those are in
  screen coordinates but the NDC conversion uses the wrong screen size, the ring position drifts.
  Check `input.screenW/H` is being set from `window.GetWidth()/GetHeight()` each frame.
- After an orbit drag, the target persists. If the ring appears to teleport, confirm the forced
  `m_camera.target = vec3(0,0,-5)` reset was removed from `Game::Update` (it was removed in a
  prior session to enable the pan feature).

### Shockwave distortion not visible / always circular

- Confirm the distort pass runs after bloom-V and before tonemap in `EndFrame`.
- Confirm the tonemap SRV table points at `distortTarget` (heap slot 3), not `m_hdrTarget` (slot 0).
- If the ring is circular regardless of camera angle: the old screen-space code is still active.
  The current shader must use ray-plane intersection (`rayDir.y < -0.001f` guard).
- If the ring is in the wrong position: confirm `cameraBack = (vm[2], vm[6], vm[10])` — the third
  column of the view matrix, not the third row.
- If the ring is present but appears sky-side instead of ground-side: `rayDir.y` sign is inverted —
  check the ndc Y flip (`1 - uv.y * 2`, not `uv.y * 2 - 1`).

### Performance: FPS drops when many meteors or particles are live

- The particle system is CPU-side: `m_fireParticles` grows without bound unless particles are
  aged out. Confirm `particle.age >= particle.maxAge` triggers removal.
- The burning system iterates `world.burning` pool directly. If the pool grows unexpectedly,
  confirm `world.burning.Remove(id)` is called at burnout and `world.physics.Remove(id)` follows.
  Leaking `BurningComp` entries keeps burning logic running on invisible entities.

---

## Systematic Investigation Strategy

1. **Change one thing at a time.** If a fix involves touching two systems, make the change in one and verify before touching the second.
2. **Use the debug camera.** Disable cinematic mode (Space) so the camera is stationary during a debugging session.
3. **Bisect by pool.** Set `rnd.visible = false` on a whole group by iterating a tracking list (`m_cubeActors`, etc.) to isolate which system produces a visual artifact.
4. **Validate CPU data before GPU upload.** If particles look wrong, print the first few `SceneParticle` values before the `memcpy`. GPU corruption is rare; CPU logic errors are common.
5. **Check barriers first on GPU validation errors.** A `D3D12 ERROR: invalid state` almost always means a missing `ResourceBarrier` call.
6. **Keep notes.** Write what you changed, what happened, and what you tried in `STUDY_GUIDE_PROGRESS.md`. Short-loop debugging without notes leads to re-trying the same things.

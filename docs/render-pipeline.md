# Render Pipeline Walkthrough

## Frame Lifecycle

### 1) BeginFrame
- Transition back buffer: PRESENT -> RENDER_TARGET
- Bind current RTV and DSV
- Clear color and depth
- Set viewport and scissor
- Set primitive topology

### 2) Build Frame Camera Data
In DX12Context::RenderScene:
- Build RH view matrix from eye/target
- Build RH perspective matrix
- Pack camera position into constants

### 3) Main Geometry Pass
For each entity:
- Set PSO and root signature
- Upload root constants b0 (frame: view/proj/camera)
- Upload root constants b1 (object: world/tint/renderParams)
- Draw mesh

### 4) Projected Shadow Pass
- Build directional projection matrix onto the ground plane
- Redraw non-ground entities with:
  - world override (projected matrix)
  - dark translucent tint
  - renderParams.x set to shadow mode
- Layer a few slightly offset passes for softer look

### 5) EndFrame
- Transition back buffer: RENDER_TARGET -> PRESENT
- Close and execute command list
- Present swap chain
- Move to next frame and sync via fence values

## GPU State Highlights
- Back-face culling enabled
- FrontCounterClockwise = TRUE
- Depth test enabled (LESS)
- Blend enabled for translucent shadow pass compatibility

## Stability Notes
- Shadows must not be coplanar with floor (z-fighting)
- Slight Y offsets between shadow layers improve temporal stability
- Keep matrix order consistent when composing projected transforms

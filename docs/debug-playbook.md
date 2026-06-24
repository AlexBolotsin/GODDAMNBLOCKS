# Debug Playbook

## Build and Run Loop
Use this baseline cycle for all changes:
1. Build: cmake --build ./build --config Debug
2. Run: ./build/Debug/MazeGame.exe
3. Observe frame output and motion behavior
4. Repeat with one focused change per iteration

## If Material Init Fails
Likely source: runtime shader compile or PSO creation failure.

Checks:
1. Inspect D3DCompile error text (OutputDebugString path in Material.cpp).
2. Verify HLSL syntax and unique local variable names.
3. Verify root constant counts and cbuffer fields still match C++ uploads.
4. Confirm PSO RTV/DSV formats match swap chain/depth formats.

## If Scene Looks Wrong

### Geometry not visible
- Validate camera matrix and projection matrix terms
- Validate world transforms and winding
- Check culling front-face convention

### Shadows flicker
- Avoid coplanar projected geometry
- Add slight plane/layer separation
- Keep depth testing stable and predictable

### Colors look blown out
- Reduce additive lighting terms
- Clamp/saturate final color carefully
- Consider proper linear + tone map workflow as next upgrade

## If Crash Happens In Draw
1. Ensure material, mesh, PSO, root signature are non-null.
2. Validate command allocator/list reset and close order.
3. Check resource states before and after render pass.
4. Rebuild and bisect recent edits.

## Investigation Strategy
- Change one thing at a time.
- Prefer deterministic camera and animation when debugging.
- Keep notes for each experiment: what changed, what effect observed.

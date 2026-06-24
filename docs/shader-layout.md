# Shader and Root Constant Layout

## Root Constants Contract
Current contract in Material root signature:
- Slot 0 (b0): frame constants
  - float4x4 viewMatrix
  - float4x4 projMatrix
  - float4 cameraPosition

- Slot 1 (b1): per-draw constants
  - float4x4 worldMatrix
  - float4 tintColor
  - float4 renderParams

## Important Rule
The C++ struct layout and HLSL cbuffer layout must match exactly.
If one side changes and the other does not, rendering corrupts or fails.

## Current Shader Stages

### VS (VSMain)
- Transforms object position -> world -> view -> clip
- Passes world position and world normal to PS
- Passes clip y/w for sky-aware fog tinting

### PS (PSMain)
- Branches by renderParams.x:
  - > 0.5: shadow mode output (dark alpha, fog-aware)
  - else: normal lighting mode

Normal mode includes:
- Ambient + key + fill directional lighting
- Floor-only checker/grid and normal perturbation
- Edge darkening based on NdotV
- Distance fog blending with horizon/zenith color gradient

## Common Error Patterns
1. Changed Num32BitValues but not the struct size upload
2. Changed cbuffer member order in HLSL only
3. Duplicate local symbol names inside shader function
4. Register index mismatch (b0 vs b1 confusion)

## Quick Validation Checklist
1. Root signature constants count equals uploaded 32-bit values.
2. C++ structs are tightly compatible with HLSL field order.
3. Shader compile errors are surfaced from D3DCompile output.
4. Draw call always sets PSO and root signature before constants.

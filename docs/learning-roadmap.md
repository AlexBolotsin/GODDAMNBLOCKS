# Learning Roadmap (Next 4 Stages)

## Stage 1: Color Pipeline
Goal: make lighting look physically plausible and consistent.
- Add linear workflow and gamma-correct output
- Add tone mapping (filmic/ACES)
- Study impact of clamping and exposure

## Stage 2: True Shadow Mapping
Goal: replace projected shadows with light-space depth test.
- Add depth-only shadow pass
- Sample shadow map in main pass
- Add PCF filtering and bias tuning

## Stage 3: Material Expansion
Goal: richer surface quality.
- Externalize shaders into .hlsl files
- Add roughness and normal map inputs
- Separate per-material and per-object properties

## Stage 4: Post and Stability
Goal: reduce shimmer and improve final image quality.
- Add FXAA as first pass
- Study TAA fundamentals
- Add optional bloom and vignette carefully

## Suggested Study Cadence
1. Read docs/architecture.md
2. Read docs/render-pipeline.md
3. Read docs/shader-layout.md
4. Make one small renderer upgrade
5. Validate with debug-playbook checklist

## Definition of Done For Each Stage
- Build passes in Debug
- Runtime is stable (no init failures)
- Visual before/after is documented with notes
- No regressions in camera, depth, or shadows

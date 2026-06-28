# ProtoStudyGameEngine — Docs Index

This is a small Direct3D 12 forward renderer built for learning.
Read the docs in order below. Each one builds on the previous.

---

## Read Order

| # | File | What you will learn |
|---|---|---|
| 1 | [architecture.md](architecture.md) | What every system is and how they connect, including the ECS |
| 2 | [render-pipeline.md](render-pipeline.md) | Every GPU pass in frame order, with why |
| 3 | [shader-layout.md](shader-layout.md) | All root signatures, constant buffers, and register slots |
| 4 | [debug-playbook.md](debug-playbook.md) | How to diagnose and fix the common failures |
| 5 | [learning-roadmap.md](learning-roadmap.md) | What was built, what is possible next |
| 6 | [implementation-log.md](implementation-log.md) | Chronological record of decisions and fixes |

See `STUDY_GUIDE_PROGRESS.md` in the project root for the full engine snapshot and study checklist.
See `CONTROLS.txt` in the project root for all keyboard and mouse bindings.

---

## Suggested Workflow

1. Read one doc end to end.
2. Open the source files it references and read the relevant section.
3. Make one small change to that system.
4. Build with `Ctrl+Shift+B` (Build Debug) and run with `F5`.
5. Observe what changed and take a note in `STUDY_GUIDE_PROGRESS.md`.

---

## Current Engine Feature Summary

- Double-buffered D3D12 swap chain with 8× MSAA
- Real-time shadow map (2048×2048 depth-only pass)
- Forward-lit geometry with fog and sky-gradient blending
- GPU-instanced billboard sprites (up to 5 000, hover animation on GPU)
- Additive billboard particle system (up to 8 000 particles)
- HDR rendering with bloom (bright-pass → separable Gaussian blur)
- Screen-space air distortion: world-space shockwave rings computed via ray–ground-plane intersection
- Tone mapping with optional scanline and Bayer-dither post effects
- ECS: EntityID + ComponentPool\<T\> (sparse-set) with typed pools in World
- Game logic: meteor casting, explosion VFX, fire trails, burning sprites with ragdoll
- Orbit / pan / zoom camera, targeting ring, cinematic auto-orbit mode

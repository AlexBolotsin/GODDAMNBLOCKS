# Sprite Assets

## Current Sheet
- Source image: 19338.png
- Background color is chroma-key green, not alpha
- Footer text is present at the bottom and should be excluded from frame extraction

## Prep Workflow
Use the atlas extraction script to convert the raw sheet into frame metadata:

```powershell
./Tools/Build-SpriteAtlas.ps1 -InputPath ./Sprites/19338.png -OutputPath ./Sprites/19338.atlas.json
```

## What The Atlas Contains
- Chroma-key color
- Content bounds excluding footer region
- Extracted frame rectangles
- Row-grouped clips for animation lookup

## Renderer Notes
Your current renderer does not sample textures yet. This atlas is the prep step for the next upgrade:
1. Add UVs to the sprite mesh
2. Load the texture into a shader resource
3. Sample the texture in the pixel shader
4. Use the atlas frame rectangles to animate billboard actors

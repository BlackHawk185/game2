# Textures Directory

Place your texture files here:

## Required Textures:
- **grass.png** - Main grass block texture (16x16 or 32x32 recommended for pixel art style)

## Texture Guidelines:
- Use PNG format for transparency support
- Keep power-of-2 dimensions (16x16, 32x32, 64x64, etc.)
- For pixel art style, use nearest neighbor filtering
- The engine will automatically load textures from this directory

## Current Implementation:
The engine looks for `grass.png` in this directory. If not found, it will fall back to a generated procedural grass texture.

To add your AI-generated grass texture:
1. Generate a grass texture using your AI model
2. Save it as `grass.png` in this directory
3. Restart the engine to see your new texture!

# Assets Directory

This directory contains all game assets organized by type for the MMORPG Engine.

## Directory Structure

### 📁 models/
3D model files (GLB/GLTF) for instanced rendering:
- Used for detailed objects that are GPU instanced rather than voxelized
- Examples: trees, lamps, rocks, furniture, decorations
- Current: `grass.glb` - Grass model for terrain decoration

### 📁 textures/
Texture files (PNG, JPG, etc.) for voxel blocks and surfaces:
- **grass.png** - Main grass block texture *(required)*
- **dirt.png** - Dirt block texture  
- **stone.png** - Stone block texture
- Additional block textures as needed

## Asset Guidelines

### Textures
- **Format**: PNG (for transparency support) or JPG
- **Dimensions**: Power-of-2 sizes (16x16, 32x32, 64x64 recommended)
- **Style**: Pixel art style with nearest neighbor filtering
- **Fallbacks**: Engine generates procedural textures if files missing

### Models  
- **Format**: GLB/GLTF preferred (OBJ supported)
- **Scale**: Consider voxel grid scale (1 unit = 1 voxel)
- **Optimization**: Keep poly count reasonable for instanced rendering

## Asset Loading

- **Models**: Loaded by GLBLoader for BlockType::OBJ blocks
- **Textures**: Loaded by TextureManager for voxel rendering
- **Paths**: All paths relative to project root or build directory
- **Hot-reload**: Assets can be updated without engine restart

## Current Implementation

The engine automatically scans this directory structure and loads assets as needed. Missing assets fall back to procedural generation where possible.

## File Naming Convention

- Use lowercase names with underscores for spaces
- Include descriptive names: `tree_oak.obj`, `lamp_street.obj`
- Keep texture names simple: `dirt.png`, `grass.png`

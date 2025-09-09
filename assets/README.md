# Assets Directory

This directory contains all game assets organized by type.

## Directory Structure

- **models/** - 3D model files (OBJ, etc.) for instanced rendering
  - Used for detailed objects that will be GPU instanced rather than voxelized
  - Examples: trees, lamps, rocks, furniture, decorations

- **textures/** - Texture files (PNG, JPG, etc.)
  - Used for voxel blocks and other rendered surfaces
  - Examples: dirt.png, grass.png, stone.png

## Asset Loading

- Models are loaded by the BlockType system for OBJ-type blocks
- Textures are loaded by the texture manager for voxel rendering
- All paths are relative to the project root or build directory

## File Naming Convention

- Use lowercase names with underscores for spaces
- Include descriptive names: `tree_oak.obj`, `lamp_street.obj`
- Keep texture names simple: `dirt.png`, `grass.png`

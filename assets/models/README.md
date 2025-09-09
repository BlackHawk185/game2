# 3D Models

This directory contains 3D model files for GPU instanced rendering.

## Current Models

- **tree.obj** - Simple tree model for natural decoration
- **lamp.obj** - Lamp post model for lighting/decoration  
- **rock.obj** - Rock model for natural terrain features

## Model Requirements

- OBJ format with vertices and faces
- Keep models relatively low-poly for performance
- Models should be roughly 1-2 voxel units in size
- Use simple geometry for placeholder models

## Adding New Models

1. Create or export OBJ file
2. Place in this directory
3. Register in BlockType.cpp with `BlockRenderType::OBJ`
4. Update island generation or block placement logic as needed

## Future Enhancements

- Material/texture support
- Animation support
- LOD (Level of Detail) models
- Batch loading system

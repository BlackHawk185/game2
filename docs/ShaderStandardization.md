# Shader Standardization Specification
# MMORPG Engine - September 2025

## Core Principles
1. **Consistent Naming**: All uniforms use `u` prefix (e.g., `uModel`, `uView`)
2. **UBO First**: Use Uniform Buffer Objects for common data (camera, lighting)
3. **Material System**: Support different material types (voxel, fluid, UI)
4. **Performance**: Minimize uniform updates per draw call

## Standard Uniform Layout

### Universal UBOs (binding points 0-2)
```glsl
// Binding 0: Camera Data (144 bytes)
layout (std140, binding = 0) uniform CameraData {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec3 uCameraPosition;
    float uNearPlane;
    vec3 uCameraDirection;
    float uFarPlane;
};

// Binding 1: Lighting Data (96 bytes)
layout (std140, binding = 1) uniform LightingData {
    vec3 uSunDirection;
    float uSunIntensity;
    vec3 uSunColor;
    float uAmbientIntensity;
    vec3 uAmbientColor;
    float uTimeOfDay;           // 0.0-1.0
    vec2 uShadowMapSize;
    float uShadowBias;
    float _padding1;
};

// Binding 2: Material Data (64 bytes) - Per-material type
layout (std140, binding = 2) uniform MaterialData {
    vec4 uDiffuseColor;         // .a = transparency
    vec3 uSpecularColor;
    float uRoughness;
    vec3 uEmissiveColor;
    float uMetallic;
    // Material type specific data...
};
```

### Per-Object Uniforms (when UBO not suitable)
```glsl
uniform mat4 uModel;            // Transform matrix
uniform int uMaterialID;        // Material lookup
uniform float uAnimationTime;   // For animated materials
```

### Texture Bindings (consistent slots)
```glsl
// Slot 0-7: Material textures
uniform sampler2D uDiffuseTexture;      // Slot 0
uniform sampler2D uNormalTexture;       // Slot 1
uniform sampler2D uSpecularTexture;     // Slot 2
uniform sampler2D uEmissiveTexture;     // Slot 3
// Slots 4-7 reserved for material-specific

// Slot 8-15: Lighting textures
uniform sampler2D uShadowMap;           // Slot 8
uniform sampler2D uLightMapFace0;       // Slot 9 (+X face)
uniform sampler2D uLightMapFace1;       // Slot 10 (-X face)
uniform sampler2D uLightMapFace2;       // Slot 11 (+Y face)
uniform sampler2D uLightMapFace3;       // Slot 12 (-Y face)
uniform sampler2D uLightMapFace4;       // Slot 13 (+Z face)
uniform sampler2D uLightMapFace5;       // Slot 14 (-Z face)
uniform float uLightMapStrength;        // Control lightmap influence
```

## Material Types

### Voxel Material
- Uses lightmaps for face-based lighting
- Supports texture atlasing
- Per-chunk transform optimization

### Fluid Material  
- Supports transparency and blending
- Color tinting and flow animation
- Particle-based rendering

### UI Material
- Screen-space coordinates
- No lighting calculations
- Text and widget support

## Implementation Plan

1. **Phase 1**: Update SimpleShader to use standard naming
2. **Phase 2**: Create UBO-based camera and lighting system  
3. **Phase 3**: Implement material system for different object types
4. **Phase 4**: Migrate fluid particles to proper material system
5. **Phase 5**: Consolidate external shaders

## Benefits
- Consistent API across all rendering code
- Better performance through UBOs
- Easier debugging and maintenance
- Support for future features (PBR, etc.)
- Clear separation of concerns

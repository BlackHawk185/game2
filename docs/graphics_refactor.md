Graphics Engine Refactor (2025-08-31)

Summary

- Unified OpenGL usage across the rendering code.
- Removed hardcoded absolute texture path and added project‑relative lookup.
- Fixed shader sampler uniform bug (uses glUniform1i now).
- Simplified the legacy Renderer facade to avoid early GL calls.

Key Changes

- engine/Rendering/Renderer.cpp: No longer calls GL prior to GLAD init; clear() remains.
- engine/Rendering/TextureManager.[h/cpp]: Uses GLAD headers, sensible internal formats, optional mipmaps.
- engine/Rendering/SimpleShader.[h/cpp]: Header no longer includes GL headers; cpp includes GLAD; setInt uses glUniform1i.
- engine/Rendering/VBORenderer.[h/cpp]: Header no longer pulls GL headers; cpp includes GLAD; texture path discovered via common project‑relative folders.

Notes

- VBORenderer is the primary rendering path (IslandChunkSystem uses it exclusively).
- Legacy immediate‑mode rendering is still present for compatibility but isolated.
- If you relocate assets, keep textures under a textures/ folder next to the executable or project root.

Next Steps (Optional)

- Introduce a RenderSystem facade that owns VBORenderer and TextureManager.
- Implement a simple ShaderManager to share compiled programs.
- VoxelRenderer removed; legacy immediate‑mode path in VoxelChunk replaced by VBORenderer.

# OpenGL Version Standards

## 🚨 CRITICAL: Use OpenGL 4.6 Core Profile ONLY

**For AI Assistants and Developers:**

### ✅ CORRECT OpenGL Version
- **OpenGL 4.6 Core Profile**
- Context version: `4.6`
- Profile: `GLFW_OPENGL_CORE_PROFILE`

### ❌ DO NOT USE
- OpenGL 3.3 (outdated)
- OpenGL 3.0/3.1/3.2 (legacy)
- Compatibility profile (deprecated features)

### GLFW Configuration
```cpp
// ALWAYS use this configuration:
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

#ifdef _DEBUG
glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
```

### OpenGL 4.6 Features We Can Use
- **Direct State Access (DSA)** - More efficient state management
- **Compute Shaders** - GPU compute workloads
- **Shader Storage Buffer Objects (SSBO)** - Large data storage
- **Multi-draw indirect** - Efficient batch rendering
- **Enhanced debugging** - Better error reporting
- **Bindless textures** - Reduced state changes

### Shader Versions
- **Vertex/Fragment Shaders**: `#version 460 core`
- **Compute Shaders**: `#version 460 core`
- **Geometry/Tessellation**: `#version 460 core`

### Why OpenGL 4.6?
1. **Modern Features**: Access to latest GPU capabilities
2. **Better Performance**: More efficient rendering paths
3. **Future Proof**: Current industry standard
4. **Better Debugging**: Enhanced debug output
5. **Direct State Access**: Cleaner, more efficient code

### Hardware Support
- **NVIDIA**: GTX 400 series and newer (2010+)
- **AMD**: HD 7000 series and newer (2012+)
- **Intel**: HD Graphics 500 series and newer (2016+)

### Current Engine Status
- ✅ Window system configured for OpenGL 4.6
- ✅ GLAD OpenGL loader supports 4.6
- 🔄 Shaders need updating to `#version 460 core`
- 🔄 VoxelRenderer can use DSA features

**REMEMBER: Always check this file before implementing OpenGL features!**

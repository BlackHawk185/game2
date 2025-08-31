#version 330 core

// Input from vertex shader
in vec3 FragPos;    // Fragment position in world space
in vec3 Normal;     // Normal vector
in vec2 TexCoord;   // Texture coordinates

// Uniforms
uniform sampler2D uTexture;     // Texture sampler
uniform vec3 uLightPos;         // Light position
uniform vec3 uLightColor;       // Light color
uniform vec3 uViewPos;          // Camera position
uniform float uAmbientStrength; // Ambient light strength
uniform float uExposure;        // Exposure control for tone mapping

// Output color
out vec4 FragColor;

// ACES tone mapping function - makes colors look cinematic
vec3 acesToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    // Sample texture
    vec4 texColor = texture(uTexture, TexCoord);
    
    // Basic Phong lighting calculation
    
    // Ambient
    vec3 ambient = uAmbientStrength * uLightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // Specular (simple)
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = 0.5 * spec * uLightColor;
    
    // Combine lighting with texture
    vec3 result = (ambient + diffuse + specular) * texColor.rgb;
    
    // Apply exposure control
    result *= uExposure;
    
    // Apply ACES tone mapping for cinematic look
    result = acesToneMapping(result);
    
    // Apply gamma correction (convert from linear to sRGB)
    result = pow(result, vec3(1.0/2.2));
    
    // Output final color
    FragColor = vec4(result, texColor.a);
}

#version 430

in vec3 vNormal;
in vec3 vPosition;
in vec2 vTexCoord;

uniform vec3 uLightDir;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform vec3 uMaterialColor;
uniform bool uUseMaterialColor;

out vec4 fColor;

void main()
{
    // Normalize the normal (interpolation can make it non-unit)
    vec3 n = normalize(vNormal);
    
    // Calculate diffuse component (n dot l)
    float ndotl = max(dot(n, uLightDir), 0.0);
    
    // Get base color from texture, material color, or white
    vec3 baseColor = vec3(1.0);
    if (uUseTexture)
    {
        baseColor = texture(uTexture, vTexCoord).rgb;
    }
    else if (uUseMaterialColor)
    {
        baseColor = uMaterialColor;
    }
    
    // Combine ambient and diffuse with texture/material
    vec3 color = baseColor * (uAmbientColor + uDiffuseColor * ndotl);
    
    fColor = vec4(color, 1.0);
}

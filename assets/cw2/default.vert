#version 430

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelViewProjection;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vPosition;
out vec2 vTexCoord;

void main()
{
    vPosition = (uModel * vec4(aPosition, 1.0)).xyz;
    vNormal = normalize((uModel * vec4(aNormal, 0.0)).xyz);
    vTexCoord = aTexCoord;
    gl_Position = uModelViewProjection * vec4(aPosition, 1.0);
}

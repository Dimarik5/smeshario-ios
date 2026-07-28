#version 300 es
in vec2 position;

uniform mat3 projectionMatrix;
uniform mat3 transformMatrix;
uniform mat3 objectMatrix;

void main()
{
    vec3 pos = vec3(position, 1.0);
    gl_Position = vec4(projectionMatrix * transformMatrix * objectMatrix * pos, 1.0);
}

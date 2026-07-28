#version 300 es
in vec2 position;

uniform mat3 projectionMatrix;
uniform mat3 transformMatrix;
uniform mat3 objectMatrix;
uniform mat3 textureMatrix;

out vec2 textureCoordinate;
out vec2 pPos;

void main()
{
    vec3 pos = vec3(position, 1);
    pPos = (objectMatrix * pos).xy;
    textureCoordinate = (textureMatrix * pos).xy;
    gl_Position = vec4(projectionMatrix * transformMatrix * objectMatrix * pos, 1);
}

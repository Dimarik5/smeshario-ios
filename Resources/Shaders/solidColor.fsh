#version 300 es
#ifdef GL_ES
 precision highp float;
#endif
uniform lowp vec4 color;

out vec4 fragColor;

void main()
{
    fragColor = color; //gl_FragColor
}

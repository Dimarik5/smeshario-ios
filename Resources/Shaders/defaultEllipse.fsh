#version 300 es
#ifdef GL_ES
 precision highp float;
#endif
in vec2 textureCoordinate;
in vec2 pPos;

uniform sampler2D imgTexture;
uniform lowp int inkEffect;
uniform lowp vec4 blendColor;

uniform highp vec2 centerpos;
uniform highp vec2 radius;

out vec4 fragColor;

void main()
{
    vec4 color = texture(imgTexture, textureCoordinate) * blendColor;

    if(inkEffect == 2)            //INVERT
        color.rgb = vec3(1.0,1.0,1.0)-color.rgb;
    else if(inkEffect == 10)    //MONO
    {
        float mono = 0.3125*color.r + 0.5625*color.g + 0.125*color.b;
        color.rgb = vec3(mono,mono,mono);
    }
    
    highp vec2 eC = vec2(pPos.x-centerpos.x, pPos.y-centerpos.y);
    highp float ellipseFactor = (eC.x*eC.x)/radius.x + (eC.y*eC.y)/radius.y;
    if(ellipseFactor >= 1.0)
        color.a = 0.0;
    
    fragColor = color; //gl_FragColor
}

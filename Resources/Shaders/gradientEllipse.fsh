#version 300 es
#ifdef GL_ES
 precision highp float;
#endif
in vec4 vColor;
in vec2 pPos;

uniform lowp int inkEffect;
uniform lowp vec4 blendColor;

uniform highp vec2 centerpos;
uniform highp vec2 radius;

out vec4 fragColor;

void main()
{
    lowp vec4 nColor = vColor * blendColor;

    if(inkEffect == 2)            //INVERT
        nColor.rgb = vec3(1.0,1.0,1.0)-nColor.rgb;
    else if(inkEffect == 10)    //MONO
    {
        float mono = 0.3125*nColor.r + 0.5625*nColor.g + 0.125*nColor.b;
        nColor.rgb = vec3(mono,mono,mono);
    }
    highp vec2 eC = vec2(pPos.x-centerpos.x, pPos.y-centerpos.y);
    highp float ellipseFactor = (eC.x*eC.x)/radius.x + (eC.y*eC.y)/radius.y;
    if(ellipseFactor >= 1.0)
        nColor.a = 0.0;
    
    fragColor = nColor; //gl_FragColor
}


